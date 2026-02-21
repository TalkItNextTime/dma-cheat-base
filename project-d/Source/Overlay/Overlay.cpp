#include <Pch.hpp>
#include <SDK.hpp>
#include <ESP/ESP.hpp>
#include <array>

#include "Overlay.hpp"
#include "Fonts/IBMPlexMono_Medium.h"
#include "Localization.hpp"

ID3D11Device* Overlay::device = nullptr;

ID3D11DeviceContext* Overlay::device_context = nullptr;

IDXGISwapChain* Overlay::swap_chain = nullptr;

ID3D11RenderTargetView* Overlay::render_targetview = nullptr;

HWND Overlay::overlay = nullptr;
WNDCLASSEX Overlay::wc = { };

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	const char* L(const char* text)
	{
		return Localization::Localize(text);
	}

	void SyncLanguageFromConfig()
	{
		config.Language = std::clamp(config.Language, 0, 1);
		Localization::CurrentLanguage = static_cast<Localization::Language>(config.Language);
	}
}

LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;

	case WM_DESTROY:
		Overlay::DestroyDevice();
		Overlay::DestroyOverlay();
		Overlay::DestroyImGui();
		PostQuitMessage(0);
		return 0;

	case WM_CLOSE:
		Overlay::DestroyDevice();
		Overlay::DestroyOverlay();
		Overlay::DestroyImGui();
		return 0;
	}

	return DefWindowProc(window, msg, wParam, lParam);
}

bool Overlay::CreateDevice()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));

	sd.BufferCount = 2;

	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;

	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;

	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	sd.OutputWindow = overlay;

	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;

	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&featureLevel,
		&device_context);

	if (result == DXGI_ERROR_UNSUPPORTED) {
		result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0U,
			featureLevelArray,
			2, D3D11_SDK_VERSION,
			&sd,
			&swap_chain,
			&device,
			&featureLevel,
			&device_context);

		LOG_ERROR("Created with D3D_DRIVER_TYPE_WARP");
	}

	if (result != S_OK) {
		LOG_ERROR("Device not supported");
		return false;
	}

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer)
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_targetview);
		back_buffer->Release();
		return true;
	}

	LOG_ERROR("Failed to create device");
	return false;
}

void Overlay::DestroyDevice()
{
	if (device)
	{
		device->Release();
		device_context->Release();
		swap_chain->Release();
		render_targetview->Release();
	}
	else
		LOG_ERROR("Device not found when exiting");
}

bool Overlay::CreateOverlay()
{
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = GetModuleHandleA(0);
	wc.lpszClassName = L"Awhare";

	RegisterClassEx(&wc);

	overlay = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		wc.lpszClassName,
		L"Awhare",
		WS_POPUP,
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		NULL,
		NULL,
		wc.hInstance,
		NULL
	);

	if (overlay == NULL)
	{
		LOG_ERROR("Failed to create overlay");
		return false;
	}

	SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	{
		RECT client_area{};
		RECT window_area{};

		GetClientRect(overlay, &client_area);
		GetWindowRect(overlay, &window_area);

		POINT diff{};
		ClientToScreen(overlay, &diff);

		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		DwmExtendFrameIntoClientArea(overlay, &margins);
	}

	ShowWindow(overlay, SW_SHOW);
	UpdateWindow(overlay);

	return true;
}

void Overlay::DestroyOverlay()
{
	DestroyWindow(overlay);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
}

bool Overlay::CreateImGui()
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;

	ImFont* uiFont = nullptr;
	const std::array<std::filesystem::path, 4> yaHeiCandidates = {
		std::filesystem::path("C:/Windows/Fonts/msyh.ttc"),
		std::filesystem::path("C:/Windows/Fonts/msyh.ttf"),
		std::filesystem::path("C:/Windows/Fonts/msyhbd.ttc"),
		std::filesystem::path("C:/Windows/Fonts/msyhl.ttc")
	};

	for (const auto& candidate : yaHeiCandidates)
	{
		if (!std::filesystem::exists(candidate))
			continue;

		uiFont = io.Fonts->AddFontFromFileTTF(
			candidate.string().c_str(),
			16.0f,
			nullptr,
			io.Fonts->GetGlyphRangesChineseFull()
		);

		if (uiFont)
		{
			LOG_INFO("Loaded UI font: {}", candidate.string());
			break;
		}
	}

	if (!uiFont)
	{
		const std::filesystem::path localFont = std::filesystem::current_path() / "font.otf";
		if (std::filesystem::exists(localFont))
		{
			uiFont = io.Fonts->AddFontFromFileTTF(
				localFont.string().c_str(),
				16.0f,
				nullptr,
				io.Fonts->GetGlyphRangesChineseFull()
			);
			if (uiFont)
				LOG_INFO("Loaded fallback UI font: {}", localFont.string());
		}
	}

	if (!uiFont)
	{
		uiFont = io.Fonts->AddFontFromMemoryCompressedTTF(
			IBMPlexMono_Medium_compressed_data,
			IBMPlexMono_Medium_compressed_size,
			14.0f,
			nullptr,
			io.Fonts->GetGlyphRangesDefault()
		);
		LOG_WARN("Failed to load Microsoft YaHei, using bundled fallback font.");
	}

	if (!uiFont)
		uiFont = io.Fonts->AddFontDefault();

	io.FontDefault = uiFont;

	if (!ImGui_ImplWin32_Init(overlay)) {
		LOG_ERROR("Failed ImGui_ImplWin32_Init");
		return false;
	}

	if (!ImGui_ImplDX11_Init(device, device_context)) {
		LOG_ERROR("Failed ImGui_ImplDX11_Init");
		return false;
	}

	return true;
}

void Overlay::DestroyImGui()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Overlay::SetForeground(HWND window)
{
	if (!IsWindowInForeground(window))
		BringToForeground(window);
}

bool Overlay::IsHostKeyDown(const int virtualKey)
{
	const bool localKeyDown = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;

	c_keys* keyboard = mem.GetKeyboard();
	if (!keyboard)
		return localKeyDown;

	const bool dmaKeyDown = keyboard->IsKeyDown(static_cast<uint32_t>(virtualKey));
	return localKeyDown || dmaKeyDown;
}

void Overlay::StartRender()
{
	SyncLanguageFromConfig();

	MSG msg;
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	m_FrameStart = std::chrono::steady_clock::now();
	OverlayFps = ImGui::GetIO().Framerate;

	const bool insertDown = IsHostKeyDown(VK_INSERT);
	if (insertDown && !m_InsertHeld)
	{
		shouldRenderMenu = !shouldRenderMenu;

		if (shouldRenderMenu)
		{
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW);
		}
		else
		{
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED);
		}
	}

	m_InsertHeld = insertDown;
}

void Overlay::EndRender()
{
	ImGui::Render();

	float color[4];
	if (config.Visuals.Enabled && config.Visuals.Background) // Black bg
	{
		color[0] = 0; color[1] = 0; color[2] = 0; color[3] = 1;
	}
	else // Transparent bg
	{
		color[0] = 0; color[1] = 0; color[2] = 0; color[3] = 0;
	}

	device_context->OMSetRenderTargets(1, &render_targetview, nullptr);
	device_context->ClearRenderTargetView(render_targetview, color);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	swap_chain->Present(config.Visuals.VSync ? 1U : 0U, 0U);

	if (m_FrameStart.time_since_epoch().count() != 0)
	{
		const auto frameUs = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - m_FrameStart
		).count();

		if (frameUs >= 0)
			PerfDebug::RecordOverlayFrame(static_cast<std::uint64_t>(frameUs));
	}
}

void Overlay::StyleMenu(ImGuiIO& IO, ImGuiStyle& style)
{
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Disable Ini file
	IO.IniFilename = nullptr;

    // Custom styles
    style.WindowRounding    = 0;
    style.ChildRounding     = 0;
    style.FrameRounding     = 0;
    style.GrabRounding      = 0;
    style.PopupRounding     = 0;
    style.TabRounding       = 0;
    style.ScrollbarRounding = 0;

    style.ButtonTextAlign   = { 0.5f, 0.5f };
    style.WindowTitleAlign  = { 0.5f, 0.5f };
    style.FramePadding      = { 8.0f, 8.0f };
    style.WindowPadding     = { 10.0f, 10.0f };
    style.ItemSpacing       = ImVec2(style.WindowPadding.x, style.WindowPadding.y * 0.75f);
    style.ItemInnerSpacing  = { 10, 4 };

    style.WindowBorderSize  = 1;
    style.FrameBorderSize   = 1;
    style.PopupBorderSize   = 1;

    style.ScrollbarSize     = 12.f;
    style.GrabMinSize       = style.FrameRounding;
    
    // Colors
    style.Colors[ImGuiCol_WindowBg]             = ImAdd::HexToColorVec4(0x181818, 0.3f);
    style.Colors[ImGuiCol_PopupBg]              = ImAdd::HexToColorVec4(0x181818, 1.0f);
    style.Colors[ImGuiCol_ChildBg]              = ImAdd::HexToColorVec4(0x282828, 1.0f);

    style.Colors[ImGuiCol_Text]                 = ImAdd::HexToColorVec4(0xFFFFFF, 1.0f);
    style.Colors[ImGuiCol_CheckMark]            = style.Colors[ImGuiCol_Text];
    style.Colors[ImGuiCol_TextDisabled]         = ImAdd::HexToColorVec4(0xA3A3A3, 1.0f);

    style.Colors[ImGuiCol_SliderGrab]           = ImAdd::HexToColorVec4(0x545070, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive]     = ImAdd::HexToColorVec4(0x45425D, 1.0f);
    
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImAdd::HexToColorVec4(0x181818, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImAdd::HexToColorVec4(0x181818, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImAdd::HexToColorVec4(0x181818, 1.0f);

    style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);

    style.Colors[ImGuiCol_Border]               = ImAdd::HexToColorVec4(0x060606, 1.0f);
    style.Colors[ImGuiCol_Separator]            = style.Colors[ImGuiCol_Border];

    style.Colors[ImGuiCol_Button]               = ImAdd::HexToColorVec4(0x181818, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]        = ImAdd::HexToColorVec4(0x181818, 0.7f);
    style.Colors[ImGuiCol_ButtonActive]         = ImAdd::HexToColorVec4(0x181818, 0.5f);

    style.Colors[ImGuiCol_FrameBg]              = ImAdd::HexToColorVec4(0x181818, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImAdd::HexToColorVec4(0x181818, 0.7f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImAdd::HexToColorVec4(0x181818, 0.5f);

    style.Colors[ImGuiCol_Header]               = ImAdd::HexToColorVec4(0x282828, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered]        = ImAdd::HexToColorVec4(0x282828, 0.7f);
    style.Colors[ImGuiCol_HeaderActive]         = ImAdd::HexToColorVec4(0x282828, 0.5f);

	if (m_Tabs.empty())
	{
		m_iSelectedPage = 0;

		m_Tabs.push_back("Aim");     // MenuPage_Aiming
		m_Tabs.push_back("ESP");    // MenuPage_Visuals
		m_Tabs.push_back("Config");    // MenuPage_Configs
		m_Tabs.push_back("Info");       // MenuPage_Info
	}
}

bool Overlay::Create()
{
	shouldRun = true;
	shouldRenderMenu = false;
	m_InsertHeld = false;
	m_FrameStart = {};
	m_iSelectedPage = 0;
	m_Tabs.clear();

	if (!CreateOverlay())
		return false;

	if (!CreateDevice())
		return false;

	if (!CreateImGui())
		return false;

	SetForeground(GetConsoleWindow());
	return true;
}

void Overlay::Destroy()
{
	DestroyImGui();
	DestroyDevice();
	DestroyOverlay();
}

void Overlay::RenderMenu()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowSize(ImVec2(570, 500), ImGuiCond_Always);
	ImGui::Begin(
		"Awhare",
		&shouldRenderMenu,
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoDecoration
	);

	StyleMenu(io, style);

	OverlayFps = ImGui::GetIO().Framerate;

	ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - ImGui::CalcTextSize("Awhare").x / 2);
	ImGui::Text("Awhare");

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
	ImGui::BeginChild("Main", ImVec2(0, 0), ImGuiCol_Border);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	{
		ImGui::BeginChild("MenuBar", ImVec2(0, ImGui::GetFrameHeight()), 0, ImGuiWindowFlags_NoBackground);
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			{
				int RadioWidth = ImGui::GetWindowWidth() / m_Tabs.size();

				for (int i = 0; i < m_Tabs.size(); i++)
				{
					ImAdd::RadioFrame(m_Tabs[i], &m_iSelectedPage, i, i % 2 == 0, ImVec2(i == m_Tabs.size() - 1 ? -0.1f : RadioWidth, ImGui::GetWindowHeight()));
					ImGui::SameLine();
				}
			}
			ImGui::PopStyleVar();

			ImGui::GetWindowDrawList()->AddLine(ImGui::GetWindowPos() + ImVec2(style.WindowBorderSize, ImGui::GetWindowHeight() - style.WindowBorderSize), ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowWidth() - style.WindowBorderSize, ImGui::GetWindowHeight() - style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
		}
		ImGui::EndChild();
		ImGui::SetCursorPosY(ImGui::GetFrameHeight());
		ImGui::BeginChild("Content", ImVec2(0, ImGui::GetWindowHeight() - ImGui::GetFrameHeight() * 2), ImGuiChildFlags_Border, ImGuiWindowFlags_NoBackground);
		{
			float fGroupWidth = (ImGui::GetWindowWidth() - style.WindowPadding.x * 2 - style.ItemSpacing.x) / 2;

			if (m_iSelectedPage == MenuPage_Aim)
			{
				constexpr std::uint64_t kAllBonesMask = Structs::AimAllBoneMask;
				auto drawBoneMaskEditor = [&](const char* idSuffix, std::uint64_t& mask, const std::uint64_t fallbackMask)
				{
					mask &= kAllBonesMask;
					for (size_t i = 0; i < Structs::AimBoneNames.size(); ++i)
					{
						const std::uint64_t bit = 1ull << static_cast<std::uint64_t>(i);
						bool enabled = (mask & bit) != 0ull;
						std::string label = std::string(Structs::AimBoneNames[i]) + "##" + idSuffix + std::to_string(i);
						if (ImGui::Checkbox(L(label.c_str()), &enabled))
						{
							if (enabled)
								mask |= bit;
							else
								mask &= ~bit;
						}

						if ((i % 2) == 0 && i + 1 < Structs::AimBoneNames.size())
							ImGui::SameLine(ImGui::GetWindowWidth() * 0.47f);
					}

					if (mask == 0)
						mask = fallbackMask;
				};

				ImGui::BeginChild("AimTabsRoot", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoBackground);
				{
					if (ImGui::BeginTabBar("AimSubTabs", ImGuiTabBarFlags_None))
					{
						if (ImGui::BeginTabItem(L("Aimbot")))
						{
							ImAdd::CheckBox("Aimbot##Enable", &config.Aim.Aimbot);

							if (config.Aim.Aimbot)
							{
								if (ProcInfo::KmboxInitialized)
								{
									if (ImGui::BeginTabBar("AimbotLayoutTabs", ImGuiTabBarFlags_None))
									{
										if (ImGui::BeginTabItem(L("General")))
										{
											ImAdd::SeparatorText("Hotkeys");
											ImAdd::KeyBindOptions primaryMode = (ImAdd::KeyBindOptions)config.Aim.AimbotKeyMode;
											ImAdd::KeyBind("Primary Key", &config.Aim.AimbotKey, 0, &primaryMode);
											config.Aim.AimbotKeyMode = (int)primaryMode;

											ImAdd::CheckBox("Enable Secondary Key", &config.Aim.AimbotSecondKeyEnabled);
											if (config.Aim.AimbotSecondKeyEnabled)
											{
												ImAdd::KeyBindOptions secondaryMode = (ImAdd::KeyBindOptions)config.Aim.AimbotSecondKeyMode;
												ImAdd::KeyBind("Secondary Key", &config.Aim.AimbotSecondKey, 0, &secondaryMode);
												config.Aim.AimbotSecondKeyMode = (int)secondaryMode;
											}

											ImAdd::SeparatorText("General");
											ImAdd::CheckBox("Draw FOV", &config.Aim.DrawFov);
											if (config.Aim.DrawFov)
											{
												ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
												ImAdd::ColorEdit4("##AimFovColor", (float*)&config.Aim.AimbotFovColor);
											}

											ImAdd::CheckBox("Dynamic FOV", &config.Aim.DynamicFov);
											ImAdd::SliderFloat("Dynamic FOV Min Px", &config.Aim.DynamicFovMinPx, 2.0f, 40.0f);
											ImAdd::CheckBox("Aim Visible", &config.Aim.AimVisible);
											ImAdd::CheckBox("Aim Teammates", &config.Aim.AimFriendly);
											ImAdd::CheckBox("Block Aimbot When Flashed", &config.Aim.BlockAimbotWhenFlashed);
											ImAdd::SliderFloat("Deadzone (px)", &config.Aim.DeadzonePx, 0.0f, 6.0f);

											ImAdd::SeparatorText("Aim Parts (Bone Points)");
											drawBoneMaskEditor("AimbotBoneParts", config.Aim.AimbotBoneMask, Structs::AimDefaultAimbotBoneMask);

											ImAdd::SeparatorText("RCS");
											ImAdd::CheckBox("Global RCS", &config.Aim.GlobalRcsEnabled);
											if (config.Aim.GlobalRcsEnabled)
											{
												ImAdd::SliderFloat("Global RCS Pitch", &config.Aim.GlobalRcsPitch, 0.0f, 4.0f);
												ImAdd::SliderFloat("Global RCS Yaw", &config.Aim.GlobalRcsYaw, 0.0f, 4.0f);
											}

											ImAdd::CheckBox("Aimbot RCS", &config.Aim.AimbotRcsEnabled);
											if (config.Aim.AimbotRcsEnabled)
											{
												ImAdd::SliderFloat("Aimbot RCS Pitch", &config.Aim.AimbotRcsPitch, 0.0f, 4.0f);
												ImAdd::SliderFloat("Aimbot RCS Yaw", &config.Aim.AimbotRcsYaw, 0.0f, 4.0f);
											}

											ImAdd::CheckBox("Fuse Global RCS During Aim", &config.Aim.FuseGlobalRcsWithAimbot);
											ImGui::TextDisabled("%s", L("Aimbot thread interval: 2ms (~500Hz)."));
											ImGui::EndTabItem();
										}

										if (ImGui::BeginTabItem(L("Weapon Tabs")))
										{
											if (ImGui::BeginTabBar("AimbotWeaponTabs", ImGuiTabBarFlags_None))
											{
												for (int i = 0; i < Structs::AimWeapon_Count; ++i)
												{
													if (ImGui::BeginTabItem(L(Structs::AimWeaponGroupNames[i])))
													{
														config.Aim.WeaponProfileEditorIndex = i;
														Structs::AimWeaponProfile& profile = config.Aim.WeaponProfiles[i];
														ImAdd::SliderFloat("Profile FOV", &profile.Fov, 0.1f, 45.0f);
														ImAdd::SliderFloat("Profile Smooth", &profile.Smooth, 1.0f, 100.0f);
														ImAdd::SliderFloat("Curve Strength", &profile.CurveStrength, 0.0f, 0.85f);
														ImAdd::CheckBox("Profile Dynamic FOV", &profile.DynamicFov);
														ImAdd::SliderFloat("Dynamic Distance Scale", &profile.DynamicFovDistanceScale, 400.0f, 4500.0f);
														ImAdd::Combo("Target Strategy", &profile.TargetStrategy, Structs::AimTargetStrategyNames.data(), (int)Structs::AimTargetStrategyNames.size());
														ImAdd::SliderInt("Target Switch Delay (ms)", &profile.TargetSwitchDelayMs, 0, 600);
														ImGui::EndTabItem();
													}
												}
												ImGui::EndTabBar();
											}

											ImGui::EndTabItem();
										}

										ImGui::EndTabBar();
									}
								}
								else
								{
									const char* disconnectedText = L("KMBOX not connected.");
									ImGui::SetCursorPos(
										ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) / 2 -
										ImGui::CalcTextSize(disconnectedText) / 2 + ImVec2(0, ImGui::GetFrameHeight())
									);
									ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", disconnectedText);
								}
							}

							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem(L("Trigger")))
						{
							ImAdd::CheckBox("Trigger##Enable", &config.Aim.Trigger);

							if (config.Aim.Trigger)
							{
								if (ProcInfo::KmboxInitialized)
								{
									if (ImGui::BeginTabBar("TriggerLayoutTabs", ImGuiTabBarFlags_None))
									{
										if (ImGui::BeginTabItem(L("General")))
										{
											ImAdd::SeparatorText("Hotkeys");
											ImGui::TextDisabled("%s", L("Trigger only works while holding hotkey."));
											ImAdd::KeyBindOptions triggerMode = (ImAdd::KeyBindOptions)config.Aim.TriggerKeyMode;
											ImAdd::KeyBind("Primary Trigger Key", &config.Aim.TriggerKey, 0, &triggerMode);
											config.Aim.TriggerKeyMode = (int)ImAdd::KeyBindOptions::OnKeyDown;

											ImAdd::CheckBox("Enable Secondary Trigger Key", &config.Aim.TriggerSecondKeyEnabled);
											if (config.Aim.TriggerSecondKeyEnabled)
											{
												ImAdd::KeyBindOptions triggerSecondMode = (ImAdd::KeyBindOptions)config.Aim.TriggerSecondKeyMode;
												ImAdd::KeyBind("Secondary Trigger Key", &config.Aim.TriggerSecondKey, 0, &triggerSecondMode);
												config.Aim.TriggerSecondKeyMode = (int)ImAdd::KeyBindOptions::OnKeyDown;
											}

											ImAdd::SeparatorText("Detection");
											ImAdd::Combo("Trigger Detect Mode", &config.Aim.TriggerDetectMode, Structs::TriggerDetectModeNames.data(), (int)Structs::TriggerDetectModeNames.size());
											ImAdd::SliderFloat("Unified Hitbox Radius (px)", &config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
											ImAdd::SliderFloat("Hitbox Scale", &config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
											ImAdd::SliderFloat("Hitbox Add (px)", &config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
											ImAdd::SliderFloat("Head Radius (px)", &config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);

											ImAdd::SeparatorText("Safety");
											ImAdd::CheckBox("Block Trigger When Flashed", &config.Aim.BlockTriggerWhenFlashed);
											ImGui::TextDisabled("%s", L("Reloading / non-gun is always blocked."));

											ImAdd::SeparatorText("Hitbox Debug");
											ImAdd::CheckBox("Enable Trigger Hitbox Debug", &config.Aim.TriggerHitboxDebug);
											ImAdd::CheckBox("Head Sphere Debug", &config.Aim.TriggerHeadSphereDebug);
											if (config.Aim.TriggerHitboxDebug)
											{
												ImGui::TextDisabled("%s", L("ESP draws 3D box per bone segment."));
												ImAdd::SliderFloat("Debug Thickness", &config.Aim.TriggerHitboxDebugThickness, 0.5f, 4.0f);
												ImAdd::ColorEdit4("Hitbox Color", (float*)&config.Aim.TriggerHitboxDebugColor);
												ImAdd::ColorEdit4("Active Hitbox Color", (float*)&config.Aim.TriggerHitboxDebugActiveColor);
											}

											ImGui::TextDisabled("%s", L("Trigger thread interval: 2ms (~500Hz)."));
											ImGui::EndTabItem();
										}

										if (ImGui::BeginTabItem(L("Weapon Tabs")))
										{
											if (ImGui::BeginTabBar("TriggerWeaponTabs", ImGuiTabBarFlags_None))
											{
												for (int i = 0; i < Structs::AimWeapon_Count; ++i)
												{
													if (ImGui::BeginTabItem(L(Structs::AimWeaponGroupNames[i])))
													{
														config.Aim.TriggerProfileEditorIndex = i;
														Structs::TriggerWeaponProfile& profile = config.Aim.TriggerProfiles[i];
														ImAdd::SliderInt("Pre Fire Delay (ms)", &profile.PreFireDelayMs, 0, 600);
														ImAdd::SliderInt("Post Fire Interval (ms)", &profile.PostFireIntervalMs, 0, 1200);
														ImAdd::SliderInt("Timeout Force Fire (ms)", &profile.TimeoutForceFireMs, 0, 3000);
														ImAdd::SeparatorText("Trigger Parts (Bone Points)");
														const std::string profileBoneMaskId = std::string("TriggerProfileBoneMask") + std::to_string(i);
														drawBoneMaskEditor(profileBoneMaskId.c_str(), profile.BoneMask, kAllBonesMask);
														ImGui::EndTabItem();
													}
												}

												for (int i = 0; i < Structs::TriggerSpecial_Count; ++i)
												{
													if (ImGui::BeginTabItem(L(Structs::TriggerSpecialWeaponNames[i])))
													{
														config.Aim.TriggerSpecialEditorIndex = i;
														Structs::TriggerSpecialProfile& special = config.Aim.TriggerSpecialProfiles[i];
														ImAdd::SliderInt("Special Pre Fire Delay (ms)", &special.PreFireDelayMs, 0, 600);
														ImAdd::SliderInt("Special Post Fire Interval (ms)", &special.PostFireIntervalMs, 0, 1500);
														ImAdd::SliderInt("Special Timeout Force Fire (ms)", &special.TimeoutForceFireMs, 0, 3000);
														ImAdd::SliderInt("Special Hold Fire (ms)", &special.HoldFireMs, 0, 600);
														ImAdd::SeparatorText("Trigger Parts (Bone Points)");
														const std::string specialBoneMaskId = std::string("TriggerSpecialBoneMask") + std::to_string(i);
														drawBoneMaskEditor(specialBoneMaskId.c_str(), special.BoneMask, kAllBonesMask);
														ImGui::EndTabItem();
													}
												}

												ImGui::EndTabBar();
											}
											ImGui::EndTabItem();
										}

										ImGui::EndTabBar();
									}
								}
								else
								{
									const char* disconnectedText = L("KMBOX not connected.");
									ImGui::SetCursorPos(
										ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) / 2 -
										ImGui::CalcTextSize(disconnectedText) / 2 + ImVec2(0, ImGui::GetFrameHeight())
									);
									ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", disconnectedText);
								}
							}

							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}
				}
				ImGui::EndChild();
			}

			else if (m_iSelectedPage == MenuPage_Visuals)
			{
				ImGui::BeginChild("Visuals", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_MenuBar);
				{
					if (ImGui::BeginMenuBar()) {
						ImGui::Text("%s", L("ESP"));
						ImGui::EndMenuBar();
					}

					const float panelSpacing = style.ItemSpacing.x;
					const float fullWidth = ImGui::GetContentRegionAvail().x;
					const float leftPanelWidth = (fullWidth - panelSpacing) * 0.50f;

					ImGui::BeginChild("VisualsHeaderRow", ImVec2(0, ImGui::GetFrameHeight() + style.ItemSpacing.y), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
					{
						ImAdd::CheckBox(Localization::Pick("ESP Visuals", "透视ESP"), &config.Visuals.Enabled);
						ImGui::SameLine(leftPanelWidth + panelSpacing + style.WindowPadding.x * 0.35f);
						ImAdd::CheckBox(Localization::Pick("GHelper", "GHelper"), &config.Visuals.GrenadeHelper);
					}
					ImGui::EndChild();

					ImGui::BeginChild("EspSettingsPanel", ImVec2(leftPanelWidth, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);
					{
						if (config.Visuals.Enabled)
						{
							ImAdd::SeparatorText("General");

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Watermark", &config.Visuals.Watermark);
								if (config.Visuals.Watermark)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##WatermarkColor", (float*)&config.Visuals.WatermarkColor);
								}
							}

							ImAdd::CheckBox("Background", &config.Visuals.Background);

							ImAdd::SeparatorText("Visual");
							ImAdd::CheckBox("VSync", &config.Visuals.VSync);
							ImAdd::CheckBox("Team Check", &config.Visuals.TeamCheck);
							ImAdd::CheckBox("Visible Check", &config.Visuals.VisibleCheck);

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Hitmarker", &config.Visuals.Hitmarker);
								if (config.Visuals.Hitmarker)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##HitmarkerColor", (float*)&config.Visuals.HitmarkerColor);
								}
							}

							ImAdd::SeparatorText("Players");

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Name", &config.Visuals.Name);
								if (config.Visuals.Name)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##NameColor", (float*)&config.Visuals.NameColor);
								}
							}

							ImAdd::CheckBox("Health", &config.Visuals.Health);

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Armor", &config.Visuals.Armor);
								if (config.Visuals.Armor)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##ArmorColor", (float*)&config.Visuals.ArmorColor);
								}
							}

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Money", &config.Visuals.Money);
								if (config.Visuals.Money)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##MoneyColor", (float*)&config.Visuals.MoneyColor);
								}
							}

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Defuser", &config.Visuals.Defuser);
								if (config.Visuals.Defuser)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##DefuserColor", (float*)&config.Visuals.DefuserColor);
								}
							}

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Box", &config.Visuals.Box);
								if (config.Visuals.Box)
								{
									ImAdd::ColorEdit4("Box Color", (float*)&config.Visuals.BoxColor);
									ImAdd::ColorEdit4("Box Color Visible", (float*)&config.Visuals.BoxColorVisible);
								}
							}

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Weapon", &config.Visuals.Weapon);
								if (config.Visuals.Weapon)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##WeaponColor", (float*)&config.Visuals.WeaponColor);
								}
							}

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Bones", &config.Visuals.Bones);
								if (config.Visuals.Bones)
								{
									ImAdd::ColorEdit4("Bones Color", (float*)&config.Visuals.BonesColor);
									ImAdd::ColorEdit4("Bones Color Visible", (float*)&config.Visuals.BonesColorVisible);
								}
							}

							ImAdd::SeparatorText("World");
							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("C4", &config.Visuals.C4);
								if (config.Visuals.C4)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##C4Color", (float*)&config.Visuals.C4Color);
									ImAdd::SliderFloat("C4 Card X", &config.Visuals.C4PanelPosX, 0.0f, 1.0f);
									ImAdd::SliderFloat("C4 Card Y", &config.Visuals.C4PanelPosY, 0.0f, 1.0f);
								}
							}
						}
						else
						{
							ImGui::TextDisabled("%s", Localization::Pick("Enable ESP Visuals to configure ESP settings.", "启用透视ESP后可配置左侧透视设置。"));
						}
					}
					ImGui::EndChild();

					ImGui::SameLine(0.0f, panelSpacing);
					ImGui::BeginChild("UtilityHelperPanel", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);
					{
						ImAdd::SeparatorText(Localization::Pick("GHelper Settings", "GHelper设置"));
						ImGui::TextWrapped("%s", Localization::Pick("Independent utility helper settings panel.", "独立道具辅助设置面板。"));
						ImGui::Spacing();

						if (config.Visuals.GrenadeHelper)
						{
							static char helperMapName[64] = "";
							static char helperNote[128] = "";
							static float helperRecordDistance = 8000.0f;
							static int helperThrowTypeIndex = 0;
							static int helperSelectedRow = -1;
							static std::vector<GrenadeSpotEditorRow> helperRows{};
							static std::string helperStatus{};

							if (helperMapName[0] == '\0')
							{
								const std::string suggestedMap = esp.GetSuggestedGrenadeMapName();
								strncpy_s(helperMapName, suggestedMap.c_str(), _TRUNCATE);
								helperStatus = esp.GetGrenadeStatus();
							}

							const char* grenadeTypeItems[] = {
								Localization::Pick("Smoke", "烟雾"),
								Localization::Pick("Flashbang", "闪光"),
								Localization::Pick("HE Grenade", "高爆"),
								Localization::Pick("Decoy", "诱饵"),
								Localization::Pick("Molotov", "燃烧瓶")
							};
							const char* throwTypeItems[] = {
								Localization::Pick("Stand Throw", "站投"),
								Localization::Pick("Jump Throw", "跳投"),
								Localization::Pick("Run Throw", "跑投"),
								Localization::Pick("Run Jump Throw", "跑跳投")
							};

							ImAdd::CheckBox(Localization::Pick("Filter By Held Utility", "按手持道具过滤"), &config.Visuals.GrenadeHelperFilterByWeapon);
							ImAdd::CheckBox(Localization::Pick("Draw Stand Positions", "绘制站位点"), &config.Visuals.GrenadeHelperDrawStand);
							ImAdd::CheckBox(Localization::Pick("Draw Aim Targets", "绘制瞄点"), &config.Visuals.GrenadeHelperDrawAim);
							ImAdd::CheckBox(Localization::Pick("Manual Utility Type", "手动指定道具类型"), &config.Visuals.GrenadeHelperManualTypeOverride);

							if (config.Visuals.GrenadeHelperManualTypeOverride)
							{
								int grenadeTypeIndex = std::clamp(config.Visuals.GrenadeHelperManualType, 0, 4);
								if (ImAdd::Combo(Localization::Pick("Utility Type", "道具类型"), &grenadeTypeIndex, grenadeTypeItems, IM_ARRAYSIZE(grenadeTypeItems)))
									config.Visuals.GrenadeHelperManualType = grenadeTypeIndex;
							}

							ImAdd::SliderFloat(Localization::Pick("Stand Tolerance", "站位容差"), &config.Visuals.GrenadeHelperStandTolerance, 10.0f, 120.0f);
							config.Visuals.GrenadeHelperFocusRadius = std::clamp(config.Visuals.GrenadeHelperFocusRadius, 5.0f, 50.0f);
							ImAdd::SliderFloat(Localization::Pick("Focus / Aim Radius", "聚焦/瞄点半径"), &config.Visuals.GrenadeHelperFocusRadius, 5.0f, 50.0f);
							ImAdd::SliderFloat(Localization::Pick("Max Stand Draw Distance", "站位最远绘制距离"), &config.Visuals.GrenadeHelperMaxStandDrawDistance, 50.0f, 8000.0f);
							ImAdd::SliderFloat(Localization::Pick("Guide Line Threshold", "引导线阈值"), &config.Visuals.GrenadeHelperLooseGuideDistance, 50.0f, 1000.0f);
							ImAdd::SliderFloat(Localization::Pick("Top Hint X Ratio", "顶部提示X比例"), &config.Visuals.GrenadeHelperTopHintOffsetX, 0.0f, 1.0f);
							ImAdd::SliderFloat(Localization::Pick("Top Hint Y Ratio", "顶部提示Y比例"), &config.Visuals.GrenadeHelperTopHintOffsetY, 0.0f, 1.0f);
							ImAdd::SeparatorText(Localization::Pick("Helper Style", "辅助样式"));
							ImAdd::ColorEdit4(Localization::Pick("Stand Point Color", "点位颜色"), (float*)&config.Visuals.GrenadeHelperStandColor);
							ImAdd::ColorEdit4(Localization::Pick("Aim Point Color", "瞄点颜色"), (float*)&config.Visuals.GrenadeHelperAimColor);
							ImAdd::ColorEdit4(Localization::Pick("Guide Line Color", "引导线颜色"), (float*)&config.Visuals.GrenadeHelperGuideLineColor);
							ImAdd::ColorEdit4(Localization::Pick("Font Color", "字体颜色"), (float*)&config.Visuals.GrenadeHelperFontColor);
							ImAdd::SliderFloat(Localization::Pick("Font Size", "字体大小"), &config.Visuals.GrenadeHelperFontSize, 10.0f, 32.0f);
							ImAdd::ColorEdit4(Localization::Pick("Throw Hint Color", "投掷提示颜色"), (float*)&config.Visuals.GrenadeHelperTopHintColor);
							ImAdd::SliderFloat(Localization::Pick("Throw Hint Size", "投掷提示大小"), &config.Visuals.GrenadeHelperTopHintFontSize, 14.0f, 72.0f);

							ImAdd::SeparatorText(Localization::Pick("Spot Management", "点位管理"));
							ImGui::InputText(Localization::Pick("Map Name", "地图名"), helperMapName, IM_ARRAYSIZE(helperMapName));
							ImGui::InputText(Localization::Pick("Spot Note", "点位备注"), helperNote, IM_ARRAYSIZE(helperNote));
							ImAdd::Combo(Localization::Pick("Record Throw Type", "记录投掷方式"), &helperThrowTypeIndex, throwTypeItems, IM_ARRAYSIZE(throwTypeItems));
							ImAdd::SliderFloat(Localization::Pick("Record Aim Distance", "记录瞄点距离"), &helperRecordDistance, 500.0f, 50000.0f);

							if (ImAdd::Button(Localization::Pick("Reload Spots", "重载点位"), ImVec2(110.0f, 0.0f)))
							{
								std::string status{};
								if (esp.ReloadGrenadeSpots(helperMapName, status))
								{
									helperRows = esp.GetGrenadeSpotEditorRows();
									helperSelectedRow = helperRows.empty() ? -1 : 0;
								}
								helperStatus = status;
							}
							ImGui::SameLine();
							if (ImAdd::Button(Localization::Pick("Sync Held Type", "同步手持类型"), ImVec2(126.0f, 0.0f)))
							{
								int detectedTypeIndex = -1;
								if (esp.DetectCurrentGrenadeTypeIndex(detectedTypeIndex))
								{
									config.Visuals.GrenadeHelperManualType = detectedTypeIndex;
									config.Visuals.GrenadeHelperManualTypeOverride = true;
									helperStatus = Localization::Pick("Synced current held grenade type", "已同步当前手持道具类型");
								}
								else
								{
									helperStatus = Localization::Pick("Sync failed: not holding a utility grenade", "同步失败：当前未手持可识别道具");
								}
							}
							ImGui::SameLine();
							if (ImAdd::Button(Localization::Pick("Record Spot", "记录点位"), ImVec2(96.0f, 0.0f)))
							{
								std::string status{};
								if (esp.RecordCurrentGrenadeSpot(
									helperMapName,
									helperNote,
									helperThrowTypeIndex,
									helperRecordDistance,
									config.Visuals.GrenadeHelperManualTypeOverride,
									config.Visuals.GrenadeHelperManualType,
									status))
								{
									helperRows = esp.GetGrenadeSpotEditorRows();
									helperSelectedRow = helperRows.empty() ? -1 : 0;
								}
								helperStatus = status;
							}

							if (ImAdd::Button(Localization::Pick("Delete Selected", "删除选中"), ImVec2(110.0f, 0.0f)))
							{
								if (helperSelectedRow >= 0 && helperSelectedRow < static_cast<int>(helperRows.size()))
								{
									helperRows.erase(helperRows.begin() + helperSelectedRow);
									if (helperRows.empty())
										helperSelectedRow = -1;
									else
										helperSelectedRow = std::clamp(helperSelectedRow, 0, static_cast<int>(helperRows.size()) - 1);
								}
							}
							ImGui::SameLine();
							if (ImAdd::Button(Localization::Pick("Save List", "保存列表"), ImVec2(110.0f, 0.0f)))
							{
								std::string status{};
								if (esp.SaveGrenadeSpotEditorRows(helperMapName, helperRows, status))
								{
									helperRows = esp.GetGrenadeSpotEditorRows();
									helperSelectedRow = helperRows.empty() ? -1 : std::clamp(helperSelectedRow, 0, static_cast<int>(helperRows.size()) - 1);
								}
								helperStatus = status;
							}

							if (!helperStatus.empty())
								ImGui::TextWrapped("%s", helperStatus.c_str());

							const ImVec2 listSize(0.0f, ImGui::GetTextLineHeightWithSpacing() * 11.0f);
							if (ImGui::BeginTable("GHelperSpotTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable, listSize))
							{
								ImGui::TableSetupColumn(Localization::Pick("ID", "编号"), ImGuiTableColumnFlags_WidthFixed, 40.0f);
								ImGui::TableSetupColumn(Localization::Pick("Type", "类型"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
								ImGui::TableSetupColumn(Localization::Pick("Throw", "投掷"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
								ImGui::TableSetupColumn(Localization::Pick("Name", "名称"), ImGuiTableColumnFlags_WidthStretch);
								ImGui::TableHeadersRow();

								for (int rowIndex = 0; rowIndex < static_cast<int>(helperRows.size()); ++rowIndex)
								{
									GrenadeSpotEditorRow& row = helperRows[static_cast<std::size_t>(rowIndex)];
									ImGui::PushID(row.Id != 0 ? row.Id : rowIndex);
									ImGui::TableNextRow();

									ImGui::TableSetColumnIndex(0);
									if (ImGui::Selectable("##SpotSelect", helperSelectedRow == rowIndex, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
										helperSelectedRow = rowIndex;
									ImGui::SameLine();
									ImGui::Text("%d", row.Id);

									ImGui::TableSetColumnIndex(1);
									int typeIndex = std::clamp(row.TypeIndex, 0, 4);
									if (ImGui::Combo("##Type", &typeIndex, grenadeTypeItems, IM_ARRAYSIZE(grenadeTypeItems)))
										row.TypeIndex = typeIndex;

									ImGui::TableSetColumnIndex(2);
									int throwIndex = std::clamp(row.ThrowTypeIndex, 0, 3);
									if (ImGui::Combo("##Throw", &throwIndex, throwTypeItems, IM_ARRAYSIZE(throwTypeItems)))
										row.ThrowTypeIndex = throwIndex;

									ImGui::TableSetColumnIndex(3);
									char rowNameBuffer[128]{};
									strncpy_s(rowNameBuffer, row.Name.c_str(), _TRUNCATE);
									if (ImGui::InputText("##Name", rowNameBuffer, IM_ARRAYSIZE(rowNameBuffer)))
										row.Name = rowNameBuffer;

									ImGui::PopID();
								}

								ImGui::EndTable();
							}
						}
						else
						{
							ImGui::TextDisabled("%s", Localization::Pick("Enable GHelper Utility to configure helper settings.", "启用GHelper后可配置右侧道具辅助设置。"));
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndChild();
			}
			else if (m_iSelectedPage == MenuPage_Config)
			{
				ImGui::BeginChild("Configs", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_MenuBar);
				{
					if (ImGui::BeginMenuBar()) {
						ImGui::Text("%s", L("Configs"));
						ImGui::EndMenuBar();
					}
						
					static char configName[128] = "";
					static std::vector<std::string> configFiles;

					if (ImAdd::Button("Refresh"))
					{
						configFiles = config.ListConfigs("configs/");
						LOG_INFO("Refreshed config list");
					}

					ImGui::Separator();

					// Config List
					if (ImGui::BeginListBox(L("Config list")))
					{
						if (configFiles.empty())
						{
							ImGui::Selectable(L("No configs found"), false, ImGuiSelectableFlags_Disabled);
						}
						else
						{
							for (const auto& file : configFiles)
							{
								bool isSelected = (file == configName);
								if (ImGui::Selectable(file.c_str(), isSelected))
								{
									strcpy(configName, file.c_str());
									LOG_INFO("Selected config: {}", configName);
								}

								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
						}
						ImGui::EndListBox();
					}

					// Config Name Input
					ImGui::InputText(L("Config Name"), configName, IM_ARRAYSIZE(configName));

					// Control Buttons
					float buttonWidth = 75.0f;
					float buttonSpacing = 10.0f;
					ImGui::Dummy(ImVec2(0.0f, 5.0f));

					if (ImAdd::Button("Load", ImVec2(buttonWidth, 0)))
					{
						std::string filePath = "configs/" + std::string(configName);
						if (!config.LoadFromFile(filePath))
						{
							LOG_ERROR("Failed to load config: {}", filePath);
						}
						else
						{
							SyncLanguageFromConfig();
							LOG_INFO("Loaded config: {}", filePath);
						}
					}

					ImGui::SameLine(0.0f, buttonSpacing);

					if (ImAdd::Button("Save", ImVec2(buttonWidth, 0)))
					{
						std::string filePath = "configs/" + std::string(configName);
						if (!config.SaveToFile(filePath))
						{
							LOG_ERROR("Failed to save config: {}", filePath);
						}
						else
						{
							LOG_INFO("Saved config: {}", filePath);
						}
					}

					ImGui::SameLine(0.0f, buttonSpacing);

					if (ImAdd::Button("Delete", ImVec2(buttonWidth, 0)))
					{
						std::string filePath = "configs/" + std::string(configName);
						if (!config.DeleteConfigFile(filePath))
						{
							LOG_ERROR("Failed to delete config: {}", filePath);
						}
						else
						{
							LOG_INFO("Deleted config: {}", filePath);
							configFiles = config.ListConfigs("configs/");
						}
					}

					ImGui::SameLine(0.0f, buttonSpacing);

					if (ImAdd::Button("Import", ImVec2(buttonWidth, 0)))
					{
						if (config.LoadFromClipboard())
						{
							SyncLanguageFromConfig();
							LOG_INFO("Config imported from clipboard");
						}
						else
						{
							LOG_ERROR("Failed to import config from clipboard");
						}
					}
				}
				ImGui::EndChild();
			}

			else if (m_iSelectedPage == MenuPage_Info)
			{
				ImGui::BeginChild("Info", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_MenuBar);
				{
					if (ImGui::BeginMenuBar()) {
						ImGui::Text("%s", L("Info"));
						ImGui::EndMenuBar();
					}

					int languageIndex = std::clamp(config.Language, 0, 1);
					const char* languageItems[] = { "English", "Chinese" };
					if (ImAdd::Combo("Language", &languageIndex, languageItems, IM_ARRAYSIZE(languageItems)))
					{
						languageIndex = std::clamp(languageIndex, 0, 1);
						config.Language = languageIndex;
						Localization::CurrentLanguage = static_cast<Localization::Language>(config.Language);

						if (!config.SaveToFile("configs/config.json"))
							LOG_ERROR("Failed to persist language setting to configs/config.json");
					}

					ImAdd::SeparatorText("Hardware");

					ImGui::Text("%s", L("DMA:"));
					ImGui::SameLine();
					ImGui::TextColored(
						ProcInfo::DmaInitialized ? ImVec4(0, 1, 0, 1)/* green */ : ImVec4(1, 0, 0, 1)/* red */,
						"%s",
						ProcInfo::DmaInitialized ? L("Connected") : L("Disconnected")
					);

					ImGui::Text("%s", L("KMBOX:"));
					ImGui::SameLine();
					ImGui::TextColored(
						ProcInfo::KmboxInitialized ? ImVec4(0, 1, 0, 1)/* green */: ImVec4(1, 0, 0, 1)/* red */,
						"%s",
						ProcInfo::KmboxInitialized ? L("Connected") : L("Disconnected")
					);

					ImAdd::SeparatorText("Game");

					ImGui::Text("%s", L("Client:"));
					ImGui::SameLine();
					ImGui::Text("0x%llx", Globals::ClientBase);

					ImAdd::SeparatorText("Cheat");

					ImGui::Text(L("Overlay FPS: %.2f"), OverlayFps);
					ImGui::Text(L("Host INSERT: %s"), IsHostKeyDown(VK_INSERT) ? L("Down") : L("Up"));
					ImGui::Text(L("Host LMB: %s"), IsHostKeyDown(VK_LBUTTON) ? L("Down") : L("Up"));
					ImGui::Text(L("Host RMB: %s"), IsHostKeyDown(VK_RBUTTON) ? L("Down") : L("Up"));
					ImGui::Text(L("Host X1: %s"), IsHostKeyDown(VK_XBUTTON1) ? L("Down") : L("Up"));
					ImGui::Text(L("Host X2: %s"), IsHostKeyDown(VK_XBUTTON2) ? L("Down") : L("Up"));

					float buttonWidth = 100.0f;
					float buttonSpacing = 20.0f;
					ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 2 * buttonWidth - buttonSpacing) / 2);

					if (ImAdd::Button("Open folder", ImVec2(buttonWidth, 0)))
					{
						ShellExecuteA(nullptr, "open", "explorer.exe", ".\\", nullptr, SW_SHOW);
					}

					ImGui::SameLine();

					if (ImAdd::Button("Unload", ImVec2(buttonWidth, 0)))
					{
						Globals::Running = false;
						shouldRun = false;
					}
				}
				ImGui::EndChild();
			}
		}
		ImGui::EndChild();
		ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeight());
		ImGui::BeginChild("Footer", ImVec2(0, 0), 0, ImGuiWindowFlags_NoBackground);
		{
			ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(), ImGui::GetColorU32(ImGuiCol_ChildBg), style.WindowRounding, ImDrawFlags_RoundCornersBottom);
			ImGui::GetWindowDrawList()->AddLine(ImGui::GetWindowPos() + ImVec2(style.WindowBorderSize, 0), ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowWidth() - style.WindowBorderSize, 0), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);
			const char* buildText = L("Build: Developer");
			const char* expiryText = L("Expires: Never");
			ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowPos() + style.FramePadding, ImGui::GetColorU32(ImGuiCol_Text), buildText);
			ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowWidth() - ImGui::CalcTextSize(expiryText).x - style.FramePadding.x, style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), expiryText);
		}
		ImGui::EndChild();
	}

	ImGui::EndChild();
	ImGui::End();
}
