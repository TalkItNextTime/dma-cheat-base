#include <Pch.hpp>
#include <SDK.hpp>
#include <ESP/ESP.hpp>
#include <Radar/Radar.hpp>
#include <array>
#include <wincrypt.h>
#include <wincodec.h>

#include "Overlay.hpp"
#include "BonesShowcaseEmbedded.hpp"
#include "Fonts/IBMPlexMono_Medium.h"
#include "Localization.hpp"
#include "StartupStatus.hpp"

#pragma comment(lib, "Crypt32.lib")

ID3D11Device* Overlay::device = nullptr;

ID3D11DeviceContext* Overlay::device_context = nullptr;

IDXGISwapChain* Overlay::swap_chain = nullptr;

ID3D11RenderTargetView* Overlay::render_targetview = nullptr;

HWND Overlay::overlay = nullptr;
WNDCLASSEX Overlay::wc = { };

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	void SyncLanguageFromConfig()
	{
		config.Language = std::clamp(config.Language, 0, 1);
		Localization::CurrentLanguage = static_cast<Localization::Language>(config.Language);
	}

	const char* LocalizedMainTabLabel(const int pageIndex)
	{
		switch (pageIndex)
		{
		case MenuPage_Aim:
			return Localization::Pick("Aim", "自瞄");
		case MenuPage_Visuals:
			return Localization::Pick("Visuals", "视觉");
		case MenuPage_Radar:
			return Localization::Pick("Radar", "雷达");
		case MenuPage_Config:
			return Localization::Pick("Config", "配置");
		case MenuPage_Info:
			return Localization::Pick("Info", "信息");
		default:
			return "";
		}
	}

	const char* LocalizedAimWeaponGroupName(const int index)
	{
		const int clamped = std::clamp(index, 0, Structs::AimWeapon_Count - 1);
		return Localization::Pick(Structs::AimWeaponGroupNames[clamped], Structs::AimWeaponGroupNamesZh[clamped]);
	}

	const char* LocalizedTriggerSpecialWeaponName(const int index)
	{
		const int clamped = std::clamp(index, 0, Structs::TriggerSpecial_Count - 1);
		return Localization::Pick(Structs::TriggerSpecialWeaponNames[clamped], Structs::TriggerSpecialWeaponNamesZh[clamped]);
	}

	const char* LocalizedAimBoneName(const int slot)
	{
		const int clamped = std::clamp(slot, 0, static_cast<int>(Structs::AimBoneNames.size()) - 1);
		return Localization::Pick(Structs::AimBoneNames[clamped], Structs::AimBoneNamesZh[clamped]);
	}

	const std::array<const char*, 3>& LocalizedTargetStrategyNames()
	{
		return Localization::IsChinese() ? Structs::AimTargetStrategyNamesZh : Structs::AimTargetStrategyNames;
	}

	const std::array<const char*, 2>& LocalizedTriggerDetectModeNames()
	{
		return Localization::IsChinese() ? Structs::TriggerDetectModeNamesZh : Structs::TriggerDetectModeNames;
	}

	struct ThrowKeySelection
	{
		std::string Mouse = "LB";
		bool W = false;
		bool A = false;
		bool S = false;
		bool D = false;
		bool Shift = false;
		bool Ctrl = false;
		bool Space = false;
	};

	std::string TrimAscii(std::string value)
	{
		const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
		while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
			value.pop_back();
		return value;
	}

	std::string ToUpperAscii(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
		{
			return static_cast<char>(std::toupper(c));
		});
		return value;
	}

	std::string NormalizeMouseToken(const std::string& tokenUpper)
	{
		if (tokenUpper == "LB" || tokenUpper == "LMB" || tokenUpper == "LEFT" || tokenUpper == "LEFTCLICK")
			return "LB";
		if (tokenUpper == "RB" || tokenUpper == "RMB" || tokenUpper == "RIGHT" || tokenUpper == "RIGHTCLICK")
			return "RB";
		if (tokenUpper == "LRB" || tokenUpper == "DUAL" || tokenUpper == "BOTH")
			return "LRB";
		return {};
	}

	void ParseThrowToken(const std::string& rawToken, ThrowKeySelection& inOut)
	{
		const std::string trimmed = TrimAscii(rawToken);
		const std::string tokenUpper = ToUpperAscii(trimmed);
		if (tokenUpper.empty())
			return;

		if (const std::string mouse = NormalizeMouseToken(tokenUpper); !mouse.empty())
		{
			inOut.Mouse = mouse;
			return;
		}
		if (tokenUpper == "W")
			inOut.W = true;
		else if (tokenUpper == "A")
			inOut.A = true;
		else if (tokenUpper == "S")
			inOut.S = true;
		else if (tokenUpper == "D")
			inOut.D = true;
		else if (tokenUpper == "SHIFT" || tokenUpper == "SHIFT_EN" || tokenUpper == "SHIFTEN" || tokenUpper == "SHIFT-EN" ||
			tokenUpper == "WALK" || tokenUpper == "SLOWWALK" || tokenUpper == "SILENTWALK" ||
			trimmed == "静步" || trimmed == "静走" || trimmed == "慢走" || trimmed == "走" || trimmed == "静")
			inOut.Shift = true;
		else if (tokenUpper == "CTRL" )
			inOut.Ctrl = true;
		else if (tokenUpper == "SPACE" )
			inOut.Space = true;
	}

	ThrowKeySelection ParseThrowType(const std::string& throwTypeRaw)
	{
		ThrowKeySelection keys{};
		const std::string throwType = TrimAscii(throwTypeRaw);
		if (throwType.empty())
			return keys;

		const std::string lowered = ToUpperAscii(throwType);
		if (lowered == "STANDTHROW")
			return keys;
		if (lowered == "JUMPTHROW")
		{
			keys.Space = true;
			return keys;
		}
		if (lowered == "RUNTHROW")
		{
			keys.W = true;
			return keys;
		}
		if (lowered == "RUNJUMPTHROW" || lowered == "RUNJUMP")
		{
			keys.W = true;
			keys.Space = true;
			return keys;
		}
		if (lowered == "WALKTHROW" || lowered == "SHIFTTHROW" || lowered == "SILENTWALKTHROW")
		{
			keys.Shift = true;
			return keys;
		}
		if (lowered == "WALKJUMPTHROW" || lowered == "WALKJUMP" || lowered == "SHIFTJUMPTHROW" || lowered == "SILENTWALKJUMPTHROW")
		{
			keys.Shift = true;
			keys.Space = true;
			return keys;
		}

		size_t begin = 0;
		while (begin <= throwType.size())
		{
			const size_t plusPos = throwType.find('+', begin);
			const size_t endPos = plusPos == std::string::npos ? throwType.size() : plusPos;
			ParseThrowToken(throwType.substr(begin, endPos - begin), keys);
			if (plusPos == std::string::npos)
				break;
			begin = plusPos + 1;
		}

		return keys;
	}

	std::string BuildCanonicalThrowType(const ThrowKeySelection& keys)
	{
		std::string result = keys.Mouse.empty() ? std::string("LB") : keys.Mouse;
		const auto appendKey = [&](const char* key)
		{
			result += "+";
			result += key;
		};

		if (keys.W) appendKey("W");
		if (keys.A) appendKey("A");
		if (keys.S) appendKey("S");
		if (keys.D) appendKey("D");
		if (keys.Shift) appendKey("Shift");
		if (keys.Ctrl) appendKey("Ctrl");
		if (keys.Space) appendKey("Space");
		return result;
	}

	std::vector<std::string> BuildThrowDisplayTokens(const std::string& throwTypeRaw, const bool includePlus)
	{
		const ThrowKeySelection keys = ParseThrowType(throwTypeRaw);
		std::vector<std::string> keyboard{};
		if (keys.W) keyboard.push_back("W");
		if (keys.A) keyboard.push_back("A");
		if (keys.S) keyboard.push_back("S");
		if (keys.D) keyboard.push_back("D");
		if (keys.Shift) keyboard.push_back("Shift");
		if (keys.Ctrl) keyboard.push_back("Ctrl");
		if (keys.Space) keyboard.push_back("Space");

		std::vector<std::string> out{};
		out.reserve(keyboard.size() + (includePlus ? 2 : 1));
		out.push_back(keys.Mouse.empty() ? std::string("LB") : keys.Mouse);
		if (!keyboard.empty())
		{
			if (includePlus)
			{
				out.push_back("+");
				out.insert(out.end(), keyboard.begin(), keyboard.end());
			}
			else
			{
				for (const std::string& key : keyboard)
					out.push_back(key);
			}
		}

		return out;
	}

	std::string GetThrowTokenDisplayText(const std::string& rawToken)
	{
		const std::string tokenUpper = ToUpperAscii(TrimAscii(rawToken));
		if (tokenUpper == "PLUS" || tokenUpper == "+")
			return "+";

		const bool isShiftToken = tokenUpper == "SHIFT" || tokenUpper == "SHIFT_EN" || tokenUpper == "SHIFTEN" || tokenUpper == "SHIFT-EN";
		if (Localization::IsChinese())
		{
			if (tokenUpper == "LB")
				return "左键";
			if (tokenUpper == "RB")
				return "右键";
			if (tokenUpper == "LRB")
				return "双键";
			if (isShiftToken)
				return "静步";
			if (tokenUpper == "CTRL" )
				return "蹲";
			if (tokenUpper == "SPACE")
				return "跳";
		}

		if (tokenUpper == "LB")
			return "LB";
		if (tokenUpper == "RB")
			return "RB";
		if (tokenUpper == "LRB")
			return "LRB";
		if (isShiftToken)
			return "Shift";
		if (tokenUpper == "CTRL" )
			return "Ctrl";
		if (tokenUpper == "SPACE")
			return "Space";
		return rawToken;
	}

	void DrawThrowTypeTagRow(const std::string& throwTypeRaw, const char* idScope, const bool includePlus = true)
	{
		const std::vector<std::string> tokens = BuildThrowDisplayTokens(throwTypeRaw, includePlus);
		if (tokens.empty())
			return;

		ImGui::PushID(idScope);
		for (size_t i = 0; i < tokens.size(); ++i)
		{
			if (i > 0)
				ImGui::SameLine(0.0f, 4.0f);

			ImGui::BeginDisabled();
			const std::string displayText = GetThrowTokenDisplayText(tokens[i]);
			ImGui::SmallButton(displayText.c_str());
			ImGui::EndDisabled();
		}
		ImGui::PopID();
	}

	bool DrawThrowTypePopupEditor(const char* popupId, std::string& inOutThrowType)
	{
		bool changed = false;
		static ThrowKeySelection draft{};
		static std::string activePopup{};

		if (ImGui::BeginPopup(popupId))
		{
			if (activePopup != popupId)
			{
				draft = ParseThrowType(inOutThrowType);
				activePopup = popupId;
			}

			ImGui::TextUnformatted(Localization::Pick("Mouse Buttons", "鼠标键位"));
			int mouseIndex = 0;
			if (draft.Mouse == "RB")
				mouseIndex = 1;
			else if (draft.Mouse == "LRB")
				mouseIndex = 2;
			const char* mouseTokens[] = { "LB", "RB", "LRB" };
			const char* mouseItems[] = {
				Localization::Pick("LB", "左键"),
				Localization::Pick("RB", "右键"),
				Localization::Pick("LRB", "双键")
			};
			if (ImGui::Combo("##ThrowMouse", &mouseIndex, mouseItems, IM_ARRAYSIZE(mouseItems)))
				draft.Mouse = mouseTokens[std::clamp(mouseIndex, 0, 2)];

			ImGui::Spacing();
			ImGui::TextUnformatted(Localization::Pick("Keyboard Keys", "键盘按键"));
			ImGui::Checkbox("W", &draft.W);
			ImGui::SameLine();
			ImGui::Checkbox("A", &draft.A);
			ImGui::SameLine();
			ImGui::Checkbox("S", &draft.S);
			ImGui::SameLine();
			ImGui::Checkbox("D", &draft.D);
			ImGui::Checkbox(Localization::Pick("Shift", "静步"), &draft.Shift);
			ImGui::SameLine();
			ImGui::Checkbox(Localization::Pick("Ctrl", "蹲"), &draft.Ctrl);
			ImGui::SameLine();
			ImGui::Checkbox(Localization::Pick("Space", "跳"), &draft.Space);

			const std::string preview = BuildCanonicalThrowType(draft);
			ImGui::Spacing();
			ImGui::TextUnformatted(Localization::Pick("Preview", "预览"));
			DrawThrowTypeTagRow(preview, "ThrowPreview");

			if (ImAdd::Button(Localization::Pick("Apply", "应用"), ImVec2(90.0f, 0.0f)))
			{
				inOutThrowType = preview;
				changed = true;
				activePopup.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImAdd::Button(Localization::Pick("Cancel", "取消"), ImVec2(90.0f, 0.0f)))
			{
				activePopup.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		else if (activePopup == popupId)
		{
			activePopup.clear();
		}

		return changed;
	}

	template <typename T>
	void ReleaseComPtr(T*& ptr)
	{
		if (ptr)
		{
			ptr->Release();
			ptr = nullptr;
		}
	}

	struct EmbeddedImageTexture
	{
		ID3D11ShaderResourceView* Srv = nullptr;
		int Width = 0;
		int Height = 0;
		bool TriedLoad = false;
	};

	EmbeddedImageTexture& GetBonesShowcaseTextureCache()
	{
		static EmbeddedImageTexture cache{};
		return cache;
	}

	void ResetBonesShowcaseTextureCache()
	{
		EmbeddedImageTexture& cache = GetBonesShowcaseTextureCache();
		ReleaseComPtr(cache.Srv);
		cache.Width = 0;
		cache.Height = 0;
		cache.TriedLoad = false;
	}

	bool DecodeBase64ViaWinApi(const std::string& base64Text, std::vector<std::uint8_t>& outBytes)
	{
		if (base64Text.empty())
			return false;
		if (base64Text.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)()))
			return false;

		DWORD outSize = 0;
		if (!CryptStringToBinaryA(base64Text.c_str(), static_cast<DWORD>(base64Text.size()), CRYPT_STRING_BASE64_ANY, nullptr, &outSize, nullptr, nullptr))
			return false;

		outBytes.assign(outSize, 0u);
		if (!CryptStringToBinaryA(base64Text.c_str(), static_cast<DWORD>(base64Text.size()), CRYPT_STRING_BASE64_ANY, outBytes.data(), &outSize, nullptr, nullptr))
		{
			outBytes.clear();
			return false;
		}

		outBytes.resize(outSize);
		return true;
	}

	bool DecodePngViaWic(const std::vector<std::uint8_t>& pngBytes, std::vector<std::uint8_t>& outPixels, UINT& outWidth, UINT& outHeight)
	{
		outPixels.clear();
		outWidth = 0;
		outHeight = 0;
		if (pngBytes.empty())
			return false;

		const HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		const bool needUninit = SUCCEEDED(initHr);

		IWICImagingFactory* factory = nullptr;
		IWICStream* stream = nullptr;
		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;

		bool success = false;
		do
		{
			if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
				break;
			if (FAILED(factory->CreateStream(&stream)))
				break;
			if (pngBytes.size() > static_cast<size_t>((std::numeric_limits<DWORD>::max)()))
				break;
			if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(pngBytes.data()), static_cast<DWORD>(pngBytes.size()))))
				break;
			if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
				break;
			if (FAILED(decoder->GetFrame(0, &frame)))
				break;
			if (FAILED(factory->CreateFormatConverter(&converter)))
				break;
			if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom)))
				break;
			if (FAILED(converter->GetSize(&outWidth, &outHeight)))
				break;
			if (outWidth == 0 || outHeight == 0)
				break;

			const UINT stride = outWidth * 4u;
			const UINT totalSize = stride * outHeight;
			outPixels.assign(totalSize, 0u);
			if (FAILED(converter->CopyPixels(nullptr, stride, totalSize, outPixels.data())))
			{
				outPixels.clear();
				break;
			}

			success = true;
		}
		while (false);

		ReleaseComPtr(converter);
		ReleaseComPtr(frame);
		ReleaseComPtr(decoder);
		ReleaseComPtr(stream);
		ReleaseComPtr(factory);
		if (needUninit)
			CoUninitialize();

		return success;
	}

	bool CreateTextureFromRgba(ID3D11Device* device, const std::vector<std::uint8_t>& rgbaPixels, const UINT width, const UINT height, EmbeddedImageTexture& outTexture)
	{
		if (!device || rgbaPixels.empty() || width == 0 || height == 0)
			return false;

		D3D11_TEXTURE2D_DESC textureDesc{};
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData{};
		initData.pSysMem = rgbaPixels.data();
		initData.SysMemPitch = static_cast<UINT>(width * 4u);

		ID3D11Texture2D* texture = nullptr;
		if (FAILED(device->CreateTexture2D(&textureDesc, &initData, &texture)))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView* srv = nullptr;
		const HRESULT srvHr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
		texture->Release();
		if (FAILED(srvHr) || !srv)
			return false;

		outTexture.Srv = srv;
		outTexture.Width = static_cast<int>(width);
		outTexture.Height = static_cast<int>(height);
		return true;
	}

	const EmbeddedImageTexture* EnsureBonesShowcaseTexture(ID3D11Device* device)
	{
		EmbeddedImageTexture& cache = GetBonesShowcaseTextureCache();
		if (cache.Srv)
			return &cache;
		if (cache.TriedLoad || !device)
			return nullptr;

		cache.TriedLoad = true;
		const std::string& base64 = EmbeddedBonesShowcase::GetBonesShowcasePngBase64();

		std::vector<std::uint8_t> pngBytes{};
		if (!DecodeBase64ViaWinApi(base64, pngBytes))
			return nullptr;

		std::vector<std::uint8_t> rgbaPixels{};
		UINT width = 0;
		UINT height = 0;
		if (!DecodePngViaWic(pngBytes, rgbaPixels, width, height))
			return nullptr;

		if (!CreateTextureFromRgba(device, rgbaPixels, width, height, cache))
			return nullptr;

		return cache.Srv ? &cache : nullptr;
	}

	struct ShowcaseBonePoint
	{
		int BoneSlot = 0;
		float X = 0.0f;
		float Y = 0.0f;
	};

	inline constexpr float kShowcaseImageWidth = 1380.0f;
	inline constexpr float kShowcaseImageHeight = 978.0f;
	inline constexpr float kShowcaseCropMinX = 360.0f;
	inline constexpr float kShowcaseCropMinY = 40.0f;
	inline constexpr float kShowcaseCropMaxX = 1020.0f;
	inline constexpr float kShowcaseCropMaxY = 940.0f;
	inline constexpr std::array<ShowcaseBonePoint, Structs::AimBoneNames.size()> kShowcaseBonePoints = {
		ShowcaseBonePoint{ 0, 691.0f, 470.0f },  // Pelvis
		ShowcaseBonePoint{ 1, 690.0f, 382.0f },  // Spine
		ShowcaseBonePoint{ 2, 690.0f, 306.0f },  // Chest
		ShowcaseBonePoint{ 3, 690.0f, 210.0f },  // Neck
		ShowcaseBonePoint{ 4, 690.0f, 128.0f },  // Head
		ShowcaseBonePoint{ 5, 600.0f, 262.0f },  // L Shoulder
		ShowcaseBonePoint{ 6, 505.0f, 362.0f },  // L Elbow
		ShowcaseBonePoint{ 7, 437.0f, 458.0f },  // L Hand
		ShowcaseBonePoint{ 8, 781.0f, 262.0f },  // R Shoulder
		ShowcaseBonePoint{ 9, 874.0f, 362.0f },  // R Elbow
		ShowcaseBonePoint{ 10, 942.0f, 458.0f }, // R Hand
		ShowcaseBonePoint{ 11, 638.0f, 548.0f }, // L Thigh
		ShowcaseBonePoint{ 12, 626.0f, 666.0f }, // L Knee
		ShowcaseBonePoint{ 13, 622.0f, 884.0f }, // L Foot
		ShowcaseBonePoint{ 14, 742.0f, 548.0f }, // R Thigh
		ShowcaseBonePoint{ 15, 754.0f, 666.0f }, // R Knee
		ShowcaseBonePoint{ 16, 758.0f, 884.0f }  // R Foot
	};

	std::string BuildAimbotBoneMaskDisplayText(std::uint64_t mask)
	{
		mask &= Structs::AimAllBoneMask;
		std::string text{};
		for (size_t i = 0; i < Structs::AimBoneNames.size(); ++i)
		{
			const std::uint64_t bit = 1ull << static_cast<std::uint64_t>(i);
			if ((mask & bit) == 0ull)
				continue;

			if (!text.empty())
				text += ", ";
			text += LocalizedAimBoneName(static_cast<int>(i));
		}

		if (text.empty())
			return Localization::Pick("None", "未选择");
		return text;
	}

	enum class BonePickerTargetType : int
	{
		AimbotWeapon = 0,
		TriggerWeapon,
		TriggerSpecial,
		FlickWeapon,
		FlickSpecial
	};

	struct BonePickerPanelState
	{
		bool Open = false;
		BonePickerTargetType Target = BonePickerTargetType::AimbotWeapon;
		int Index = 0;
	};

	BonePickerPanelState& GetBonePickerPanelState()
	{
		static BonePickerPanelState state{};
		return state;
	}

	void OpenBonePickerPanel(const BonePickerTargetType target, const int index)
	{
		BonePickerPanelState& state = GetBonePickerPanelState();
		state.Open = true;
		state.Target = target;
		state.Index = index;
	}

	void CloseBonePickerPanel()
	{
		GetBonePickerPanelState().Open = false;
	}

	bool IsBonePickerPanelOpenFor(const BonePickerTargetType target, const int index)
	{
		const BonePickerPanelState& state = GetBonePickerPanelState();
		return state.Open && state.Target == target && state.Index == index;
	}

	bool ResolveBonePickerBinding(BonePickerPanelState& panel, std::uint64_t*& outMask, const char*& outGroupLabel, const char*& outName)
	{
		outMask = nullptr;
		outGroupLabel = "";
		outName = "";

		switch (panel.Target)
		{
		case BonePickerTargetType::AimbotWeapon:
			panel.Index = std::clamp(panel.Index, 0, Structs::AimWeapon_Count - 1);
			outMask = &config.Aim.WeaponProfiles[panel.Index].BoneMask;
			outGroupLabel = Localization::Pick("Aimbot Weapon", "自瞄武器");
			outName = LocalizedAimWeaponGroupName(panel.Index);
			return true;

		case BonePickerTargetType::TriggerWeapon:
			panel.Index = std::clamp(panel.Index, 0, Structs::AimWeapon_Count - 1);
			outMask = &config.Aim.TriggerProfiles[panel.Index].BoneMask;
			outGroupLabel = Localization::Pick("Trigger Weapon", "扳机武器");
			outName = LocalizedAimWeaponGroupName(panel.Index);
			return true;

		case BonePickerTargetType::TriggerSpecial:
			panel.Index = std::clamp(panel.Index, 0, Structs::TriggerSpecial_Count - 1);
			outMask = &config.Aim.TriggerSpecialProfiles[panel.Index].BoneMask;
			outGroupLabel = Localization::Pick("Trigger Special", "扳机特殊武器");
			outName = LocalizedTriggerSpecialWeaponName(panel.Index);
			return true;

		case BonePickerTargetType::FlickWeapon:
			panel.Index = std::clamp(panel.Index, 0, Structs::AimWeapon_Count - 1);
			outMask = &config.Aim.FlickProfiles[panel.Index].BoneMask;
			outGroupLabel = Localization::Pick("Flick Weapon", "甩枪武器");
			outName = LocalizedAimWeaponGroupName(panel.Index);
			return true;

		case BonePickerTargetType::FlickSpecial:
			panel.Index = std::clamp(panel.Index, 0, Structs::TriggerSpecial_Count - 1);
			outMask = &config.Aim.FlickSpecialProfiles[panel.Index].BoneMask;
			outGroupLabel = Localization::Pick("Flick Special", "甩枪特殊武器");
			outName = LocalizedTriggerSpecialWeaponName(panel.Index);
			return true;

		default:
			break;
		}

		return false;
	}

	void DrawBonePickerPanel(const ImVec2& menuWindowPos, const ImVec2& menuWindowSize)
	{
		BonePickerPanelState& panel = GetBonePickerPanelState();
		if (!panel.Open)
			return;

		std::uint64_t* maskPtr = nullptr;
		const char* groupLabel = "";
		const char* targetName = "";
		if (!ResolveBonePickerBinding(panel, maskPtr, groupLabel, targetName) || !maskPtr)
		{
			panel.Open = false;
			return;
		}

		std::uint64_t& mask = *maskPtr;
		mask &= Structs::AimAllBoneMask;
		const bool useAimbotFallback =
			panel.Target == BonePickerTargetType::AimbotWeapon ||
			panel.Target == BonePickerTargetType::FlickWeapon ||
			panel.Target == BonePickerTargetType::FlickSpecial;
		const std::uint64_t fallbackMask = useAimbotFallback
			? Structs::AimDefaultAimbotBoneMask
			: Structs::AimAllBoneMask;
		if (mask == 0ull)
			mask = fallbackMask;

		const float panelGap = 4.0f;
		const ImVec2 panelPos{ menuWindowPos.x + menuWindowSize.x + panelGap, menuWindowPos.y };
		const ImVec2 panelSize{ 500.0f, menuWindowSize.y };
		ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);

		const ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDecoration;

		ImGui::Begin("##BonePickerPanel", nullptr, windowFlags);
		ImGuiStyle& style = ImGui::GetStyle();

		const char* title = panel.Target == BonePickerTargetType::AimbotWeapon
			? Localization::Pick("Aimbot Bone Picker", "自瞄瞄点面板")
			: ((panel.Target == BonePickerTargetType::FlickWeapon || panel.Target == BonePickerTargetType::FlickSpecial)
				? Localization::Pick("Flick Bone Picker", "甩枪瞄点面板")
				: Localization::Pick("Trigger Bone Picker", "扳机瞄点面板"));
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.5f - ImGui::CalcTextSize(title).x * 0.5f);
		ImGui::Text("%s", title);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
		ImGui::BeginChild("BonePickerMain", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NoBackground);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		{
			ImGui::BeginChild("BonePickerHeader", ImVec2(0, ImGui::GetFrameHeight()), 0, ImGuiWindowFlags_NoBackground);
			{
				ImGui::Text("%s: %s", groupLabel, targetName);
				ImGui::SameLine();
				ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 86.0f - style.WindowPadding.x);
				if (ImAdd::Button(Localization::Pick("Close", "关闭"), ImVec2(86.0f, 0.0f)))
					panel.Open = false;
			}
			ImGui::EndChild();

			ImGui::SetCursorPosY(ImGui::GetFrameHeight());
			ImGui::BeginChild("BonePickerContent", ImVec2(0, ImGui::GetWindowHeight() - ImGui::GetFrameHeight() * 2.0f), ImGuiChildFlags_Border, ImGuiWindowFlags_NoBackground);
			{
				ImGui::TextDisabled("%s", Localization::Pick("Click body points to toggle bones.", "点击人物圆点以切换骨骼点。"));

				const EmbeddedImageTexture* texture = EnsureBonesShowcaseTexture(Overlay::device);
				if (!texture || !texture->Srv)
				{
					ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "%s", Localization::Pick("Failed to load showcase texture.", "骨骼示意图加载失败。"));
				}
				else
				{
					const float cropWidth = kShowcaseCropMaxX - kShowcaseCropMinX;
					const float cropHeight = kShowcaseCropMaxY - kShowcaseCropMinY;
					const float aspect = cropWidth / cropHeight;
					const ImVec2 avail = ImGui::GetContentRegionAvail();
					const float footerReserve = ImGui::GetFrameHeightWithSpacing() * 2.8f;
					const float maxImageHeight = (std::max)(150.0f, avail.y - footerReserve);
					const float imageWidth = (std::min)(avail.x, maxImageHeight * aspect);
					const float imageHeight = imageWidth / aspect;

					if (imageWidth > 10.0f && imageHeight > 10.0f)
					{
						const float startX = ImGui::GetCursorPosX() + (avail.x - imageWidth) * 0.5f;
						ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), startX));

						const ImVec2 imageSize{ imageWidth, imageHeight };
						const ImVec2 uv0{ kShowcaseCropMinX / kShowcaseImageWidth, kShowcaseCropMinY / kShowcaseImageHeight };
						const ImVec2 uv1{ kShowcaseCropMaxX / kShowcaseImageWidth, kShowcaseCropMaxY / kShowcaseImageHeight };
						ImGui::Image(reinterpret_cast<ImTextureID>(texture->Srv), imageSize, uv0, uv1);

						const ImVec2 imageMin = ImGui::GetItemRectMin();
						const ImVec2 imageMax = ImGui::GetItemRectMax();
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						drawList->AddRect(imageMin, imageMax, IM_COL32(255, 255, 255, 36), 8.0f, 0, 1.5f);

						const float pointRadius = 8.0f;
						for (const ShowcaseBonePoint& point : kShowcaseBonePoints)
						{
							const float nx = std::clamp((point.X - kShowcaseCropMinX) / cropWidth, 0.0f, 1.0f);
							const float ny = std::clamp((point.Y - kShowcaseCropMinY) / cropHeight, 0.0f, 1.0f);
							const ImVec2 center{
								imageMin.x + nx * imageSize.x,
								imageMin.y + ny * imageSize.y
							};

							ImGui::PushID(point.BoneSlot);
							ImGui::SetCursorScreenPos(ImVec2(center.x - pointRadius, center.y - pointRadius));
							ImGui::InvisibleButton("##BonePoint", ImVec2(pointRadius * 2.0f, pointRadius * 2.0f));
							const bool hovered = ImGui::IsItemHovered();
							if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
							{
								const std::uint64_t bit = 1ull << static_cast<std::uint64_t>(point.BoneSlot);
								if ((mask & bit) != 0ull)
									mask &= ~bit;
								else
									mask |= bit;

								if (mask == 0ull)
									mask = fallbackMask;
							}

							const std::uint64_t bit = 1ull << static_cast<std::uint64_t>(point.BoneSlot);
							const bool selected = (mask & bit) != 0ull;
							const ImU32 fillColor = selected
								? (hovered ? IM_COL32(95, 228, 95, 255) : IM_COL32(60, 200, 60, 240))
								: (hovered ? IM_COL32(245, 245, 245, 255) : IM_COL32(205, 205, 205, 225));
							drawList->AddCircleFilled(center, pointRadius, fillColor, 24);
							drawList->AddCircle(center, pointRadius, IM_COL32(18, 18, 18, 240), 24, 1.8f);

							if (hovered)
							{
								ImGui::BeginTooltip();
								ImGui::Text("%s", LocalizedAimBoneName(point.BoneSlot));
								ImGui::EndTooltip();
							}
							ImGui::PopID();
						}
					}
				}

				std::string summary = BuildAimbotBoneMaskDisplayText(mask);
				std::array<char, 512> summaryBuffer{};
				strncpy_s(summaryBuffer.data(), summaryBuffer.size(), summary.c_str(), _TRUNCATE);
				const std::string readonlyId = std::string("##BoneMaskPanelReadonly_") + std::to_string(static_cast<int>(panel.Target)) + "_" + std::to_string(panel.Index);
				ImGui::InputText(readonlyId.c_str(), summaryBuffer.data(), summaryBuffer.size(), ImGuiInputTextFlags_ReadOnly);
			}
			ImGui::EndChild();

			ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeight());
			ImGui::BeginChild("BonePickerFooter", ImVec2(0, 0), 0, ImGuiWindowFlags_NoBackground);
			{
				if (ImAdd::Button(Localization::Pick("Clear", "清空"), ImVec2(120.0f, 0.0f)))
					mask = Structs::BoneMaskFromBoneId(Structs::AimHeadBoneId);
				ImGui::SameLine();
				if (ImAdd::Button(Localization::Pick("Select All", "全选"), ImVec2(120.0f, 0.0f)))
					mask = Structs::AimAllBoneMask;
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();
		ImGui::End();
	}

	void DrawBoneSelectorControl(
		const BonePickerTargetType target,
		const int profileIndex,
		std::uint64_t& mask,
		const std::uint64_t fallbackMask,
		const char* sectionTitle)
	{
		mask &= Structs::AimAllBoneMask;
		if (mask == 0ull)
			mask = fallbackMask;

		ImAdd::SeparatorText(sectionTitle);

		const std::string summary = BuildAimbotBoneMaskDisplayText(mask);
		std::array<char, 512> summaryBuffer{};
		strncpy_s(summaryBuffer.data(), summaryBuffer.size(), summary.c_str(), _TRUNCATE);
		const std::string summaryId = std::string("##BoneMaskSummary_") + std::to_string(static_cast<int>(target)) + "_" + std::to_string(profileIndex);
		ImGui::InputText(summaryId.c_str(), summaryBuffer.data(), summaryBuffer.size(), ImGuiInputTextFlags_ReadOnly);

		const bool panelOpen = IsBonePickerPanelOpenFor(target, profileIndex);
		if (ImAdd::Button(panelOpen ? Localization::Pick("Hide Picker", "隐藏瞄点面板") : Localization::Pick("Select Bone Points", "选择骨骼点"), ImVec2(150.0f, 0.0f)))
		{
			if (panelOpen)
				CloseBonePickerPanel();
			else
				OpenBonePickerPanel(target, profileIndex);
		}
		ImGui::SameLine();
		if (ImAdd::Button(Localization::Pick("Select All", "全选"), ImVec2(120.0f, 0.0f)))
			mask = Structs::AimAllBoneMask;
	}

	void DrawAimbotBoneSelectorControl(const int weaponIndex, std::uint64_t& mask, const std::uint64_t fallbackMask)
	{
		DrawBoneSelectorControl(
			BonePickerTargetType::AimbotWeapon,
			weaponIndex,
			mask,
			fallbackMask,
			Localization::Pick("Aim Points", "瞄点")
		);
	}

	void DrawTriggerBoneSelectorControl(const BonePickerTargetType target, const int profileIndex, std::uint64_t& mask, const std::uint64_t fallbackMask)
	{
		DrawBoneSelectorControl(
			target,
			profileIndex,
			mask,
			fallbackMask,
			Localization::Pick("Trigger Points", "触发点")
		);
	}

	void DrawFlickBoneSelectorControl(const int weaponIndex, std::uint64_t& mask, const std::uint64_t fallbackMask)
	{
		DrawBoneSelectorControl(
			BonePickerTargetType::FlickWeapon,
			weaponIndex,
			mask,
			fallbackMask,
			Localization::Pick("Flick Points", "甩枪瞄点")
		);
	}

	void DrawFlickSpecialBoneSelectorControl(const int profileIndex, std::uint64_t& mask, const std::uint64_t fallbackMask)
	{
		DrawBoneSelectorControl(
			BonePickerTargetType::FlickSpecial,
			profileIndex,
			mask,
			fallbackMask,
			Localization::Pick("Flick Points", "甩枪瞄点")
		);
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
	ResetBonesShowcaseTextureCache();

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
	wc.lpszClassName = L"CCS2";

	RegisterClassEx(&wc);

	overlay = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		wc.lpszClassName,
		L"CCS2",
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
			m_MenuJustOpened = true;
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW);
		}
		else
		{
			m_MenuJustOpened = false;
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
    style.TabBorderSize     = 1;
    style.TabBarBorderSize  = 1;

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

    // Tab bars (Aimbot/Trigger and nested tabs) follow the same grayscale palette as main menu blocks.
    style.Colors[ImGuiCol_Tab]                  = ImAdd::HexToColorVec4(0x181818, 1.0f);
    style.Colors[ImGuiCol_TabHovered]           = ImAdd::HexToColorVec4(0x282828, 0.85f);
    style.Colors[ImGuiCol_TabActive]            = ImAdd::HexToColorVec4(0x282828, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused]         = ImAdd::HexToColorVec4(0x141414, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive]   = ImAdd::HexToColorVec4(0x202020, 1.0f);

	if (m_Tabs.empty())
	{
		m_iSelectedPage = 0;

		m_Tabs.push_back("Aim");     // MenuPage_Aiming
		m_Tabs.push_back("Visuals");    // MenuPage_Visuals
		m_Tabs.push_back("Radar");      // MenuPage_Radar
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
		"CCS2",
		&shouldRenderMenu,
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoDecoration
	);
	const ImVec2 menuWindowPos = ImGui::GetWindowPos();
	const ImVec2 menuWindowSize = ImGui::GetWindowSize();

	StyleMenu(io, style);

	OverlayFps = ImGui::GetIO().Framerate;

	ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - ImGui::CalcTextSize("CCS2").x / 2);
	ImGui::Text("CCS2");

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
					ImAdd::RadioFrame(LocalizedMainTabLabel(i), &m_iSelectedPage, i, i % 2 == 0, ImVec2(i == m_Tabs.size() - 1 ? -0.1f : RadioWidth, ImGui::GetWindowHeight()));
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
				ImGui::BeginChild("AimTabsRoot", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);
				{
					if (ImGui::BeginTabBar("AimSubTabs", ImGuiTabBarFlags_None))
					{
						if (ImGui::BeginTabItem(Localization::Pick("Aimbot", "自瞄")))
						{
							ImAdd::CheckBox("Aimbot##Enable", &config.Aim.Aimbot);
							
							if (config.Aim.Aimbot)
							{
								if (ProcInfo::KmboxInitialized)
								{
									if (ImGui::BeginTabBar("AimbotLayoutTabs", ImGuiTabBarFlags_None))
									{
										if (ImGui::BeginTabItem(Localization::Pick("General", "通用")))
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

											ImAdd::CheckBox("Aim Visible", &config.Aim.AimVisible);
											ImAdd::CheckBox("Aim Teammates", &config.Aim.AimFriendly);
											ImAdd::CheckBox("Block Aimbot When Flashed", &config.Aim.BlockAimbotWhenFlashed);
											ImAdd::SliderFloat("Deadzone (px)", &config.Aim.DeadzonePx, 0.0f, 6.0f);
											ImGui::TextDisabled("%s", Localization::Pick("Aimbot thread interval: 2ms (~500Hz).", "自瞄线程间隔：2ms (~500Hz)。"));
											ImGui::EndTabItem();
										}

										if (ImGui::BeginTabItem(Localization::Pick("Weapon Tabs", "武器分页")))
										{
											if (ImGui::BeginTabBar("AimbotWeaponTabs", ImGuiTabBarFlags_None))
											{
													for (int i = 0; i < Structs::AimWeapon_Count; ++i)
													{
														if (ImGui::BeginTabItem(LocalizedAimWeaponGroupName(i)))
														{
															config.Aim.WeaponProfileEditorIndex = i;
															Structs::AimWeaponProfile& profile = config.Aim.WeaponProfiles[i];
															const bool supportsRecoilAxis = i != Structs::AimWeapon_Pistol && i != Structs::AimWeapon_Shotgun;
															auto showHoverTip = [&](const char* en, const char* zh)
															{
																if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
																	return;
																ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f, 0.0f), ImVec2(560.0f, FLT_MAX));
																ImGui::BeginTooltip();
																ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
																ImGui::TextUnformatted(Localization::Pick(en, zh));
																ImGui::PopTextWrapPos();
																ImGui::EndTooltip();
															};

															ImAdd::SliderFloat(Localization::Pick("Profile FOV", "武器FOV"), &profile.Fov, 0.1f, 45.0f);
															showHoverTip(
																"Maximum lock radius for this weapon profile. Larger value allows targets farther from crosshair.",
																"该武器配置的最大锁定半径。数值越大，越容易锁到离准心更远的目标。"
															);
															ImAdd::SliderFloat(Localization::Pick("Profile Smooth", "武器平滑"), &profile.Smooth, 1.0f, 100.0f);
															showHoverTip(
																"Main speed controller. Higher value = slower and smoother; lower value = faster response.",
																"主速度控制项。值越高越慢越平滑；值越低响应越快。"
															);
															if (supportsRecoilAxis)
															{
																ImAdd::SliderFloat(Localization::Pick("Spray Axis Strength X", "连发轴向强度 X"), &profile.SprayAxisStrengthX, 0.50f, 2.50f);
																showHoverTip(
																	"Only active during sustained fire recoil control. Scales horizontal correction before smoothing.",
																	"仅在连发压枪时生效。用于放大/缩小横向修正，再进入平滑。"
																);
																ImAdd::SliderFloat(Localization::Pick("Spray Axis Strength Y", "连发轴向强度 Y"), &profile.SprayAxisStrengthY, 0.50f, 2.50f);
																showHoverTip(
																	"Only active during sustained fire recoil control. Scales vertical correction before smoothing.",
																	"仅在连发压枪时生效。用于放大/缩小纵向修正，再进入平滑。"
																);
																ImAdd::SliderFloat(Localization::Pick("Spray Axis Deadzone X", "连发轴向死区 X"), &profile.SprayAxisDeadzoneX, 0.0f, 1.0f);
																showHoverTip(
																	"If horizontal error is below this threshold, X movement is suppressed to reduce jitter.",
																	"当横向误差小于该阈值时，X 轴移动会被抑制，用于减少左右抖动。"
																);
																ImAdd::SliderFloat(Localization::Pick("Spray Axis Deadzone Y", "连发轴向死区 Y"), &profile.SprayAxisDeadzoneY, 0.0f, 1.0f);
																showHoverTip(
																	"If vertical error is below this threshold, Y movement is suppressed to reduce micro shake.",
																	"当纵向误差小于该阈值时，Y 轴移动会被抑制，用于减少上下微抖。"
																);
															}
															else
															{
																ImGui::TextDisabled("%s", Localization::Pick("Pistols and shotguns skip recoil-axis control.", "手枪和霰弹枪不启用后座轴向控制。"));
																showHoverTip(
																	"These weapon types do not use recoil-axis controls, so related settings are hidden.",
																	"这两类武器不启用后座轴向控制，因此相关设置会被隐藏。"
																);
															}

															ImAdd::SliderFloat(Localization::Pick("Curve Strength", "曲线强度"), &profile.CurveStrength, 0.0f, 0.85f);
															showHoverTip(
																"Adds a slight curved tracking path. Set 0 to disable.",
																"增加轻微弧线跟枪轨迹。设为 0 可关闭。"
															);
															const auto& targetStrategyItems = LocalizedTargetStrategyNames();
															ImAdd::Combo(Localization::Pick("Target Strategy", "目标策略"), &profile.TargetStrategy, targetStrategyItems.data(), static_cast<int>(targetStrategyItems.size()));
															showHoverTip(
																"Crosshair: nearest on screen. Distance: nearest in world. Hybrid: weighted combination.",
																"准星优先：屏幕距离最近。距离优先：世界距离最近。混合：两者加权。"
															);
															ImAdd::SliderInt(Localization::Pick("Target Switch Delay (ms)", "目标切换延时 (ms)"), &profile.TargetSwitchDelayMs, 0, 600);
															showHoverTip(
																"Delay before switching to a new target after current lock is lost.",
																"当前目标丢失后，切换到新目标前的等待时间。"
															);
															DrawAimbotBoneSelectorControl(i, profile.BoneMask, Structs::AimDefaultAimbotBoneMask);
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
									const char* disconnectedText = Localization::Pick("KMBOX not connected.", "KMBOX 未连接。");
									ImGui::SetCursorPos(
										ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) / 2 -
										ImGui::CalcTextSize(disconnectedText) / 2 + ImVec2(0, ImGui::GetFrameHeight())
									);
									ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", disconnectedText);
								}
							}

							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem(Localization::Pick("Trigger", "扳机")))
						{
							ImAdd::CheckBox("Trigger##Enable", &config.Aim.Trigger);
							
							if (config.Aim.Trigger)
							{
								if (ProcInfo::KmboxInitialized)
								{
									if (ImGui::BeginTabBar("TriggerLayoutTabs", ImGuiTabBarFlags_None))
									{
										if (ImGui::BeginTabItem(Localization::Pick("General", "通用")))
										{
											ImAdd::SeparatorText("Hotkeys");
											ImGui::TextDisabled("%s", Localization::Pick("Trigger only works while holding hotkey.", "扳机仅在按住热键时生效。"));
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
											const auto& triggerDetectModeItems = LocalizedTriggerDetectModeNames();
											ImAdd::Combo(Localization::Pick("Trigger Detect Mode", "扳机检测模式"), &config.Aim.TriggerDetectMode, triggerDetectModeItems.data(), static_cast<int>(triggerDetectModeItems.size()));
											ImAdd::SliderFloat("Unified Hitbox Radius (px)", &config.Aim.TriggerUnifiedHitboxRadiusPx, 0.5f, 40.0f);
											ImAdd::SliderFloat("Hitbox Scale", &config.Aim.TriggerHitboxScale, 0.25f, 3.0f);
											ImAdd::SliderFloat("Hitbox Add (px)", &config.Aim.TriggerHitboxAddPx, -20.0f, 40.0f);
											ImAdd::SliderFloat("Head Radius (px)", &config.Aim.TriggerHeadRadiusPx, 0.5f, 80.0f);

											ImAdd::SeparatorText("Safety");
											ImAdd::CheckBox("Block Trigger When Flashed", &config.Aim.BlockTriggerWhenFlashed);
											ImGui::TextDisabled("%s", Localization::Pick("Reloading / non-gun is always blocked.", "换弹 / 非枪械时始终阻止触发。"));

											ImAdd::SeparatorText("Hitbox Debug");
											ImAdd::CheckBox("Head Sphere Debug", &config.Aim.TriggerHeadSphereDebug);
											if (config.Aim.TriggerHitboxDebug)
											{
												ImGui::TextDisabled("%s", Localization::Pick("ESP draws 3D box per bone segment.", "ESP 会按骨骼段绘制 3D 盒体。"));
												ImAdd::SliderFloat("Debug Thickness", &config.Aim.TriggerHitboxDebugThickness, 0.5f, 4.0f);
												ImAdd::ColorEdit4("Hitbox Color", (float*)&config.Aim.TriggerHitboxDebugColor);
												ImAdd::ColorEdit4("Active Hitbox Color", (float*)&config.Aim.TriggerHitboxDebugActiveColor);
											}

											ImGui::TextDisabled("%s", Localization::Pick("Trigger thread interval: 2ms (~500Hz).", "扳机线程间隔：2ms (~500Hz)。"));
											ImGui::EndTabItem();
										}

										if (ImGui::BeginTabItem(Localization::Pick("Weapon Tabs", "武器分页")))
										{
											if (ImGui::BeginTabBar("TriggerWeaponTabs", ImGuiTabBarFlags_None))
											{
												for (int i = 0; i < Structs::AimWeapon_Count; ++i)
												{
													if (ImGui::BeginTabItem(LocalizedAimWeaponGroupName(i)))
													{
														config.Aim.TriggerProfileEditorIndex = i;
														Structs::TriggerWeaponProfile& profile = config.Aim.TriggerProfiles[i];
														ImAdd::SliderInt("Pre Fire Delay (ms)", &profile.PreFireDelayMs, 0, 600);
														ImAdd::SliderInt("Post Fire Interval (ms)", &profile.PostFireIntervalMs, 0, 1200);
														ImAdd::SliderInt("Timeout Force Fire (ms)", &profile.TimeoutForceFireMs, 0, 3000);
														DrawTriggerBoneSelectorControl(BonePickerTargetType::TriggerWeapon, i, profile.BoneMask, Structs::AimAllBoneMask);
														ImGui::EndTabItem();
													}
												}

												for (int i = 0; i < Structs::TriggerSpecial_Count; ++i)
												{
													if (ImGui::BeginTabItem(LocalizedTriggerSpecialWeaponName(i)))
													{
														config.Aim.TriggerSpecialEditorIndex = i;
														Structs::TriggerSpecialProfile& special = config.Aim.TriggerSpecialProfiles[i];
														ImAdd::SliderInt("Special Pre Fire Delay (ms)", &special.PreFireDelayMs, 0, 600);
														ImAdd::SliderInt("Special Post Fire Interval (ms)", &special.PostFireIntervalMs, 0, 1500);
														ImAdd::SliderInt("Special Timeout Force Fire (ms)", &special.TimeoutForceFireMs, 0, 3000);
														ImAdd::SliderInt("Special Hold Fire (ms)", &special.HoldFireMs, 0, 600);
														DrawTriggerBoneSelectorControl(BonePickerTargetType::TriggerSpecial, i, special.BoneMask, Structs::AimAllBoneMask);
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
									const char* disconnectedText = Localization::Pick("KMBOX not connected.", "KMBOX 未连接。");
									ImGui::SetCursorPos(
										ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - ImGui::GetFrameHeight()) / 2 -
										ImGui::CalcTextSize(disconnectedText) / 2 + ImVec2(0, ImGui::GetFrameHeight())
									);
									ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", disconnectedText);
								}
							}

							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem(Localization::Pick("Flick", "甩枪")))
						{
							ImAdd::CheckBox("Flick##Enable", &config.Aim.Flick);

							if (config.Aim.Flick)
							{
								if (ProcInfo::KmboxInitialized)
								{
									if (ImGui::BeginTabBar("FlickLayoutTabs", ImGuiTabBarFlags_None))
									{
										if (ImGui::BeginTabItem(Localization::Pick("General", "通用")))
										{
											ImAdd::SeparatorText("Hotkeys");
											ImAdd::KeyBindOptions flickMode = (ImAdd::KeyBindOptions)config.Aim.FlickKeyMode;
											ImAdd::KeyBind(Localization::Pick("Flick Key", "甩枪热键"), &config.Aim.FlickKey, 0, &flickMode);
											config.Aim.FlickKeyMode = (int)flickMode;

											ImAdd::SeparatorText("General");
											ImGui::TextDisabled("%s", Localization::Pick("Flick = aimbot + trigger cycles while hotkey is held.", "甩枪 = 按住热键时循环执行 自瞄 + 扳机。"));
											ImGui::TextDisabled("%s", Localization::Pick("Each cycle restarts after the configured restart interval.", "每轮结束后会按设定的重启间隔自动开始下一轮。"));
											ImGui::TextDisabled("%s", Localization::Pick("Flick thread interval: 2ms (~500Hz).", "甩枪线程间隔：2ms (~500Hz)。"));
											ImGui::EndTabItem();
										}

										if (ImGui::BeginTabItem(Localization::Pick("Weapon Tabs", "武器分页")))
										{
											if (ImGui::BeginTabBar("FlickWeaponTabs", ImGuiTabBarFlags_None))
											{
												for (int i = 0; i < Structs::AimWeapon_Count; ++i)
												{
													if (ImGui::BeginTabItem(LocalizedAimWeaponGroupName(i)))
													{
														config.Aim.FlickProfileEditorIndex = i;
														Structs::FlickWeaponProfile& profile = config.Aim.FlickProfiles[i];
														ImAdd::SliderFloat(Localization::Pick("Flick Smooth", "甩枪平滑"), &profile.Smooth, 1.0f, 100.0f);
														ImAdd::SliderInt(Localization::Pick("Max Flick Time (ms)", "最长甩枪时间 (ms)"), &profile.MaxFlickTimeMs, 10, 5000);
														ImAdd::SliderInt(Localization::Pick("Restart Interval (ms)", "重启甩枪间隔 (ms)"), &profile.RestartIntervalMs, 0, 5000);
														ImAdd::SliderFloat(Localization::Pick("Flick FOV", "甩枪FOV"), &profile.Fov, 0.1f, 60.0f);
														ImAdd::CheckBox(Localization::Pick("Dynamic FOV", "动态FOV"), &profile.DynamicFovEnabled);
														ImAdd::CheckBox(Localization::Pick("Autowall", "穿墙甩枪"), &profile.AutowallEnabled);
														ImAdd::CheckBox(Localization::Pick("Autowall Killshot Only", "仅可斩杀时穿墙甩枪"), &profile.AutowallKillshotOnly);
														DrawFlickBoneSelectorControl(i, profile.BoneMask, Structs::AimDefaultAimbotBoneMask);
														ImGui::EndTabItem();
													}
												}

												for (int i = 0; i < Structs::TriggerSpecial_Count; ++i)
												{
													if (ImGui::BeginTabItem(LocalizedTriggerSpecialWeaponName(i)))
													{
														config.Aim.FlickSpecialEditorIndex = i;
														Structs::FlickWeaponProfile& profile = config.Aim.FlickSpecialProfiles[i];
														ImAdd::SliderFloat(Localization::Pick("Flick Smooth", "甩枪平滑"), &profile.Smooth, 1.0f, 100.0f);
														if (i == Structs::TriggerSpecial_Revolver)
														{
															ImGui::TextDisabled("%s", Localization::Pick("R8 max aim time is fixed at 235ms.", "R8 最长瞄准时间固定为 235ms。"));
															ImAdd::SliderFloat(Localization::Pick("R8 Follow Smooth", "R8 跟枪平滑"), &profile.FollowSmooth, 1.0f, 100.0f);
														}
														else
														{
															ImAdd::SliderInt(Localization::Pick("Max Flick Time (ms)", "最长甩枪时间 (ms)"), &profile.MaxFlickTimeMs, 10, 5000);
														}
														ImAdd::SliderInt(Localization::Pick("Restart Interval (ms)", "重启甩枪间隔 (ms)"), &profile.RestartIntervalMs, 0, 5000);
														ImAdd::SliderFloat(Localization::Pick("Flick FOV", "甩枪FOV"), &profile.Fov, 0.1f, 60.0f);
														ImAdd::CheckBox(Localization::Pick("Dynamic FOV", "动态FOV"), &profile.DynamicFovEnabled);
														ImAdd::CheckBox(Localization::Pick("Autowall", "穿墙甩枪"), &profile.AutowallEnabled);
														ImAdd::CheckBox(Localization::Pick("Autowall Killshot Only", "仅可斩杀时穿墙甩枪"), &profile.AutowallKillshotOnly);
														DrawFlickSpecialBoneSelectorControl(i, profile.BoneMask, Structs::AimDefaultAimbotBoneMask);
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
									const char* disconnectedText = Localization::Pick("KMBOX not connected.", "KMBOX 未连接。");
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
				ImGui::BeginChild("Visuals", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);
				{

					if (ImGui::BeginTabBar("VisualsTopTabs"))
					{
						if (ImGui::BeginTabItem(Localization::Pick("ESP", "透视")))
						{
							ImAdd::CheckBox(Localization::Pick("ESP", "透视"), &config.Visuals.Enabled);
							ImGui::Spacing();
							ImGui::Separator();
							ImGui::Spacing();

							ImGui::BeginChild("EspSettingsPanel", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);
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

							// ImGui::BeginGroup();
							// {
							// 	ImAdd::CheckBox("Hitmarker", &config.Visuals.Hitmarker);
							// 	if (config.Visuals.Hitmarker)
							// 	{
							// 		ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
							// 		ImAdd::ColorEdit4("##HitmarkerColor", (float*)&config.Visuals.HitmarkerColor);
							// 	}
							// }

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

							ImGui::BeginGroup();
							{
								ImAdd::CheckBox("Spectator List", &config.Visuals.SpectatorList);
								if (config.Visuals.SpectatorList)
								{
									ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() * 2 - style.WindowPadding.x * 2);
									ImAdd::ColorEdit4("##SpectatorListColor", (float*)&config.Visuals.SpectatorListColor);
									ImAdd::SliderFloat("Spectator Card X", &config.Visuals.SpectatorListPanelPosX, 0.0f, 1.0f);
									ImAdd::SliderFloat("Spectator Card Y", &config.Visuals.SpectatorListPanelPosY, 0.0f, 1.0f);
								}
							}
								}
								else
								{
									ImGui::TextDisabled("%s", Localization::Pick("Enable ESP to configure these settings.", "启用透视后可配置左侧设置。"));
								}
							}
							ImGui::EndChild();
							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem(Localization::Pick("GHelper", "道具辅助")))
						{
							ImAdd::CheckBox(Localization::Pick("GHelper", "道具辅助"), &config.Visuals.GrenadeHelper);
							
							ImGui::Separator();
							ImGui::Spacing();

							ImGui::BeginChild("UtilityHelperPanel", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_None);
							{
						ImAdd::SeparatorText(Localization::Pick("GHelper Settings", "GHelper设置"));
						ImGui::TextWrapped("%s", Localization::Pick("Independent utility helper settings panel.", "独立道具辅助设置面板。"));
						ImGui::Spacing();

						if (config.Visuals.GrenadeHelper)
						{
							static char helperMapName[64] = "";
							static char helperSpotName[128] = "";
							static char helperSpotRemark[192] = "";
							static std::string helperRecordThrowType = "LB";
							static int helperSelectedRow = -1;
							static std::vector<GrenadeSpotEditorRow> helperRows{};
							static std::string helperStatus{};
							static int helperOpenSnapshotTypeFilter = -1;
							static bool helperScrollToSelectedRow = false;
							constexpr float kRecordAimDistance = 10000.0f;

							if (helperMapName[0] == '\0')
							{
								const std::string suggestedMap = esp.GetSuggestedGrenadeMapName();
								strncpy_s(helperMapName, suggestedMap.c_str(), _TRUNCATE);
								helperStatus = esp.GetGrenadeStatus();
								helperRows = esp.GetGrenadeSpotEditorRows();
								helperSelectedRow = helperRows.empty() ? -1 : 0;
							}

							auto rowVisibleByOpenFilter = [&](const GrenadeSpotEditorRow& row)
							{
								return helperOpenSnapshotTypeFilter < 0 || std::clamp(row.TypeIndex, 0, 4) == helperOpenSnapshotTypeFilter;
							};

							auto buildVisibleRows = [&]()
							{
								std::vector<int> visible{};
								visible.reserve(helperRows.size());
								for (int rowIndex = 0; rowIndex < static_cast<int>(helperRows.size()); ++rowIndex)
								{
									if (rowVisibleByOpenFilter(helperRows[static_cast<std::size_t>(rowIndex)]))
										visible.push_back(rowIndex);
								}
								return visible;
							};

							auto alignSelectionToVisible = [&](const std::vector<int>& visibleRows)
							{
								if (visibleRows.empty())
								{
									helperSelectedRow = -1;
									return;
								}

								const bool validIndex = helperSelectedRow >= 0 && helperSelectedRow < static_cast<int>(helperRows.size());
								if (!validIndex)
								{
									helperSelectedRow = visibleRows.front();
									return;
								}

								for (const int visibleIndex : visibleRows)
								{
									if (visibleIndex == helperSelectedRow)
										return;
								}

								helperSelectedRow = visibleRows.front();
							};

							if (m_MenuJustOpened)
							{
								helperRows = esp.GetGrenadeSpotEditorRows();
								helperOpenSnapshotTypeFilter = -1;
								int detectedTypeIndex = -1;
								if (esp.DetectCurrentGrenadeTypeIndex(detectedTypeIndex))
									helperOpenSnapshotTypeFilter = std::clamp(detectedTypeIndex, 0, 4);

								const int focusedSpotId = esp.GetCurrentGrenadeFocusedSpotId();
								helperSelectedRow = -1;
								if (focusedSpotId > 0)
								{
									for (int rowIndex = 0; rowIndex < static_cast<int>(helperRows.size()); ++rowIndex)
									{
										const GrenadeSpotEditorRow& row = helperRows[static_cast<std::size_t>(rowIndex)];
										if (row.Id == focusedSpotId)
										{
											if (helperOpenSnapshotTypeFilter >= 0 && helperOpenSnapshotTypeFilter != std::clamp(row.TypeIndex, 0, 4))
												helperOpenSnapshotTypeFilter = std::clamp(row.TypeIndex, 0, 4);
											helperSelectedRow = rowIndex;
											break;
										}
									}
								}

								const std::vector<int> visibleRowsOnOpen = buildVisibleRows();
								alignSelectionToVisible(visibleRowsOnOpen);
								helperScrollToSelectedRow = helperSelectedRow >= 0;
								m_MenuJustOpened = false;
							}

							const char* grenadeTypeItems[] = {
								Localization::Pick("Smoke", "烟"),
								Localization::Pick("Flash", "闪"),
								Localization::Pick("HE", "雷"),
								Localization::Pick("Decoy", "饵"),
								Localization::Pick("Molotov", "火")
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
							config.Visuals.GrenadeHelperTopHintFontSize = std::clamp(config.Visuals.GrenadeHelperTopHintFontSize, 20.0f, 100.0f);
							ImAdd::SliderFloat(Localization::Pick("Throw Hint Size", "投掷提示大小"), &config.Visuals.GrenadeHelperTopHintFontSize, 20.0f, 100.0f);

							ImAdd::SeparatorText(Localization::Pick("Record Spot", "记录点位"));
							ImGui::InputText(Localization::Pick("Spot Name", "点位名称"), helperSpotName, IM_ARRAYSIZE(helperSpotName));
							ImGui::InputText(Localization::Pick("Remark", "备注"), helperSpotRemark, IM_ARRAYSIZE(helperSpotRemark));

							helperRecordThrowType = BuildCanonicalThrowType(ParseThrowType(helperRecordThrowType));
							ImGui::TextUnformatted(Localization::Pick("Throw Type", "投掷方式"));
							DrawThrowTypeTagRow(helperRecordThrowType, "RecordThrowPreview");
							if (ImAdd::Button(Localization::Pick("Set Throw Type", "设置投掷方式"), ImVec2(132.0f, 0.0f)))
								ImGui::OpenPopup("RecordThrowTypePopup");
							DrawThrowTypePopupEditor("RecordThrowTypePopup", helperRecordThrowType);

							if (ImAdd::Button(Localization::Pick("Record Spot", "记录点位"), ImVec2(110.0f, 0.0f)))
							{
								std::string status{};
								if (esp.RecordCurrentGrenadeSpot(
									helperMapName,
									helperSpotName,
									helperRecordThrowType,
									helperSpotRemark,
									kRecordAimDistance,
									config.Visuals.GrenadeHelperManualTypeOverride,
									config.Visuals.GrenadeHelperManualType,
									status))
								{
									helperRows = esp.GetGrenadeSpotEditorRows();
									const std::vector<int> visibleRows = buildVisibleRows();
									alignSelectionToVisible(visibleRows);
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

							ImAdd::SeparatorText(Localization::Pick("Manage Spots", "管理点位"));
							ImGui::InputText(Localization::Pick("Map Name", "地图名"), helperMapName, IM_ARRAYSIZE(helperMapName));

							if (ImAdd::Button(Localization::Pick("Reload Spots", "重载点位"), ImVec2(110.0f, 0.0f)))
							{
								std::string status{};
								if (esp.ReloadGrenadeSpots(helperMapName, status))
								{
									helperRows = esp.GetGrenadeSpotEditorRows();
									const std::vector<int> visibleRows = buildVisibleRows();
									alignSelectionToVisible(visibleRows);
								}
								helperStatus = status;
							}
							ImGui::SameLine();
							if (ImAdd::Button(Localization::Pick("Save List", "保存列表"), ImVec2(110.0f, 0.0f)))
							{
								std::string status{};
								if (esp.SaveGrenadeSpotEditorRows(helperMapName, helperRows, status))
								{
									helperRows = esp.GetGrenadeSpotEditorRows();
									const std::vector<int> visibleRows = buildVisibleRows();
									alignSelectionToVisible(visibleRows);
								}
								helperStatus = status;
							}
							ImGui::SameLine();
							if (ImAdd::Button(Localization::Pick("Delete Selected", "删除选中"), ImVec2(110.0f, 0.0f)))
							{
								if (helperSelectedRow >= 0 && helperSelectedRow < static_cast<int>(helperRows.size()))
								{
									helperRows.erase(helperRows.begin() + helperSelectedRow);
									const std::vector<int> visibleRows = buildVisibleRows();
									alignSelectionToVisible(visibleRows);
								}
							}

							if (!helperStatus.empty())
								ImGui::TextWrapped("%s", helperStatus.c_str());

							const std::vector<int> visibleRowsNow = buildVisibleRows();
							alignSelectionToVisible(visibleRowsNow);

							if (helperOpenSnapshotTypeFilter >= 0 && helperOpenSnapshotTypeFilter < IM_ARRAYSIZE(grenadeTypeItems))
							{
								ImGui::TextDisabled(
									"%s: %s",
									Localization::Pick("Open Snapshot Filter", "呼出快照过滤"),
									grenadeTypeItems[helperOpenSnapshotTypeFilter]
								);
							}

							const ImVec2 listSize(0.0f, ImGui::GetTextLineHeightWithSpacing() * 12.0f);
							const ImGuiTableFlags tableFlags =
								ImGuiTableFlags_Borders |
								ImGuiTableFlags_RowBg |
								ImGuiTableFlags_Resizable |
								ImGuiTableFlags_ScrollY;
							if (ImGui::BeginTable("GHelperSpotTable", 4, tableFlags, listSize))
							{
								ImGui::TableSetupColumn(Localization::Pick("Type", "类型"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
								ImGui::TableSetupColumn(Localization::Pick("Throw", "投掷"), ImGuiTableColumnFlags_WidthFixed, 160.0f);
								ImGui::TableSetupColumn(Localization::Pick("Name", "名称"), ImGuiTableColumnFlags_WidthStretch, 0.40f);
								ImGui::TableSetupColumn(Localization::Pick("Remark", "备注"), ImGuiTableColumnFlags_WidthStretch, 0.45f);
								ImGui::TableHeadersRow();

								for (const int rowIndex : visibleRowsNow)
								{
									GrenadeSpotEditorRow& row = helperRows[static_cast<std::size_t>(rowIndex)];
									row.ThrowType = BuildCanonicalThrowType(ParseThrowType(row.ThrowType));
									const bool rowSelected = helperSelectedRow == rowIndex;

									ImGui::PushID(row.Id != 0 ? row.Id : rowIndex);
									ImGui::TableNextRow();
									if (rowSelected)
									{
										ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 120, 72, 120));
										ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(255, 120, 72, 88));
									}

									ImGui::TableSetColumnIndex(0);
									if (ImGui::Selectable("##SpotSelect", rowSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
										helperSelectedRow = rowIndex;
									if (rowSelected && helperScrollToSelectedRow)
									{
										ImGui::SetScrollHereY(0.35f);
										helperScrollToSelectedRow = false;
									}
									ImGui::SameLine();
									int typeIndex = std::clamp(row.TypeIndex, 0, 4);
									if (ImGui::Combo("##Type", &typeIndex, grenadeTypeItems, IM_ARRAYSIZE(grenadeTypeItems)))
										row.TypeIndex = typeIndex;

									ImGui::TableSetColumnIndex(1);
									DrawThrowTypeTagRow(row.ThrowType, "RowThrowPreview", false);

									ImGui::TableSetColumnIndex(2);
									char rowNameBuffer[128]{};
									strncpy_s(rowNameBuffer, row.Name.c_str(), _TRUNCATE);
									ImGui::SetNextItemWidth(-FLT_MIN);
									if (ImGui::InputText("##Name", rowNameBuffer, IM_ARRAYSIZE(rowNameBuffer)))
										row.Name = rowNameBuffer;

									ImGui::TableSetColumnIndex(3);
									char rowRemarkBuffer[192]{};
									strncpy_s(rowRemarkBuffer, row.Remark.c_str(), _TRUNCATE);
									ImGui::SetNextItemWidth(-FLT_MIN);
									if (ImGui::InputText("##Remark", rowRemarkBuffer, IM_ARRAYSIZE(rowRemarkBuffer)))
										row.Remark = rowRemarkBuffer;

									ImGui::PopID();
								}

								ImGui::EndTable();
							}

							if (helperSelectedRow >= 0 && helperSelectedRow < static_cast<int>(helperRows.size()))
							{
								GrenadeSpotEditorRow& selected = helperRows[static_cast<std::size_t>(helperSelectedRow)];
								selected.ThrowType = BuildCanonicalThrowType(ParseThrowType(selected.ThrowType));

								ImGui::Spacing();
								ImAdd::SeparatorText(Localization::Pick("Selected Spot Throw Type", "选中点位投掷方式"));
								DrawThrowTypeTagRow(selected.ThrowType, "SelectedThrowPreview");
								if (ImAdd::Button(Localization::Pick("Set Selected Throw Type", "设置选中投掷方式"), ImVec2(160.0f, 0.0f)))
									ImGui::OpenPopup("SelectedThrowTypePopup");
								DrawThrowTypePopupEditor("SelectedThrowTypePopup", selected.ThrowType);
							}
						}
						else
						{
							ImGui::TextDisabled("%s", Localization::Pick("Enable GHelper Utility to configure helper settings.", "启用GHelper后可配置右侧道具辅助设置。"));
						}
					}
							ImGui::EndChild();
							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}
				}
				ImGui::EndChild();
			}
			else if (m_iSelectedPage == MenuPage_Radar)
			{
				ImGui::BeginChild("Radar", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_MenuBar);
				{
					if (ImGui::BeginMenuBar())
					{
						ImGui::Text("%s", Localization::Pick("Radar", "雷达"));
						ImGui::EndMenuBar();
					}

					RadarRuntimeState runtime = radarBridge.GetRuntimeState();
					static std::chrono::steady_clock::time_point radarLinkCopiedUntil{};

					ImAdd::SeparatorText(Localization::Pick("Enable", "启用"));
					ImAdd::CheckBox(Localization::Pick("Enable Radar Bridge", "启用雷达桥接"), &config.Radar.Enabled);

					ImAdd::SeparatorText(Localization::Pick("Endpoint", "地址"));
					char hostBuffer[128]{};
					strncpy_s(hostBuffer, config.Radar.Host.c_str(), _TRUNCATE);
					if (ImGui::InputText(Localization::Pick("Host", "主机"), hostBuffer, IM_ARRAYSIZE(hostBuffer)))
						config.Radar.Host = hostBuffer;

					int staticPort = config.Radar.StaticPort;
					if (ImGui::InputInt(Localization::Pick("Static Port", "静态端口"), &staticPort))
						config.Radar.StaticPort = std::clamp(staticPort, 1, 65535);

					int ingestPort = config.Radar.IngestPort;
					if (ImGui::InputInt(Localization::Pick("WebSocket Port", "WebSocket端口"), &ingestPort))
						config.Radar.IngestPort = std::clamp(ingestPort, 1, 65535);

					ImAdd::SeparatorText(Localization::Pick("Link", "链接"));
					ImGui::TextWrapped("%s: %s", Localization::Pick("Radar Link", "雷达链接"), runtime.staticUrl.empty() ? "-" : runtime.staticUrl.c_str());
					ImGui::BeginDisabled(runtime.staticUrl.empty());
					if (ImAdd::Button(Localization::Pick("Copy Link", "复制链接"), ImVec2(130.0f, 0.0f)))
					{
						ImGui::SetClipboardText(runtime.staticUrl.c_str());
						radarLinkCopiedUntil = std::chrono::steady_clock::now() + std::chrono::seconds(2);
					}
					ImGui::EndDisabled();
					if (std::chrono::steady_clock::now() < radarLinkCopiedUntil)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("%s", Localization::Pick("Copied", "已复制"));
					}

					if (config.DebugRadar)
					{
						ImAdd::SeparatorText(Localization::Pick("Runtime Status", "运行状态"));
						ImGui::Text("%s: %s", Localization::Pick("Enabled", "启用"), runtime.enabled ? Localization::Pick("Yes", "是") : Localization::Pick("No", "否"));
						ImGui::Text("%s: %s", Localization::Pick("Connected", "已连接"), runtime.connected ? Localization::Pick("Yes", "是") : Localization::Pick("No", "否"));
						ImGui::TextWrapped("%s: %s", Localization::Pick("WebSocket URL", "WebSocket地址"), runtime.webSocketUrl.empty() ? "-" : runtime.webSocketUrl.c_str());
						ImGui::TextWrapped("%s: %s", Localization::Pick("Last Error", "最近错误"), runtime.lastError.empty() ? "-" : runtime.lastError.c_str());
						ImGui::Text("%s: %llu", Localization::Pick("Last Push (epoch ms)", "最后推送(毫秒时间戳)"), static_cast<unsigned long long>(runtime.lastPushEpochMs));
					}
				}
				ImGui::EndChild();
			}
			else if (m_iSelectedPage == MenuPage_Config)
			{
				ImGui::BeginChild("Configs", ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_MenuBar);
				{
					if (ImGui::BeginMenuBar()) {
						ImGui::Text("%s", Localization::Pick("Configs", "配置"));
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
					if (ImGui::BeginListBox(Localization::Pick("Config list", "配置列表")))
					{
						if (configFiles.empty())
						{
							ImGui::Selectable(Localization::Pick("No configs found", "未找到配置"), false, ImGuiSelectableFlags_Disabled);
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
					ImGui::InputText(Localization::Pick("Config Name", "配置名"), configName, IM_ARRAYSIZE(configName));

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
						ImGui::Text("%s", Localization::Pick("Info", "信息"));
						ImGui::EndMenuBar();
					}

					int languageIndex = std::clamp(config.Language, 0, 1);
					const char* languageItems[] = {
						Localization::Pick("English", "英文"),
						Localization::Pick("Chinese", "中文")
					};
					if (ImAdd::Combo(Localization::Pick("Language", "语言"), &languageIndex, languageItems, IM_ARRAYSIZE(languageItems)))
					{
						languageIndex = std::clamp(languageIndex, 0, 1);
						config.Language = languageIndex;
						Localization::CurrentLanguage = static_cast<Localization::Language>(config.Language);

						if (!config.SaveToFile("configs/config.json"))
							LOG_ERROR("Failed to persist language setting to configs/config.json");
					}

					ImAdd::SeparatorText(Localization::Pick("Hardware", "硬件"));

					const SDK::CoreCache coreCache = sdk.GetCoreCache();
					const bool dmaRuntimeHealthy =
						ProcInfo::DmaInitialized ||
						Globals::ClientBase != 0 ||
						coreCache.IsValid;

					ImGui::Text("%s", Localization::Pick("DMA:", "DMA:"));
					ImGui::SameLine();
					ImGui::TextColored(
						dmaRuntimeHealthy ? ImVec4(0, 1, 0, 1)/* green */ : ImVec4(1, 0, 0, 1)/* red */,
						"%s",
						dmaRuntimeHealthy ? Localization::Pick("Connected", "已连接") : Localization::Pick("Disconnected", "未连接")
					);

					ImGui::Text("%s", Localization::Pick("KMBOX:", "KMBOX:"));
					ImGui::SameLine();
					ImGui::TextColored(
						ProcInfo::KmboxInitialized ? ImVec4(0, 1, 0, 1)/* green */: ImVec4(1, 0, 0, 1)/* red */,
						"%s",
						ProcInfo::KmboxInitialized ? Localization::Pick("Connected", "已连接") : Localization::Pick("Disconnected", "未连接")
					);

					ImAdd::SeparatorText(Localization::Pick("KMBOX", "KMBOX"));
					static bool kmboxUiInitialized = false;
					static char kmboxIpBuffer[64]{};
					static char kmboxUuidBuffer[64]{};
					static int kmboxPortInput = 0;
					static std::string kmboxActionStatus{};

					if (!kmboxUiInitialized || m_MenuJustOpened)
					{
						strncpy_s(kmboxIpBuffer, config.Kmbox.Ip.c_str(), _TRUNCATE);
						strncpy_s(kmboxUuidBuffer, config.Kmbox.Uuid.c_str(), _TRUNCATE);
						kmboxPortInput = std::clamp(static_cast<int>(config.Kmbox.Port), 0, 65535);
						kmboxUiInitialized = true;
					}

					ImAdd::CheckBox(Localization::Pick("Enable KMBOX", "启用KMBOX"), &config.Kmbox.Enabled);
					if (ImGui::InputText(Localization::Pick("KMBOX IP", "KMBOX 地址"), kmboxIpBuffer, IM_ARRAYSIZE(kmboxIpBuffer)))
						config.Kmbox.Ip = kmboxIpBuffer;
					if (ImGui::InputInt(Localization::Pick("KMBOX Port", "KMBOX 端口"), &kmboxPortInput))
					{
						kmboxPortInput = std::clamp(kmboxPortInput, 0, 65535);
						config.Kmbox.Port = static_cast<unsigned short>(kmboxPortInput);
					}
					if (ImGui::InputText(Localization::Pick("KMBOX UUID", "KMBOX UUID"), kmboxUuidBuffer, IM_ARRAYSIZE(kmboxUuidBuffer)))
						config.Kmbox.Uuid = kmboxUuidBuffer;

					if (ImAdd::Button(Localization::Pick("Connect KMBOX", "连接KMBOX"), ImVec2(130.0f, 0.0f)))
					{
						config.Kmbox.Ip = TrimAscii(kmboxIpBuffer);
						config.Kmbox.Uuid = TrimAscii(kmboxUuidBuffer);
						config.Kmbox.Port = static_cast<unsigned short>(std::clamp(kmboxPortInput, 0, 65535));
						const int ret = Kmbox.InitDevice(config.Kmbox.Ip, config.Kmbox.Port, config.Kmbox.Uuid);
						ProcInfo::KmboxInitialized = (ret == 0);
						if (ret == 0)
						{
							config.Kmbox.Enabled = true;
							kmboxActionStatus = Localization::Pick("KMBOX connected successfully.", "KMBOX连接成功。");
						}
						else
						{
							kmboxActionStatus = Localization::Pick("KMBOX connect failed, code=", "KMBOX连接失败，错误码=") + std::to_string(ret);
						}
					}

					ImGui::BeginDisabled(!ProcInfo::KmboxInitialized);
					if (ImAdd::Button(Localization::Pick("Move Test", "移动测试"), ImVec2(190.0f, 0.0f)))
					{
						const int ret = Kmbox.Mouse.Move(100, 100);
						kmboxActionStatus = (ret == 0)
							? Localization::Pick("Move test sent: x=100 y=100.", "移动测试已发送：x=100 y=100。")
							: (Localization::Pick("Move test failed, code=", "移动测试失败，错误码=") + std::to_string(ret));
					}
					ImGui::EndDisabled();

					if (!kmboxActionStatus.empty())
						ImGui::TextWrapped("%s", kmboxActionStatus.c_str());


					ImAdd::SeparatorText(Localization::Pick("Game", "游戏"));

					ImGui::Text("%s", Localization::Pick("Client:", "客户端:"));
					ImGui::SameLine();
					ImGui::Text("0x%llx", Globals::ClientBase);


					ImAdd::SeparatorText(Localization::Pick("Debug", "调试"));
					ImAdd::CheckBox(Localization::Pick("Radar Runtime Debug", "雷达运行状态调试"), &config.DebugRadar);
					if (ImAdd::CheckBox(Localization::Pick("Enable Debug Thread", "启用Debug线程"), &config.DebugEnabled) && !config.DebugEnabled)
					{
						config.DebugPerf = false;
						config.DebugTrigger = false;
						config.DebugVisCheck = false;
						config.DebugAutowall = false;
						config.DebugSpectatorList = false;
						config.Aim.TriggerHitboxDebug = false;
					}

					if (config.DebugEnabled)
					{
						ImAdd::CheckBox(Localization::Pick("Enable Trigger Hitbox Debug", "启用扳机碰撞体调试"), &config.Aim.TriggerHitboxDebug);
						ImAdd::CheckBox(Localization::Pick("Perf Debug Output", "性能调试输出"), &config.DebugPerf);
						ImAdd::CheckBox(Localization::Pick("Trigger Debug Output", "扳机调试输出"), &config.DebugTrigger);
						ImAdd::CheckBox(Localization::Pick("Render World", "世界渲染"), &config.DebugVisCheck);
						ImAdd::CheckBox(Localization::Pick("Autowall Debug Output", "穿墙调试输出"), &config.DebugAutowall);
						ImAdd::CheckBox(Localization::Pick("Spectator Debug Text", "观战名单调试文本"), &config.DebugSpectatorList);

						if (config.DebugVisCheck)
						{
							ImGui::TextDisabled("%s", Localization::Pick("Render World uses full cached map triangles while debug is on.", "调试开启时世界渲染会使用完整缓存地图三角形。"));

							config.Visuals.VisCheckDebugMaxDistance = std::clamp(config.Visuals.VisCheckDebugMaxDistance, 300.0f, 12000.0f);
							ImAdd::SliderFloat(
								Localization::Pick("Render World Distance", "世界渲染距离"),
								&config.Visuals.VisCheckDebugMaxDistance,
								300.0f,
								12000.0f);

							config.Visuals.VisCheckDebugMaxItems = std::clamp(config.Visuals.VisCheckDebugMaxItems, 32, 5000);
							ImAdd::SliderInt(
								Localization::Pick("Render World Max Triangles", "世界渲染最大三角形数量"),
								&config.Visuals.VisCheckDebugMaxItems,
								32,
								5000);

							ImAdd::ColorEdit4(
								Localization::Pick("Render World Color", "世界渲染颜色"),
								(float*)&config.Visuals.VisCheckDebugColor);
						}
					}

					PerfDebug::SetDebugOptions(config.DebugEnabled, config.DebugPerf, config.DebugTrigger, config.DebugVisCheck, config.DebugAutowall);
					PerfDebug::SyncDebugThread();

					ImAdd::SeparatorText(Localization::Pick("Cheat", "功能"));

					ImGui::Text(Localization::Pick("Overlay FPS: %.2f", "叠加层 FPS: %.2f"), OverlayFps);
					ImGui::Text(
						Localization::Pick("Host INSERT: %s", "主机 INSERT: %s"),
						IsHostKeyDown(VK_INSERT) ? Localization::Pick("Down", "按下") : Localization::Pick("Up", "抬起")
					);
					ImGui::Text(
						Localization::Pick("Host LMB: %s", "主机 LMB: %s"),
						IsHostKeyDown(VK_LBUTTON) ? Localization::Pick("Down", "按下") : Localization::Pick("Up", "抬起")
					);
					ImGui::Text(
						Localization::Pick("Host RMB: %s", "主机 RMB: %s"),
						IsHostKeyDown(VK_RBUTTON) ? Localization::Pick("Down", "按下") : Localization::Pick("Up", "抬起")
					);
					ImGui::Text(
						Localization::Pick("Host X1: %s", "主机 X1: %s"),
						IsHostKeyDown(VK_XBUTTON1) ? Localization::Pick("Down", "按下") : Localization::Pick("Up", "抬起")
					);
					ImGui::Text(
						Localization::Pick("Host X2: %s", "主机 X2: %s"),
						IsHostKeyDown(VK_XBUTTON2) ? Localization::Pick("Down", "按下") : Localization::Pick("Up", "抬起")
					);

					float buttonWidth = 100.0f;
					float buttonSpacing = 20.0f;
					ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 2 * buttonWidth - buttonSpacing) / 2);

					if (ImAdd::Button(Localization::Pick("Open folder", "打开目录"), ImVec2(buttonWidth, 0)))
					{
						ShellExecuteA(nullptr, "open", "explorer.exe", ".\\", nullptr, SW_SHOW);
					}

					ImGui::SameLine();

					if (ImAdd::Button(Localization::Pick("Unload", "卸载"), ImVec2(buttonWidth, 0)))
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
			const char* buildText = StartupStatus::GetBuildText(Localization::IsChinese());
			const char* expiryText = StartupStatus::GetExpiryText(Localization::IsChinese());
			ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowPos() + style.FramePadding, ImGui::GetColorU32(ImGuiCol_Text), buildText);
			ImGui::GetWindowDrawList()->AddText(ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowWidth() - ImGui::CalcTextSize(expiryText).x - style.FramePadding.x, style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), expiryText);
		}
		ImGui::EndChild();
	}

	ImGui::EndChild();
	ImGui::End();
	DrawBonePickerPanel(menuWindowPos, menuWindowSize);
}
