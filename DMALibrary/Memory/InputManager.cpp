#include "pch.h"
#include "InputManager.h"
#include "InputManagerAddressResolver.h"
#include "Registry.h"
#include "Memory/Memory.h"

//TODO: Restart winlogon.exe when it doesn't exist.
bool c_keys::InitKeyboard()
{
	keyboardDmaAvailable = false;
	gafAsyncKeyStateExport = 0;
	win_logon_pid = 0;
	lastInitFailure = dma_keyboard::KeyboardInitFailure::None;

	std::string win = registry.QueryValue("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\CurrentBuild", e_registry_type::sz);
	int Winver = 0;
	if (!win.empty())
		Winver = std::stoi(win);
	else
	{
		lastInitFailure = dma_keyboard::KeyboardInitFailure::MissingBuildNumber;
		return false;
	}

	std::string ubr = registry.QueryValue("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\UBR", e_registry_type::dword);
	int Ubr = 0;
	if (!ubr.empty())
		Ubr = std::stoi(ubr);
	else
	{
		lastInitFailure = dma_keyboard::KeyboardInitFailure::MissingUbr;
		return false;
	}

	this->win_logon_pid = mem.GetPidFromName("winlogon.exe");
	if (this->win_logon_pid == 0)
	{
		lastInitFailure = dma_keyboard::KeyboardInitFailure::WinlogonNotFound;
		return false;
	}
	if (Winver > 22000)
	{
		auto pids = mem.GetPidListFromName("csrss.exe");
		for (size_t i = 0; i < pids.size(); i++)
		{
			auto pid = pids[i];
			uintptr_t tmp = VMMDLL_ProcessGetModuleBaseU(mem.vHandle, pid, const_cast<LPSTR>("win32ksgd.sys"));
			uintptr_t g_session_global_slots = tmp + 0x3110;
			uintptr_t user_session_state = 0;
			for (int i = 0; i < 4; i++)
			{
				user_session_state = mem.Read<uintptr_t>(mem.Read<uintptr_t>(mem.Read<uintptr_t>(g_session_global_slots, pid) + 8 * i, pid), pid);
				if (user_session_state > 0x7FFFFFFFFFFF)
					break;
			}

			if (Winver >= 22631 && Ubr >= 3810)
				gafAsyncKeyStateExport = user_session_state + 0x36A8;
			else
				gafAsyncKeyStateExport = user_session_state + 0x3690;
			if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
				break;
		}
		if (gafAsyncKeyStateExport > 0x7FFFFFFFFFFF)
		{
			keyboardDmaAvailable = true;
			lastInitFailure = dma_keyboard::KeyboardInitFailure::None;
			return true;
		}
		lastInitFailure = dma_keyboard::KeyboardInitFailure::AddressUnresolved;
		return false;
	}
	else
	{
		PVMMDLL_MAP_EAT eat_map = NULL;
		PVMMDLL_MAP_EATENTRY eat_map_entry;
		const DWORD keyboard_pid = mem.GetPidFromName("winlogon.exe") | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY;
		const bool eat_lookup_succeeded = VMMDLL_Map_GetEATU(mem.vHandle, keyboard_pid, const_cast<LPSTR>("win32kbase.sys"), &eat_map);
		if (eat_lookup_succeeded && eat_map->dwVersion == VMMDLL_MAP_EAT_VERSION)
		{
			for (int i = 0; i < eat_map->cMap; i++)
			{
				eat_map_entry = eat_map->pMap + i;
				if (strcmp(eat_map_entry->uszFunction, "gafAsyncKeyState") == 0)
				{
					gafAsyncKeyStateExport = eat_map_entry->vaFunction;
					break;
				}
			}
		}
		else if (eat_lookup_succeeded && eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION)
		{
			VMMDLL_MemFree(eat_map);
			eat_map = NULL;
			lastInitFailure = dma_keyboard::KeyboardInitFailure::EatVersionMismatch;
			LOG("keyboard init: unexpected EAT map version, falling back to PDB\n");
		}
		else
		{
			lastInitFailure = dma_keyboard::KeyboardInitFailure::EatLookupFailed;
			LOG("keyboard init: EAT lookup failed, falling back to PDB\n");
		}

		if (eat_map != NULL)
		{
			VMMDLL_MemFree(eat_map);
			eat_map = NULL;
		}
		if (dma_keyboard::ShouldTryLegacyPdbLookup(eat_lookup_succeeded, gafAsyncKeyStateExport))
		{
			PVMMDLL_MAP_MODULEENTRY module_info;
			auto result = VMMDLL_Map_GetModuleFromNameW(mem.vHandle, keyboard_pid, static_cast<LPCWSTR>(L"win32kbase.sys"), &module_info, VMMDLL_MODULE_FLAG_NORMAL);
			if (!result)
			{
				lastInitFailure = dma_keyboard::KeyboardInitFailure::ModuleInfoLookupFailed;
				LOG("failed to get module info\n");
				return false;
			}

			char str[32];
			if (!VMMDLL_PdbLoad(mem.vHandle, keyboard_pid, module_info->vaBase, str))
			{
				lastInitFailure = dma_keyboard::KeyboardInitFailure::PdbLoadFailed;
				LOG("failed to load pdb\n");
				return false;
			}

			uintptr_t gafAsyncKeyState;
			if (!VMMDLL_PdbSymbolAddress(mem.vHandle, str, const_cast<LPSTR>("gafAsyncKeyState"), &gafAsyncKeyState))
			{
				lastInitFailure = dma_keyboard::KeyboardInitFailure::PdbSymbolLookupFailed;
				LOG("failed to find gafAsyncKeyState\n");
				return false;
			}

			gafAsyncKeyStateExport = dma_keyboard::ResolveLegacyGafAsyncKeyStateAddress(gafAsyncKeyStateExport, gafAsyncKeyState);
			LOG("found gafAsyncKeyState at: 0x%p\n", gafAsyncKeyState);
		}
		if (dma_keyboard::IsResolvedKernelAddress(gafAsyncKeyStateExport))
		{
			keyboardDmaAvailable = true;
			lastInitFailure = dma_keyboard::KeyboardInitFailure::None;
			return true;
		}
		lastInitFailure = dma_keyboard::KeyboardInitFailure::AddressUnresolved;
		return false;
	}
}

void c_keys::UpdateKeys()
{
	uint8_t previous_key_state_bitmap[64] = {0};
	memcpy(previous_key_state_bitmap, state_bitmap, 64);

	VMMDLL_MemReadEx(mem.vHandle, this->win_logon_pid | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, gafAsyncKeyStateExport, reinterpret_cast<PBYTE>(&state_bitmap), 64, NULL, VMMDLL_FLAG_NOCACHE);
	for (int vk = 0; vk < 256; ++vk)
		if ((state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2) && !(previous_key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2))
			previous_state_bitmap[vk / 8] |= 1 << vk % 8;
}

bool c_keys::IsKeyDown(uint32_t virtual_key_code)
{
	if (!keyboardDmaAvailable || gafAsyncKeyStateExport < 0x7FFFFFFFFFFF)
		return (GetAsyncKeyState(static_cast<int>(virtual_key_code)) & 0x8000) != 0;
	if (std::chrono::system_clock::now() - start > std::chrono::milliseconds(1))
	{
		UpdateKeys();
		start = std::chrono::system_clock::now();
	}
	return state_bitmap[(virtual_key_code * 2 / 8)] & 1 << virtual_key_code % 4 * 2;
}
