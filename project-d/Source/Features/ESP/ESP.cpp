#include <Pch.hpp>
#include <SDK.hpp>
#include "ESP.hpp"
#include <array>
#include <cfloat>
#include <cstdio>
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

    constexpr std::array<int, 17> kTrackedBones = {
        0, 2, 4, 5, 6,
        8, 9, 10,
        13, 14, 15,
        22, 23, 24,
        25, 26, 27
    };

    constexpr std::array<std::pair<int, int>, 16> kBoneLinks = {
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
        std::pair{ 26, 27 }
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

    bool IsNonZeroPosition(const Vector3& position)
    {
        return std::abs(position.x) > 0.01f ||
            std::abs(position.y) > 0.01f ||
            std::abs(position.z) > 0.01f;
    }

    int NormalizeBombSiteRaw(const int rawSite)
    {
        if (rawSite == 0)
            return 0; // A
        if (rawSite == 1 || rawSite == 2)
            return 1; // B (some dumps are 1-based)
        if (rawSite == 'A' || rawSite == 'a')
            return 0;
        if (rawSite == 'B' || rawSite == 'b')
            return 1;
        return -1;
    }

    float ReadGlobalCurrentTime()
    {
        if (!Globals::ClientBase || !Offsets::Client::dwGlobalVars)
            return 0.0f;

        const uint64_t globalVars = mem.Read<uint64_t>(Globals::ClientBase + Offsets::Client::dwGlobalVars);
        if (!IsLikelyUserAddress(globalVars))
            return 0.0f;

        float currentTime = mem.Read<float>(globalVars + 0x2C);
        if (currentTime <= 0.0f)
            currentTime = mem.Read<float>(globalVars + 0x30);
        return currentTime;
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
        bool HasDefuser = false;
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
    statusLines.reserve(5);

    if (player.IsScoped)
        statusLines.push_back({ "Scoped", IM_COL32(220, 220, 220, 255) });

    if (player.FlashDuration > 0.01f)
        statusLines.push_back({ "Flashed", IM_COL32(255, 214, 120, 255) });

    if (config.Visuals.Armor)
        statusLines.push_back({ "AR:" + std::to_string(player.Armor), ToImColor(config.Visuals.ArmorColor) });

    if (config.Visuals.Money && player.ShowMoney)
        statusLines.push_back({ "$" + std::to_string(player.Money), ToImColor(config.Visuals.MoneyColor) });

    if (config.Visuals.Defuser && player.HasDefuser)
        statusLines.push_back({ "Kit", ToImColor(config.Visuals.DefuserColor) });

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
    if (!c4.Valid || !config.Visuals.C4)
        return;

    auto formatSeconds1 = [](const float value) -> std::string
    {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "%.1f", (std::max)(0.0f, value));
        return buffer;
    };

    if (c4.OnScreen)
    {
        const ImU32 markerColor = c4.Planted ? ToImColor(config.Visuals.C4Color) : IM_COL32(240, 220, 70, 255);
        drawList->AddCircleFilled(c4.Screen.ToImVec2(), 4.0f, markerColor, 12);
        drawList->AddCircle(c4.Screen.ToImVec2(), 8.0f, markerColor, 12, 1.2f);
        drawList->AddText(ImVec2(c4.Screen.x + 9.0f, c4.Screen.y - 15.0f), markerColor, c4.Planted ? "C4(Planted)" : "C4");
    }

    if (!c4.Planted)
        return;

    std::string siteName = "Unknown";
    if (c4.BombSite == 0)
        siteName = "A";
    else if (c4.BombSite == 1)
        siteName = "B";

    const std::string line1 = std::string("C4 Site: ") + siteName + " | " + (c4.BeingDefused ? "Defusing" : "Not Defusing");

    std::string line2 = "Explode: " + formatSeconds1(c4.TimeRemaining) + "s  Defuse: ";
    if (c4.BeingDefused)
        line2 += formatSeconds1(c4.DefuseCountDown) + "s";
    else
        line2 += "--";

    std::string line3 = "Defuse Result: --";
    if (c4.BeingDefused)
        line3 = std::string("Defuse Result: ") + (c4.CanDefuse ? "SUCCESS" : "FAIL");

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    constexpr float panelW = 250.0f;
    constexpr float panelH = 84.0f;
    const float panelX = std::clamp(config.Visuals.C4PanelPosX * displaySize.x, 8.0f, (std::max)(8.0f, displaySize.x - panelW - 8.0f));
    const float panelY = std::clamp(config.Visuals.C4PanelPosY * displaySize.y, 8.0f, (std::max)(8.0f, displaySize.y - panelH - 8.0f));
    drawList->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(18, 18, 18, 190),
        5.0f
    );
    drawList->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        ToImColor(config.Visuals.C4Color),
        5.0f,
        0,
        1.2f
    );

    drawList->AddText(ImVec2(panelX + 10.0f, panelY + 8.0f), ToImColor(config.Visuals.C4Color), line1.c_str());
    drawList->AddText(
        ImVec2(panelX + 10.0f, panelY + 28.0f),
        c4.BeingDefused ? ToImColor(config.Visuals.DefuserColor) : IM_COL32(225, 225, 225, 255),
        line2.c_str()
    );
    drawList->AddText(
        ImVec2(panelX + 10.0f, panelY + 48.0f),
        c4.BeingDefused ? (c4.CanDefuse ? IM_COL32(120, 235, 120, 255) : IM_COL32(255, 110, 110, 255)) : IM_COL32(225, 225, 225, 255),
        line3.c_str()
    );
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

    const float gameTime = ReadGlobalCurrentTime();
    const std::uint64_t nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    static std::uint64_t bombPlantStartMs = 0;
    static std::uint64_t bombDefuseStartMs = 0;
    static bool wasDefusing = false;
    static bool canDefuseLatched = false;

    auto readBool = [](const std::uint64_t address, bool& outValue)
    {
        outValue = mem.Read<std::uint8_t>(address) != 0;
    };

    auto readBombWorldPos = [](const std::uint64_t bombEntity, Vector3& outPos) -> bool
    {
        outPos = {};
        if (!IsLikelyUserAddress(bombEntity))
            return false;

        if (Offsets::Schema::m_pGameSceneNode && Offsets::Schema::m_vecAbsOrigin)
        {
            const std::uint64_t sceneNode = mem.Read<std::uint64_t>(bombEntity + Offsets::Schema::m_pGameSceneNode);
            if (IsLikelyUserAddress(sceneNode))
            {
                const Vector3 absPos = mem.Read<Vector3>(sceneNode + Offsets::Schema::m_vecAbsOrigin);
                if (IsNonZeroPosition(absPos))
                {
                    outPos = absPos;
                    return true;
                }
            }
        }

        if (Offsets::Schema::m_vecC4ExplodeSpectatePos)
        {
            const Vector3 spectatePos = mem.Read<Vector3>(bombEntity + Offsets::Schema::m_vecC4ExplodeSpectatePos);
            if (IsNonZeroPosition(spectatePos))
            {
                outPos = spectatePos;
                return true;
            }
        }

        if (Offsets::Schema::m_vOldOrigin)
        {
            const Vector3 oldOrigin = mem.Read<Vector3>(bombEntity + Offsets::Schema::m_vOldOrigin);
            if (IsNonZeroPosition(oldOrigin))
            {
                outPos = oldOrigin;
                return true;
            }
        }

        return false;
    };

    auto resolveBombEntityFromHolder = [&](const std::uint64_t holderAddress) -> std::uint64_t
    {
        if (!IsLikelyUserAddress(holderAddress))
            return 0;

        // Primary path used by reference project: holder -> entity pointer.
        const std::uint64_t indirectEntity = mem.Read<std::uint64_t>(holderAddress);
        if (IsLikelyUserAddress(indirectEntity))
            return indirectEntity;

        // Fallback for builds where global points directly to entity.
        Vector3 validatePos{};
        if (readBombWorldPos(holderAddress, validatePos))
            return holderAddress;

        return 0;
    };

    const std::uint64_t clientBase = Globals::ClientBase;
    const std::uint64_t plantedHolder = mem.Read<std::uint64_t>(clientBase + Offsets::Client::dwPlantedC4);

    bool plantedFlag = false;
    if (Offsets::Client::dwPlantedC4 >= 0x8)
    {
        const std::uint8_t plantedByte = mem.Read<std::uint8_t>(clientBase + Offsets::Client::dwPlantedC4 - 0x8);
        plantedFlag = plantedByte != 0;
    }

    const std::uint64_t plantedEntity = resolveBombEntityFromHolder(plantedHolder);
    if (!plantedFlag && IsLikelyUserAddress(plantedEntity) && Offsets::Schema::m_bBombTicking)
    {
        bool tickingCandidate = false;
        readBool(plantedEntity + Offsets::Schema::m_bBombTicking, tickingCandidate);

        bool defusedCandidate = false;
        if (Offsets::Schema::m_bBombDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBombDefused, defusedCandidate);

        plantedFlag = tickingCandidate && !defusedCandidate;
    }

    if (plantedFlag && IsLikelyUserAddress(plantedEntity))
    {
        snapshot.Valid = true;

        if (Offsets::Schema::m_nBombSite)
        {
            int normalizedSite = NormalizeBombSiteRaw(static_cast<int>(mem.Read<std::uint8_t>(plantedEntity + Offsets::Schema::m_nBombSite)));
            if (normalizedSite < 0)
                normalizedSite = NormalizeBombSiteRaw(mem.Read<int>(plantedEntity + Offsets::Schema::m_nBombSite));
            snapshot.BombSite = normalizedSite;
        }

        if (Offsets::Schema::m_bBombTicking)
            readBool(plantedEntity + Offsets::Schema::m_bBombTicking, snapshot.BombTicking);
        if (Offsets::Schema::m_bBombDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBombDefused, snapshot.BombDefused);
        if (Offsets::Schema::m_bBeingDefused)
            readBool(plantedEntity + Offsets::Schema::m_bBeingDefused, snapshot.BeingDefused);
        if (Offsets::Schema::m_flTimerLength)
            snapshot.TimerLength = mem.Read<float>(plantedEntity + Offsets::Schema::m_flTimerLength);
        if (Offsets::Schema::m_flDefuseLength)
            snapshot.DefuseLength = mem.Read<float>(plantedEntity + Offsets::Schema::m_flDefuseLength);
        if (Offsets::Schema::m_flC4Blow)
            snapshot.BlowTime = mem.Read<float>(plantedEntity + Offsets::Schema::m_flC4Blow);
        if (Offsets::Schema::m_flDefuseCountDown)
            snapshot.DefuseCountDown = mem.Read<float>(plantedEntity + Offsets::Schema::m_flDefuseCountDown);

        snapshot.Planted = snapshot.BombTicking && !snapshot.BombDefused;

        if (snapshot.BombTicking && snapshot.BlowTime > 0.001f && gameTime > 0.001f)
        {
            snapshot.TimeRemaining = (std::max)(0.0f, snapshot.BlowTime - gameTime);
            bombPlantStartMs = 0;
        }
        else if (snapshot.BombTicking && snapshot.TimerLength > 0.001f)
        {
            if (bombPlantStartMs == 0)
                bombPlantStartMs = nowMs;
            const float elapsed = static_cast<float>(nowMs - bombPlantStartMs) / 1000.0f;
            snapshot.TimeRemaining = (std::max)(0.0f, snapshot.TimerLength - elapsed);
        }
        else
        {
            bombPlantStartMs = 0;
            snapshot.TimeRemaining = 0.0f;
        }

        if (snapshot.BeingDefused)
        {
            if (snapshot.DefuseCountDown > 0.001f && gameTime > 0.001f)
            {
                snapshot.DefuseCountDown = (std::max)(0.0f, snapshot.DefuseCountDown - gameTime);
                bombDefuseStartMs = 0;
            }
            else
            {
                if (bombDefuseStartMs == 0)
                    bombDefuseStartMs = nowMs;
                const float elapsed = static_cast<float>(nowMs - bombDefuseStartMs) / 1000.0f;
                const float total = snapshot.DefuseLength > 0.001f ? snapshot.DefuseLength : 10.0f;
                snapshot.DefuseCountDown = (std::max)(0.0f, total - elapsed);
            }

            const float totalDefuse = snapshot.DefuseLength > 0.001f ? snapshot.DefuseLength : 10.0f;
            snapshot.DefuseProgress = std::clamp(1.0f - (snapshot.DefuseCountDown / totalDefuse), 0.0f, 1.0f);

            if (!wasDefusing)
                canDefuseLatched = snapshot.DefuseCountDown <= (snapshot.TimeRemaining + 0.05f);

            snapshot.CanDefuse = canDefuseLatched;
            wasDefusing = true;
        }
        else
        {
            bombDefuseStartMs = 0;
            snapshot.DefuseCountDown = 0.0f;
            snapshot.DefuseProgress = 0.0f;
            snapshot.CanDefuse = false;
            wasDefusing = false;
            canDefuseLatched = false;
        }

        if (snapshot.BombDefused)
        {
            bombPlantStartMs = 0;
            bombDefuseStartMs = 0;
            wasDefusing = false;
            canDefuseLatched = false;
        }

        readBombWorldPos(plantedEntity, snapshot.Position);
    }
    else
    {
        bombPlantStartMs = 0;
        bombDefuseStartMs = 0;
        wasDefusing = false;
        canDefuseLatched = false;

        if (Offsets::Client::dwWeaponC4)
        {
            const std::uint64_t weaponHolder = mem.Read<std::uint64_t>(clientBase + Offsets::Client::dwWeaponC4);
            const std::uint64_t weaponEntity = resolveBombEntityFromHolder(weaponHolder);
            if (IsLikelyUserAddress(weaponEntity))
            {
                snapshot.Valid = true;
                snapshot.Planted = false;
                readBombWorldPos(weaponEntity, snapshot.Position);
            }
        }
    }

    if (IsNonZeroPosition(snapshot.Position))
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
    if (!Offsets::Schema::m_AttributeManager || !Offsets::Schema::m_Item || !Offsets::Schema::m_iItemDefinitionIndex)
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

    const std::uint64_t itemDefinitionIndexAddress =
        weapon +
        static_cast<uint64_t>(Offsets::Schema::m_AttributeManager) +
        static_cast<uint64_t>(Offsets::Schema::m_Item) +
        static_cast<uint64_t>(Offsets::Schema::m_iItemDefinitionIndex);

    const int weaponId = static_cast<int>(mem.Read<std::uint16_t>(itemDefinitionIndexAddress));

    if (weaponId <= 0)
        return {};

    const std::string resolvedName = WeaponIdToName(weaponId);
    if (!resolvedName.empty())
        return resolvedName;

    return "Weapon " + std::to_string(weaponId);
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
        //PerfDebug::RecordMapPoll(!m_LastPolledMapName.empty());
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
        //PerfDebug::RecordVisCheck(static_cast<std::uint64_t>(durationUs), isVisible);

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
    constexpr auto kOverrunYield = std::chrono::milliseconds(1);

    while (Globals::Running)
    {
        const auto cycleStart = std::chrono::steady_clock::now();

        RenderFrame sampledFrame{};
        sampledFrame.MapStatus = m_MapStatus;
        SampleFrame(sampledFrame);

        {
            std::lock_guard lock(m_RenderFrameMutex);
            std::swap(m_RenderFrame, sampledFrame);
        }

        const auto sampleUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - cycleStart
        ).count();
        if (sampleUs >= 0)
            PerfDebug::RecordEspSampleFrame(static_cast<std::uint64_t>(sampleUs));

        const auto elapsed = std::chrono::steady_clock::now() - cycleStart;
        if (elapsed < kSampleInterval)
            std::this_thread::sleep_for(kSampleInterval - elapsed);
        else
            std::this_thread::sleep_for(kOverrunYield);
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
                entity->HasDefuser = runtimeCache.HasDefuser;
                entity->FlashDuration = runtimeCache.FlashDuration;
            }
            else
            {
                if (Offsets::Schema::m_ArmorValue)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_ArmorValue, &entity->Armor, sizeof(entity->Armor));
                if (Offsets::Schema::m_bIsScoped)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bIsScoped, &entity->IsScoped, sizeof(entity->IsScoped));
                if (Offsets::Schema::m_bHasDefuser)
                    mem.AddScatterReadRequest(pawnFieldScatter, entity->Pawn + Offsets::Schema::m_bHasDefuser, &entity->HasDefuser, sizeof(entity->HasDefuser));
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
            runtimeCache.HasDefuser = entity->HasDefuser;
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
    bool moneyWindowActive = false;
    if (m_LastRoundEpochTick.time_since_epoch().count() != 0)
    {
        if (freezeValid)
        {
            moneyWindowActive = freezePeriod ||
                (m_LastFreezeEndTick.time_since_epoch().count() != 0 &&
                    now - m_LastFreezeEndTick <= kPostFreezeDuration);
        }
        else
        {
            const auto roundElapsed = now - m_LastRoundEpochTick;
            moneyWindowActive = roundElapsed <= (kEstimatedFreezeDuration + kPostFreezeDuration);
        }
    }

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
            identityCache.RoundEpoch = m_RoundEpoch;
            if (moneyWindowActive)
            {
                identityCache.Money = ReadMoney(entity->Controller);
                identityCache.LastMoneyRead = now;
            }
        }
        else if (moneyWindowActive &&
            (identityCache.LastMoneyRead.time_since_epoch().count() == 0 ||
                now - identityCache.LastMoneyRead >= kMoneyPollInterval))
        {
            identityCache.Money = ReadMoney(entity->Controller);
            identityCache.LastMoneyRead = now;
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
        snapshot.HasDefuser = runtimeCache.HasDefuser;
        snapshot.FlashDuration = runtimeCache.FlashDuration;

        snapshot.Origin = entity->HasAbsOrigin ? entity->AbsOrigin : entity->OldOrigin;
        snapshot.EyePosition = snapshot.Origin + entity->ViewOffset;

        snapshot.Name = identityCache.Name;
        snapshot.Money = identityCache.Money;
        snapshot.ShowMoney = moneyWindowActive;
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

    if (config.Visuals.C4)
    {
        constexpr auto kC4IntervalIdle = std::chrono::microseconds(16667);   // 60Hz
        constexpr auto kC4IntervalPlanted = std::chrono::microseconds(10000); // 100Hz

        const bool cacheValid = m_LastC4Sample.time_since_epoch().count() != 0;
        const auto targetInterval = m_C4Cache.Planted ? kC4IntervalPlanted : kC4IntervalIdle;

        if (!cacheValid || (now - m_LastC4Sample) >= targetInterval)
        {
            m_C4Cache = ReadC4Snapshot();
            m_LastC4Sample = now;
        }

        outFrame.C4 = m_C4Cache;
    }
    else
    {
        m_C4Cache = C4Snapshot{};
        m_LastC4Sample = {};
        outFrame.C4 = C4Snapshot{};
    }

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
           /* PerfDebug::RecordEspFrame(
                static_cast<std::uint64_t>(renderUs),
                resolvedControllers,
                drawnPlayers
            );*/
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

    if (config.Visuals.C4)
        RenderC4(drawList, frame.C4);

    publishPerf();
}
