#include <Pch.hpp>
#include <SDK.hpp>
#include "ESP.hpp"
#include <array>
#include <cfloat>
#include <unordered_map>
#include <unordered_set>

namespace
{
    struct BoneDataRaw
    {
        Vector3 Position{};
        std::uint8_t Padding[0x14]{};
    };

    constexpr int kMaxControllers = 64;
    constexpr int kAliveLifeStateA = 0;
    constexpr int kAliveLifeStateB = 256;
    constexpr int kHeadBone = 6;

    constexpr std::array<int, 18> kTrackedBones = {
        0, 2, 4, 5, 6,
        8, 9, 10,
        13, 14, 15,
        22, 23, 24,
        25, 26, 27,
        28
    };

    constexpr std::array<std::pair<int, int>, 17> kBoneLinks = {
        std::pair{ 0, 2 },
        std::pair{ 2, 4 },
        std::pair{ 4, 5 },
        std::pair{ 5, 6 },

        std::pair{ 4, 8 },
        std::pair{ 8, 9 },
        std::pair{ 9, 10 },

        std::pair{ 4, 13 },
        std::pair{ 13, 14 },
        std::pair{ 14, 15 },

        std::pair{ 0, 22 },
        std::pair{ 22, 23 },
        std::pair{ 23, 24 },

        std::pair{ 0, 25 },
        std::pair{ 25, 26 },
        std::pair{ 26, 27 },
        std::pair{ 27, 28 }
    };

    std::string WeaponIdToName(const int weaponId)
    {
        switch (weaponId)
        {
        case 1: return "Deagle";
        case 2: return "Dual Berettas";
        case 3: return "Five-Seven";
        case 4: return "Glock-18";
        case 7: return "AK-47";
        case 8: return "AUG";
        case 9: return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "Galil AR";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "PP-Bizon";
        case 27: return "MAG-7";
        case 28: return "Negev";
        case 29: return "Sawed-Off";
        case 30: return "Tec-9";
        case 31: return "Zeus x27";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "Nova";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 42: return "Knife";
        case 43: return "Flashbang";
        case 44: return "HE Grenade";
        case 45: return "Smoke";
        case 46: return "Molotov";
        case 47: return "Decoy";
        case 48: return "Incendiary";
        case 49: return "C4";
        case 57: return "Healthshot";
        case 59: return "Knife (T)";
        case 60: return "M4A1-S";
        case 61: return "USP-S";
        case 63: return "CZ75 Auto";
        case 64: return "R8 Revolver";
        default: return {};
        }
    }

    float DistanceSquared3D(const Vector3& a, const Vector3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    bool IsLikelyUserAddress(const uint64_t address)
    {
        return address > 0x10000ULL && address < 0x00007FFFFFFFFFFFULL;
    }

    struct SampledEntityData
    {
        int HandleIndex = 0;
        uint64_t Controller = 0;

        uint32_t PawnHandle = 0;
        uint64_t Pawn = 0;

        int Health = 0;
        int Team = 0;
        int LifeState = 0;
        int MaxHealth = 100;

        uint64_t SceneNode = 0;
        Vector3 OldOrigin{};
        Vector3 AbsOrigin{};
        bool HasAbsOrigin = false;
        Vector3 ViewOffset{ 0.0f, 0.0f, 64.0f };
        uint64_t BoneArray = 0;

        int Armor = 0;
        bool IsScoped = false;
        float FlashDuration = 0.0f;
        bool RefreshStatus = false;
    };
}

void ESP::RenderWatermark(ImDrawList* drawList) const
{
    if (!config.Visuals.Watermark)
        return;

    const std::string text = "Cosmic ESP";
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

    const ImVec2 basePos = ImVec2(12.0f, 12.0f);
    const ImVec2 bgMin = basePos - ImVec2(6.0f, 4.0f);
    const ImVec2 bgMax = basePos + textSize + ImVec2(6.0f, 4.0f);

    drawList->AddRectFilled(bgMin, bgMax, IM_COL32(18, 18, 18, 165), 4.0f);
    drawList->AddRect(bgMin, bgMax, IM_COL32(255, 255, 255, 55), 4.0f);
    drawList->AddText(basePos, ToImColor(config.Visuals.WatermarkColor), text.c_str());
}

void ESP::RenderPlayer(ImDrawList* drawList, const PlayerEspSnapshot& player) const
{
    const ImU32 boxColor = GetBoxColor(player.IsVisible);

    if (config.Visuals.Box)
    {
        drawList->AddRect(
            player.BoxMin,
            player.BoxMax,
            boxColor,
            0.0f,
            0,
            1.3f
        );
    }

    if (config.Visuals.Health)
    {
        const float boxHeight = player.BoxMax.y - player.BoxMin.y;
        const float healthRatio = std::clamp(static_cast<float>(player.Health) / static_cast<float>((std::max)(player.MaxHealth, 1)), 0.0f, 1.0f);

        const ImVec2 healthBgMin(player.BoxMin.x - 6.0f, player.BoxMin.y);
        const ImVec2 healthBgMax(player.BoxMin.x - 3.0f, player.BoxMax.y);
        const ImVec2 healthFillMin(player.BoxMin.x - 6.0f, player.BoxMax.y - boxHeight * healthRatio);
        const ImVec2 healthFillMax(player.BoxMin.x - 3.0f, player.BoxMax.y);

        const ImU32 healthColor = IM_COL32(
            static_cast<int>((1.0f - healthRatio) * 255.0f),
            static_cast<int>(healthRatio * 255.0f),
            80,
            255
        );

        drawList->AddRectFilled(healthBgMin, healthBgMax, IM_COL32(20, 20, 20, 185));
        drawList->AddRectFilled(healthFillMin, healthFillMax, healthColor);
        drawList->AddRect(healthBgMin, healthBgMax, IM_COL32(0, 0, 0, 220));
    }

    if (config.Visuals.Name)
    {
        const std::string nameText = player.Name.empty() ? "Unknown" : player.Name;
        const ImVec2 textSize = ImGui::CalcTextSize(nameText.c_str());

        const ImVec2 textPos(
            player.BoxMin.x + ((player.BoxMax.x - player.BoxMin.x) - textSize.x) * 0.5f,
            player.BoxMin.y - textSize.y - 3.0f
        );

        drawList->AddText(textPos, ToImColor(config.Visuals.NameColor), nameText.c_str());
    }

    if (config.Visuals.Weapon && !player.WeaponName.empty())
    {
        const ImVec2 textSize = ImGui::CalcTextSize(player.WeaponName.c_str());
        const ImVec2 textPos(
            player.BoxMin.x + ((player.BoxMax.x - player.BoxMin.x) - textSize.x) * 0.5f,
            player.BoxMax.y + 2.0f
        );

        drawList->AddText(textPos, ToImColor(config.Visuals.WeaponColor), player.WeaponName.c_str());
    }

    if (config.Visuals.Bones)
    {
        RenderSkeleton(drawList, player, ToImColor(config.Visuals.BonesColor));
    }

    struct StatusLine
    {
        std::string Text{};
        ImU32 Color = IM_COL32(220, 220, 220, 255);
    };

    std::vector<StatusLine> statusLines{};
    statusLines.reserve(4);

    if (player.IsScoped)
        statusLines.push_back({ "Scoped", IM_COL32(220, 220, 220, 255) });

    if (player.FlashDuration > 0.01f)
        statusLines.push_back({ "Flashed", IM_COL32(255, 214, 120, 255) });

    if (config.Visuals.Armor)
        statusLines.push_back({ "AR:" + std::to_string(player.Armor), ToImColor(config.Visuals.ArmorColor) });

    if (config.Visuals.Money)
        statusLines.push_back({ "$" + std::to_string(player.Money), ToImColor(config.Visuals.MoneyColor) });

    float lineOffset = 0.0f;
    for (const StatusLine& line : statusLines)
    {
        const ImVec2 textPos(player.BoxMax.x + 4.0f, player.BoxMin.y + lineOffset);
        drawList->AddText(textPos, line.Color, line.Text.c_str());
        lineOffset += ImGui::GetFontSize() + 1.0f;
    }
}

void ESP::RenderSkeleton(ImDrawList* drawList, const PlayerEspSnapshot& player, const ImU32 color) const
{
    if (player.Bones.empty())
        return;

    auto getBone = [&](const int index) -> const BonePoint*
    {
        for (const BonePoint& bone : player.Bones)
        {
            if (bone.Index == index)
                return &bone;
        }

        return nullptr;
    };

    for (const auto& [from, to] : kBoneLinks)
    {
        const BonePoint* fromBone = getBone(from);
        const BonePoint* toBone = getBone(to);
        if (!fromBone || !toBone)
            continue;

        if (!fromBone->OnScreen || !toBone->OnScreen)
            continue;

        drawList->AddLine(fromBone->Screen.ToImVec2(), toBone->Screen.ToImVec2(), color, 1.0f);
    }
}

void ESP::RenderC4(ImDrawList* drawList, const C4Snapshot& c4) const
{
    if (!c4.Valid || !c4.BombTicking)
        return;
    if (!config.Visuals.C4 && !config.Visuals.Defuser)
        return;

    const std::string site = (c4.BombSite == 0) ? "A" : ((c4.BombSite == 1) ? "B" : "?");
    const float shownTime = c4.TimeRemaining > 0.0f ? c4.TimeRemaining : c4.TimerLength;
    const std::string text = "C4 [" + site + "]  Timer:" + std::to_string(static_cast<int>(shownTime)) + "s";

    ImVec2 textPos(20.0f, 42.0f);

    if (c4.OnScreen)
        textPos = ImVec2(c4.Screen.x + 8.0f, c4.Screen.y - 16.0f);

    if (config.Visuals.C4)
        drawList->AddText(textPos, ToImColor(config.Visuals.C4Color), text.c_str());

    if (config.Visuals.Defuser && c4.BeingDefused)
    {
        ImVec2 defusePos = textPos;
        if (config.Visuals.C4)
            defusePos += ImVec2(0.0f, ImGui::GetFontSize() + 1.0f);

        drawList->AddText(defusePos, ToImColor(config.Visuals.DefuserColor), "Defusing");
    }
}

bool ESP::BuildBoneData(const uint64_t boneArray, PlayerEspSnapshot& inOutSnapshot) const
{
    if (!boneArray || !IsLikelyUserAddress(boneArray))
        return false;

    std::array<BoneDataRaw, kTrackedBones.size()> rawBones{};
    const auto scatter = mem.CreateScatterHandle();
    if (!scatter)
        return false;

    for (size_t i = 0; i < kTrackedBones.size(); ++i)
    {
        const uint64_t boneAddress = boneArray + static_cast<uint64_t>(kTrackedBones[i]) * Offsets::Layout::BoneStride;
        mem.AddScatterReadRequest(scatter, boneAddress, &rawBones[i], sizeof(BoneDataRaw));
    }

    mem.ExecuteReadScatter(scatter);
    mem.CloseScatterHandle(scatter);

    inOutSnapshot.Bones.clear();
    inOutSnapshot.Bones.reserve(kTrackedBones.size());

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    bool hasHead = false;
    bool anyOnScreen = false;

    for (size_t i = 0; i < kTrackedBones.size(); ++i)
    {
        const int boneIndex = kTrackedBones[i];
        const BoneDataRaw& rawBone = rawBones[i];

        BonePoint point{};
        point.Index = boneIndex;
        point.World = rawBone.Position;
        point.OnScreen = sdk.WorldToScreen(point.World, point.Screen);

        if (boneIndex == kHeadBone)
        {
            inOutSnapshot.HeadPosition = point.World;
            hasHead = true;
        }

        if (point.OnScreen)
        {
            anyOnScreen = true;
            minX = (std::min)(minX, point.Screen.x);
            minY = (std::min)(minY, point.Screen.y);
            maxX = (std::max)(maxX, point.Screen.x);
            maxY = (std::max)(maxY, point.Screen.y);
        }

        inOutSnapshot.Bones.push_back(point);
    }

    if (!hasHead)
    {
        inOutSnapshot.HeadPosition = inOutSnapshot.Origin + Vector3{ 0.0f, 0.0f, 72.0f };
    }

    if (!anyOnScreen)
        return false;

    const float width = maxX - minX;
    const float height = maxY - minY;
    if (width < 3.0f || height < 3.0f)
        return false;

    const float paddingX = (std::max)(width * 0.08f, 3.0f);
    const float paddingY = (std::max)(height * 0.06f, 3.0f);

    inOutSnapshot.BoxMin = ImVec2(minX - paddingX, minY - paddingY);
    inOutSnapshot.BoxMax = ImVec2(maxX + paddingX, maxY + paddingY);

    return true;
}

C4Snapshot ESP::ReadC4Snapshot() const
{
    C4Snapshot snapshot{};
    if (!Globals::ClientBase || !Offsets::Client::dwPlantedC4)
        return snapshot;

    const uint64_t plantedC4 = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwPlantedC4);
    if (!plantedC4)
        return snapshot;

    snapshot.Valid = true;
    if (Offsets::Schema::m_bBombTicking)
        snapshot.BombTicking = mem.Read<bool>(plantedC4 + Offsets::Schema::m_bBombTicking);
    if (Offsets::Schema::m_bBeingDefused)
        snapshot.BeingDefused = mem.Read<bool>(plantedC4 + Offsets::Schema::m_bBeingDefused);
    if (Offsets::Schema::m_nBombSite)
        snapshot.BombSite = mem.Read<int>(plantedC4 + Offsets::Schema::m_nBombSite);
    if (Offsets::Schema::m_flC4Blow)
        snapshot.BlowTime = mem.Read<float>(plantedC4 + Offsets::Schema::m_flC4Blow);
    if (Offsets::Schema::m_flTimerLength)
        snapshot.TimerLength = mem.Read<float>(plantedC4 + Offsets::Schema::m_flTimerLength);
    if (Offsets::Schema::m_flDefuseCountDown)
        snapshot.DefuseCountDown = mem.Read<float>(plantedC4 + Offsets::Schema::m_flDefuseCountDown);

    snapshot.TimeRemaining = snapshot.TimerLength;

    if (Offsets::Client::dwGlobalVars && snapshot.BlowTime > 0.0f)
    {
        const uint64_t globalVars = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGlobalVars);
        if (globalVars)
        {
            const float currentTimeCandidates[] = {
                mem.Read<float>(globalVars + 0x30),
                mem.Read<float>(globalVars + 0x34),
                mem.Read<float>(globalVars + 0x38)
            };

            for (const float candidate : currentTimeCandidates)
            {
                if (candidate <= 0.0f || candidate > 20000.0f)
                    continue;

                const float remaining = snapshot.BlowTime - candidate;
                if (remaining >= 0.0f && remaining <= 90.0f)
                {
                    snapshot.TimeRemaining = remaining;
                    break;
                }
            }
        }
    }

    if (Offsets::Schema::m_pGameSceneNode && Offsets::Schema::m_vecAbsOrigin)
    {
        const uint64_t sceneNode = mem.Read<uint64_t>(plantedC4 + Offsets::Schema::m_pGameSceneNode);
        if (IsLikelyUserAddress(sceneNode))
            snapshot.Position = mem.Read<Vector3>(sceneNode + Offsets::Schema::m_vecAbsOrigin);
    }

    snapshot.OnScreen = sdk.WorldToScreen(snapshot.Position, snapshot.Screen);

    return snapshot;
}

bool ESP::IsAlive(const int health, const int lifeState) const
{
    if (health <= 0 || health > 200)
        return false;

    return lifeState == kAliveLifeStateA || lifeState == kAliveLifeStateB;
}

ImU32 ESP::GetBoxColor(const bool isVisible) const
{
    if (!config.Visuals.VisibleCheck)
        return ToImColor(config.Visuals.BoxColor);

    if (isVisible)
        return ToImColor(config.Visuals.BoxColorVisible);

    return IM_COL32(255, 255, 255, 255);
}

ImU32 ESP::ToImColor(const ImVec4& color) const
{
    return ImGui::ColorConvertFloat4ToU32(color);
}

std::string ESP::ReadPlayerName(const uint64_t controller) const
{
    if (!controller || !Offsets::Schema::m_iszPlayerName)
        return {};

    char nameBuffer[128]{};
    if (!mem.Read(controller + Offsets::Schema::m_iszPlayerName, nameBuffer, sizeof(nameBuffer)))
        return {};

    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    std::string name(nameBuffer);
    name.erase(std::remove_if(name.begin(), name.end(), [](char c)
    {
        const unsigned char value = static_cast<unsigned char>(c);
        return value < 0x20 && value != '\t';
    }), name.end());

    return name;
}

int ESP::ReadMoney(const uint64_t controller) const
{
    if (!controller || !Offsets::Schema::m_pInGameMoneyServices || !Offsets::Schema::m_iAccount)
        return 0;

    const uint64_t moneyServices = mem.Read<uint64_t>(controller + Offsets::Schema::m_pInGameMoneyServices);
    if (!moneyServices || !IsLikelyUserAddress(moneyServices))
        return 0;

    return mem.Read<int>(moneyServices + Offsets::Schema::m_iAccount);
}

std::string ESP::ReadWeaponName(const uint64_t pawn) const
{
    if (!pawn || !Offsets::Schema::m_pWeaponServices || !Offsets::Schema::m_hActiveWeapon)
        return {};
    if (!Offsets::Schema::m_AttributeManager || !Offsets::Schema::m_iItemDefinitionIndex)
        return {};

    const uint64_t weaponServices = mem.Read<uint64_t>(pawn + Offsets::Schema::m_pWeaponServices);
    if (!weaponServices || !IsLikelyUserAddress(weaponServices))
        return {};

    const uint32_t activeWeaponHandle = mem.Read<uint32_t>(weaponServices + Offsets::Schema::m_hActiveWeapon);
    if (!activeWeaponHandle)
        return {};

    const uint64_t weapon = sdk.ResolveEntityFromHandle(activeWeaponHandle);
    if (!weapon || !IsLikelyUserAddress(weapon))
        return {};

    const int weaponId = mem.Read<int>(weapon + Offsets::Schema::m_AttributeManager + Offsets::Schema::m_iItemDefinitionIndex);
    return WeaponIdToName(weaponId);
}

void ESP::UpdateVisCheckState()
{
    ConsumeMapLoadResult();

    m_VisCheckEnabled = config.Visuals.Enabled && config.Visuals.VisibleCheck;
    if (!m_VisCheckEnabled)
    {
        m_VisCheck.reset();
        m_CurrentMapName.clear();
        m_CurrentOptPath.clear();
        m_LastPolledMapName.clear();
        m_ActiveMapRequestId = 0;
        m_MapStatus = "Map Status: (Disabled)";
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_LastMapPoll.time_since_epoch().count() == 0 ||
        now - m_LastMapPoll >= std::chrono::milliseconds(1000))
    {
        m_LastPolledMapName = sdk.GetCurrentMapName();
        PerfDebug::RecordMapPoll(!m_LastPolledMapName.empty());
        m_LastMapPoll = now;
    }

    const std::string& mapName = m_LastPolledMapName;
    if (mapName.empty())
    {
        m_MapStatus = "Map Status: (No Map)";
        return;
    }

    if (m_VisCheck && mapName == m_CurrentMapName)
    {
        m_MapStatus = BuildMapStatus(mapName, "Loaded");
        return;
    }

    if (!m_VisCheck && mapName == m_CurrentMapName && m_CurrentOptPath.empty())
    {
        m_MapStatus = BuildMapStatus(mapName, "Not Found");
        return;
    }

    for (const PendingMapLoad& pending : m_PendingMapLoads)
    {
        if (pending.RequestId == m_ActiveMapRequestId && pending.MapName == mapName)
            return;
    }

    const std::string optPath = ResolveOptPath(mapName);
    if (optPath.empty())
    {
        m_VisCheck.reset();
        m_CurrentMapName = mapName;
        m_CurrentOptPath.clear();
        m_MapStatus = BuildMapStatus(mapName, "Not Found");
        return;
    }

    RequestMapLoad(mapName, optPath);
}

void ESP::RequestMapLoad(const std::string& mapName, const std::string& optPath)
{
    PendingMapLoad pending{};
    pending.RequestId = ++m_NextMapRequestId;
    pending.MapName = mapName;
    pending.OptPath = optPath;
    pending.Future = std::async(std::launch::async, [optPath]()
    {
        auto visCheck = std::make_unique<VisCheck>(optPath);
        if (visCheck && visCheck->IsReady())
            return visCheck;

        return std::unique_ptr<VisCheck>{};
    });

    m_ActiveMapRequestId = pending.RequestId;
    m_MapStatus = BuildMapStatus(mapName, "Loading");
    m_PendingMapLoads.push_back(std::move(pending));
}

void ESP::ConsumeMapLoadResult()
{
    for (size_t index = 0; index < m_PendingMapLoads.size();)
    {
        PendingMapLoad& pending = m_PendingMapLoads[index];
        if (!pending.Future.valid())
        {
            m_PendingMapLoads.erase(m_PendingMapLoads.begin() + static_cast<long long>(index));
            continue;
        }

        if (pending.Future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            ++index;
            continue;
        }

        std::unique_ptr<VisCheck> loadedVisCheck = pending.Future.get();
        const bool requestIsCurrent = pending.RequestId == m_ActiveMapRequestId;

        if (requestIsCurrent)
        {
            if (loadedVisCheck)
            {
                m_VisCheck = std::move(loadedVisCheck);
                m_CurrentMapName = pending.MapName;
                m_CurrentOptPath = pending.OptPath;
                m_MapStatus = BuildMapStatus(pending.MapName, "Loaded");
            }
            else
            {
                m_VisCheck.reset();
                m_CurrentMapName = pending.MapName;
                m_CurrentOptPath = pending.OptPath;
                m_MapStatus = BuildMapStatus(pending.MapName, "Load Failed");
            }
        }

        m_PendingMapLoads.erase(m_PendingMapLoads.begin() + static_cast<long long>(index));
    }
}

std::string ESP::ResolveOptPath(const std::string& mapName) const
{
    if (mapName.empty())
        return {};

    const std::string fileName = mapName + ".opt";
    std::vector<std::filesystem::path> candidates{};
    candidates.reserve(64);

    auto appendCandidates = [&](std::filesystem::path base)
    {
        std::error_code ec{};
        for (int depth = 0; depth < 6 && !base.empty(); ++depth)
        {
            candidates.push_back(base / "maps" / fileName);
            candidates.push_back(base / "Maps" / fileName);
            candidates.push_back(base / "project-d" / "maps" / fileName);
            candidates.push_back(base / "project-d" / "Maps" / fileName);
            candidates.push_back(base / fileName);

            const std::filesystem::path parent = base.parent_path();
            if (parent == base || parent.empty())
                break;
            base = parent;
        }
    };

    std::error_code cwdError{};
    const std::filesystem::path cwd = std::filesystem::current_path(cwdError);
    if (!cwdError)
        appendCandidates(cwd);

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) != 0)
        appendCandidates(std::filesystem::path(modulePath).parent_path());

    for (const std::filesystem::path& candidate : candidates)
    {
        if (candidate.empty())
            continue;

        std::error_code existsError{};
        if (std::filesystem::exists(candidate, existsError) && !existsError)
            return candidate.string();
    }

    return {};
}

std::string ESP::BuildMapStatus(const std::string& mapName, const char* suffix) const
{
    std::string status = "Map Status: ";
    status += mapName.empty() ? "(Unknown)" : (mapName + ".opt");
    status += " (";
    status += suffix;
    status += ")";
    return status;
}

bool ESP::CheckVisibility(const Vector3& src, const Vector3& dst) const
{
    if (!m_VisCheck)
        return false;

    constexpr float maxDistance = 5000.0f;
    constexpr float maxDistanceSqr = maxDistance * maxDistance;
    if (DistanceSquared3D(src, dst) > maxDistanceSqr)
        return false;

    const auto start = std::chrono::steady_clock::now();
    const bool isVisible = m_VisCheck->IsPointVisible(src, dst);
    const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    if (durationUs >= 0)
        PerfDebug::RecordVisCheck(static_cast<std::uint64_t>(durationUs), isVisible);

    return isVisible;
}

void ESP::EnsureSamplerStarted()
{
    bool expected = false;
    if (!m_SamplerStarted.compare_exchange_strong(expected, true))
        return;

    std::thread([this]()
    {
        SamplerLoop();
    }).detach();
}

void ESP::UpdateRoundEpoch(const uint64_t localPawn, const bool localAlive)
{
    const auto now = std::chrono::steady_clock::now();
    if (m_LastRoundEpochTick.time_since_epoch().count() == 0)
        m_LastRoundEpochTick = now;

    bool shouldAdvanceEpoch = false;
    if (localPawn && m_LastRoundLocalPawn && localPawn != m_LastRoundLocalPawn)
    {
        shouldAdvanceEpoch = true;
    }
    else if (localAlive && !m_LastRoundLocalAlive)
    {
        if (now - m_LastRoundEpochTick > std::chrono::seconds(8))
            shouldAdvanceEpoch = true;
    }

    if (shouldAdvanceEpoch)
    {
        ++m_RoundEpoch;
        m_LastRoundEpochTick = now;
        m_PawnRuntimeCache.clear();
        m_LastFreezePeriod = false;
        m_LastFreezeEndTick = {};
    }

    if (localPawn)
        m_LastRoundLocalPawn = localPawn;
    m_LastRoundLocalAlive = localAlive;
}

void ESP::SamplerLoop()
{
    constexpr auto kSampleInterval = std::chrono::milliseconds(4); // 250Hz

    while (Globals::Running)
    {
        const auto cycleStart = std::chrono::steady_clock::now();

        RenderFrame sampledFrame{};
        sampledFrame.MapStatus = m_MapStatus;
        SampleFrame(sampledFrame);

        {
            std::lock_guard lock(m_RenderFrameMutex);
            m_RenderFrame = std::move(sampledFrame);
        }

        const auto sampleUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - cycleStart
        ).count();
        if (sampleUs >= 0)
            PerfDebug::RecordEspSampleFrame(static_cast<std::uint64_t>(sampleUs));

        const auto elapsed = std::chrono::steady_clock::now() - cycleStart;
        if (elapsed < kSampleInterval)
            std::this_thread::sleep_for(kSampleInterval - elapsed);
    }
}

bool ESP::SampleFrame(RenderFrame& outFrame)
{
    UpdateVisCheckState();
    outFrame.MapStatus = m_MapStatus;

    if (!config.Visuals.Enabled)
        return true;

    const SDK::CoreCache core = sdk.GetCoreCache();
    if (!core.IsValid || !core.LocalPawn || !core.EntityList)
        return true;

    struct LocalFields
    {
        int Team = 0;
        int Health = 0;
        int LifeState = 0;
        Vector3 Origin{};
        Vector3 ViewOffset{};
    } local{};

    const auto localScatter = mem.CreateScatterHandle();
    if (!localScatter)
        return false;

    if (Offsets::Schema::m_iTeamNum)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_iTeamNum, &local.Team, sizeof(local.Team));
    if (Offsets::Schema::m_iHealth)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_iHealth, &local.Health, sizeof(local.Health));
    if (Offsets::Schema::m_lifeState)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_lifeState, &local.LifeState, sizeof(local.LifeState));
    if (Offsets::Schema::m_vOldOrigin)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_vOldOrigin, &local.Origin, sizeof(local.Origin));
    if (Offsets::Schema::m_vecViewOffset)
        mem.AddScatterReadRequest(localScatter, core.LocalPawn + Offsets::Schema::m_vecViewOffset, &local.ViewOffset, sizeof(local.ViewOffset));

    mem.ExecuteReadScatter(localScatter);
    mem.CloseScatterHandle(localScatter);

    Vector3 localViewOffset = { 0.0f, 0.0f, 64.0f };
    if (std::abs(local.ViewOffset.x) > 0.001f || std::abs(local.ViewOffset.y) > 0.001f || std::abs(local.ViewOffset.z) > 0.001f)
        localViewOffset = local.ViewOffset;

    const Vector3 localEyePosition = local.Origin + localViewOffset;
    const int localTeam = local.Team;

    UpdateRoundEpoch(core.LocalPawn, IsAlive(local.Health, local.LifeState));
    const auto now = std::chrono::steady_clock::now();

    bool freezePeriod = false;
    bool freezeValid = false;
    if (Offsets::Client::dwGameRules && Offsets::Schema::m_bFreezePeriod)
    {
        const uint64_t gameRules = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGameRules);
        if (IsLikelyUserAddress(gameRules))
        {
            freezePeriod = mem.Read<bool>(gameRules + Offsets::Schema::m_bFreezePeriod);
            freezeValid = true;
        }
    }

    if (freezeValid)
    {
        if (freezePeriod)
        {
            if (!m_LastFreezePeriod &&
                (m_LastRoundEpochTick.time_since_epoch().count() == 0 ||
                    now - m_LastRoundEpochTick > std::chrono::seconds(8)))
            {
                ++m_RoundEpoch;
                m_LastRoundEpochTick = now;
                m_PawnRuntimeCache.clear();
            }

            m_LastFreezePeriod = true;
        }
        else if (m_LastFreezePeriod)
        {
            m_LastFreezePeriod = false;
            m_LastFreezeEndTick = now;
        }
    }

    const uint64_t controllerChunk = mem.Read<uint64_t>(core.EntityList + Offsets::EntityList::ListStart);
    if (!IsLikelyUserAddress(controllerChunk))
        return true;

    std::array<uint64_t, kMaxControllers + 1> controllerPointers{};
    if (const auto controllerScatter = mem.CreateScatterHandle())
    {
        for (int index = 1; index <= kMaxControllers; ++index)
        {
            const uint64_t entryAddress = controllerChunk + static_cast<uint64_t>(index) * Offsets::EntityList::EntryStride;
            mem.AddScatterReadRequest(controllerScatter, entryAddress, &controllerPointers[index], sizeof(uint64_t));
        }

        mem.ExecuteReadScatter(controllerScatter);
        mem.CloseScatterHandle(controllerScatter);
    }
    else
    {
        return false;
    }

    std::vector<SampledEntityData> entities{};
    entities.reserve(kMaxControllers);

    for (int index = 1; index <= kMaxControllers; ++index)
    {
        const uint64_t controller = controllerPointers[index];
        if (!IsLikelyUserAddress(controller))
            continue;

        SampledEntityData data{};
        data.HandleIndex = index;
        data.Controller = controller;
        entities.push_back(data);
    }

    outFrame.ResolvedControllers = static_cast<std::uint32_t>(entities.size());
    if (entities.empty())
        return true;

    const uint32_t pawnHandlePrimaryOffset = Offsets::Schema::m_hPlayerPawn ? Offsets::Schema::m_hPlayerPawn : Offsets::Schema::m_hPawn;
    const uint32_t pawnHandleFallbackOffset = (Offsets::Schema::m_hPlayerPawn && Offsets::Schema::m_hPawn)
        ? Offsets::Schema::m_hPawn
        : 0;

    if (!pawnHandlePrimaryOffset)
        return true;

    std::vector<uint32_t> fallbackPawnHandles(entities.size(), 0);
    if (const auto pawnHandleScatter = mem.CreateScatterHandle())
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            mem.AddScatterReadRequest(
                pawnHandleScatter,
                entities[i].Controller + pawnHandlePrimaryOffset,
                &entities[i].PawnHandle,
                sizeof(uint32_t)
            );

            if (pawnHandleFallbackOffset)
            {
                mem.AddScatterReadRequest(
                    pawnHandleScatter,
                    entities[i].Controller + pawnHandleFallbackOffset,
                    &fallbackPawnHandles[i],
                    sizeof(uint32_t)
                );
            }
        }

        mem.ExecuteReadScatter(pawnHandleScatter);
        mem.CloseScatterHandle(pawnHandleScatter);
    }
    else
    {
        return false;
    }

    std::vector<uint32_t> decodedHi(entities.size(), 0);
    std::vector<uint32_t> decodedLo(entities.size(), 0);
    std::unordered_set<uint32_t> uniqueHi{};

    for (size_t i = 0; i < entities.size(); ++i)
    {
        if (!entities[i].PawnHandle && pawnHandleFallbackOffset)
            entities[i].PawnHandle = fallbackPawnHandles[i];

        const uint32_t handleIndex = entities[i].PawnHandle & Offsets::EntityList::HandleMask;
        if (!handleIndex)
            continue;

        const uint32_t hi = handleIndex >> Offsets::EntityList::HandleHighShift;
        const uint32_t lo = handleIndex & Offsets::EntityList::HandleLowMask;
        decodedHi[i] = hi;
        decodedLo[i] = lo;
        uniqueHi.insert(hi);
    }

    std::unordered_map<uint32_t, uint64_t> chunkPointers{};
    chunkPointers.reserve(uniqueHi.size());
    for (const uint32_t hi : uniqueHi)
        chunkPointers.emplace(hi, 0ULL);

    if (!chunkPointers.empty())
    {
        if (const auto chunkScatter = mem.CreateScatterHandle())
        {
            for (auto& [hi, chunkPointer] : chunkPointers)
            {
                const uint64_t chunkAddress = core.EntityList + Offsets::EntityList::ListStart + static_cast<uint64_t>(hi) * Offsets::EntityList::ChunkStride;
                mem.AddScatterReadRequest(chunkScatter, chunkAddress, &chunkPointer, sizeof(uint64_t));
            }

            mem.ExecuteReadScatter(chunkScatter);
            mem.CloseScatterHandle(chunkScatter);
        }
        else
        {
            return false;
        }
    }

    if (const auto pawnPointerScatter = mem.CreateScatterHandle())
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (!(entities[i].PawnHandle & Offsets::EntityList::HandleMask))
                continue;

            const auto chunkIt = chunkPointers.find(decodedHi[i]);
            if (chunkIt == chunkPointers.end())
                continue;

            const uint64_t chunkPointer = chunkIt->second;
            if (!IsLikelyUserAddress(chunkPointer))
                continue;

            const uint64_t pawnAddress = chunkPointer + static_cast<uint64_t>(decodedLo[i]) * Offsets::EntityList::EntryStride;
            mem.AddScatterReadRequest(pawnPointerScatter, pawnAddress, &entities[i].Pawn, sizeof(uint64_t));
        }

        mem.ExecuteReadScatter(pawnPointerScatter);
        mem.CloseScatterHandle(pawnPointerScatter);
    }
    else
    {
        return false;
    }

    std::vector<SampledEntityData*> activeEntities{};
    activeEntities.reserve(entities.size());
    for (SampledEntityData& entity : entities)
    {
        if (!IsLikelyUserAddress(entity.Pawn))
            continue;
        if (entity.Pawn == core.LocalPawn)
            continue;
        activeEntities.push_back(&entity);
    }

    if (activeEntities.empty())
        return true;

    if (const auto pawnFieldScatter = mem.CreateScatterHandle())
    {
        for (SampledEntityData* entity : activeEntities)
        {
            if (Offsets::Schema::m_iHealth)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iHealth, &entity->Health, sizeof(entity->Health));
            if (Offsets::Schema::m_iTeamNum)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iTeamNum, &entity->Team, sizeof(entity->Team));
            if (Offsets::Schema::m_lifeState)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_lifeState, &entity->LifeState, sizeof(entity->LifeState));
            if (Offsets::Schema::m_iMaxHealth)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_iMaxHealth, &entity->MaxHealth, sizeof(entity->MaxHealth));
            if (Offsets::Schema::m_pGameSceneNode)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_pGameSceneNode, &entity->SceneNode, sizeof(entity->SceneNode));
            if (Offsets::Schema::m_vOldOrigin)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_vOldOrigin, &entity->OldOrigin, sizeof(entity->OldOrigin));
            if (Offsets::Schema::m_vecViewOffset)
                mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_vecViewOffset, &entity->ViewOffset, sizeof(entity->ViewOffset));

            PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
            entity->RefreshStatus = runtimeCache.LastStatusRead.time_since_epoch().count() == 0 ||
                now - runtimeCache.LastStatusRead >= std::chrono::milliseconds(150);

            if (!entity->RefreshStatus)
            {
                entity->Armor = runtimeCache.Armor;
                entity->IsScoped = runtimeCache.IsScoped;
                entity->FlashDuration = runtimeCache.FlashDuration;
            }
            else
            {
                if (Offsets::Schema::m_ArmorValue)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_ArmorValue, &entity->Armor, sizeof(entity->Armor));
                if (Offsets::Schema::m_bIsScoped)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bIsScoped, &entity->IsScoped, sizeof(entity->IsScoped));
                if (Offsets::Schema::m_flFlashDuration)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_flFlashDuration, &entity->FlashDuration, sizeof(entity->FlashDuration));
            }
        }

        mem.ExecuteReadScatter(pawnFieldScatter);
        mem.CloseScatterHandle(pawnFieldScatter);
    }
    else
    {
        return false;
    }

    for (SampledEntityData* entity : activeEntities)
    {
        if (std::abs(entity->ViewOffset.x) <= 0.001f && std::abs(entity->ViewOffset.y) <= 0.001f && std::abs(entity->ViewOffset.z) <= 0.001f)
            entity->ViewOffset = { 0.0f, 0.0f, 64.0f };

        if (entity->RefreshStatus)
        {
            PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
            runtimeCache.Armor = (std::max)(0, entity->Armor);
            runtimeCache.IsScoped = entity->IsScoped;
            runtimeCache.FlashDuration = (std::max)(0.0f, entity->FlashDuration);
            runtimeCache.LastStatusRead = now;
        }

        if (!IsLikelyUserAddress(entity->SceneNode))
            entity->SceneNode = 0;
    }

    if (const auto sceneScatter = mem.CreateScatterHandle())
    {
        for (SampledEntityData* entity : activeEntities)
        {
            if (!entity->SceneNode)
                continue;

            if (Offsets::Schema::m_vecAbsOrigin)
            {
                mem.AddScatterReadRequest(
                    sceneScatter,
                    entity->SceneNode + Offsets::Schema::m_vecAbsOrigin,
                    &entity->AbsOrigin,
                    sizeof(entity->AbsOrigin)
                );
                entity->HasAbsOrigin = true;
            }

            if (Offsets::Schema::m_modelState)
            {
                mem.AddScatterReadRequest(
                    sceneScatter,
                    entity->SceneNode + Offsets::Schema::m_modelState + Offsets::Layout::BoneArray,
                    &entity->BoneArray,
                    sizeof(entity->BoneArray)
                );
            }
        }

        mem.ExecuteReadScatter(sceneScatter);
        mem.CloseScatterHandle(sceneScatter);
    }
    else
    {
        return false;
    }

    outFrame.Players.clear();
    outFrame.Players.reserve(activeEntities.size());

    std::unordered_set<uint64_t> activeControllers{};
    std::unordered_set<uint64_t> activePawns{};
    activeControllers.reserve(activeEntities.size());
    activePawns.reserve(activeEntities.size());

    constexpr auto kMoneyPollInterval = std::chrono::seconds(1);
    constexpr auto kPostFreezeDuration = std::chrono::seconds(20);
    constexpr auto kEstimatedFreezeDuration = std::chrono::seconds(15); // fallback when freeze flag is unavailable.

    for (SampledEntityData* entity : activeEntities)
    {
        if (!IsAlive(entity->Health, entity->LifeState))
            continue;

        if (config.Visuals.TeamCheck && localTeam > 0 && entity->Team == localTeam)
            continue;

        activeControllers.insert(entity->Controller);
        activePawns.insert(entity->Pawn);

        ControllerIdentityCache& identityCache = m_ControllerIdentityCache[entity->Controller];
        if (identityCache.RoundEpoch != m_RoundEpoch)
        {
            identityCache.Name = ReadPlayerName(entity->Controller);
            identityCache.Money = ReadMoney(entity->Controller);
            identityCache.RoundEpoch = m_RoundEpoch;
            identityCache.LastMoneyRead = now;
        }
        else if (m_LastRoundEpochTick.time_since_epoch().count() != 0)
        {
            bool withinMoneyWindow = false;
            if (freezeValid)
            {
                if (freezePeriod)
                {
                    withinMoneyWindow = true;
                }
                else if (m_LastFreezeEndTick.time_since_epoch().count() != 0 &&
                    now - m_LastFreezeEndTick <= kPostFreezeDuration)
                {
                    withinMoneyWindow = true;
                }
            }
            else
            {
                const auto roundElapsed = now - m_LastRoundEpochTick;
                withinMoneyWindow = roundElapsed <= (kEstimatedFreezeDuration + kPostFreezeDuration);
            }

            if (withinMoneyWindow &&
                (identityCache.LastMoneyRead.time_since_epoch().count() == 0 ||
                    now - identityCache.LastMoneyRead >= kMoneyPollInterval))
            {
                identityCache.Money = ReadMoney(entity->Controller);
                identityCache.LastMoneyRead = now;
            }
        }

        PawnRuntimeCache& runtimeCache = m_PawnRuntimeCache[entity->Pawn];
        if (runtimeCache.WeaponName.empty() ||
            runtimeCache.LastWeaponRead.time_since_epoch().count() == 0 ||
            now - runtimeCache.LastWeaponRead >= std::chrono::milliseconds(500))
        {
            runtimeCache.WeaponName = ReadWeaponName(entity->Pawn);
            runtimeCache.LastWeaponRead = now;
        }

        PlayerEspSnapshot snapshot{};
        snapshot.Controller = entity->Controller;
        snapshot.Pawn = entity->Pawn;
        snapshot.SceneNode = entity->SceneNode;
        snapshot.BoneArray = IsLikelyUserAddress(entity->BoneArray) ? entity->BoneArray : 0;

        snapshot.Health = entity->Health;
        snapshot.MaxHealth = entity->MaxHealth > 0 ? entity->MaxHealth : 100;
        snapshot.Team = entity->Team;
        snapshot.LifeState = entity->LifeState;

        snapshot.Armor = runtimeCache.Armor;
        snapshot.IsScoped = runtimeCache.IsScoped;
        snapshot.FlashDuration = runtimeCache.FlashDuration;

        snapshot.Origin = entity->HasAbsOrigin ? entity->AbsOrigin : entity->OldOrigin;
        snapshot.EyePosition = snapshot.Origin + entity->ViewOffset;

        snapshot.Name = identityCache.Name;
        snapshot.Money = identityCache.Money;
        snapshot.WeaponName = runtimeCache.WeaponName;

        bool hasBoxData = false;
        if (config.Visuals.Bones && snapshot.BoneArray)
            hasBoxData = BuildBoneData(snapshot.BoneArray, snapshot);

        if (!hasBoxData)
        {
            Vector2 screenHead{};
            Vector2 screenFeet{};
            snapshot.HeadPosition = snapshot.Origin + Vector3{ 0.0f, 0.0f, 72.0f };

            if (!sdk.WorldToScreen(snapshot.HeadPosition, screenHead) || !sdk.WorldToScreen(snapshot.Origin, screenFeet))
                continue;

            const float boxHeight = std::abs(screenFeet.y - screenHead.y);
            if (boxHeight < 4.0f)
                continue;

            const float boxWidth = boxHeight * 0.45f;
            snapshot.BoxMin = ImVec2(screenFeet.x - boxWidth * 0.5f, screenHead.y);
            snapshot.BoxMax = ImVec2(screenFeet.x + boxWidth * 0.5f, screenFeet.y);
        }

        if (config.Visuals.VisibleCheck)
            snapshot.IsVisible = CheckVisibility(localEyePosition, snapshot.HeadPosition);

        outFrame.Players.push_back(std::move(snapshot));
    }

    for (auto it = m_ControllerIdentityCache.begin(); it != m_ControllerIdentityCache.end();)
    {
        if (activeControllers.find(it->first) == activeControllers.end())
            it = m_ControllerIdentityCache.erase(it);
        else
            ++it;
    }

    for (auto it = m_PawnRuntimeCache.begin(); it != m_PawnRuntimeCache.end();)
    {
        if (activePawns.find(it->first) == activePawns.end())
            it = m_PawnRuntimeCache.erase(it);
        else
            ++it;
    }

    if (config.Visuals.C4 || config.Visuals.Defuser)
        outFrame.C4 = ReadC4Snapshot();
    else
        outFrame.C4 = C4Snapshot{};

    return true;
}

void ESP::Render(ImDrawList* drawList)
{
    const auto renderStart = std::chrono::steady_clock::now();
    EnsureSamplerStarted();

    std::uint32_t resolvedControllers = 0;
    std::uint32_t drawnPlayers = 0;
    const auto publishPerf = [&]()
    {
        const auto renderUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - renderStart
        ).count();

        if (renderUs >= 0)
        {
            PerfDebug::RecordEspFrame(
                static_cast<std::uint64_t>(renderUs),
                resolvedControllers,
                drawnPlayers
            );
        }
    };

    if (!drawList)
    {
        publishPerf();
        return;
    }

    if (!config.Visuals.Enabled)
    {
        publishPerf();
        return;
    }

    RenderFrame frame{};
    {
        std::lock_guard lock(m_RenderFrameMutex);
        frame = m_RenderFrame;
    }

    resolvedControllers = frame.ResolvedControllers;
    drawnPlayers = static_cast<std::uint32_t>(frame.Players.size());

    RenderWatermark(drawList);

    if (config.Visuals.VisibleCheck)
    {
        const ImVec2 statusPos(12.0f, config.Visuals.Watermark ? 34.0f : 12.0f);
        const char* mapStatus = frame.MapStatus.empty() ? "Map Status: (Waiting)" : frame.MapStatus.c_str();
        drawList->AddText(statusPos, IM_COL32(210, 210, 210, 255), mapStatus);
    }

    for (const PlayerEspSnapshot& player : frame.Players)
        RenderPlayer(drawList, player);

    if (config.Visuals.C4 || config.Visuals.Defuser)
        RenderC4(drawList, frame.C4);

    publishPerf();
}
