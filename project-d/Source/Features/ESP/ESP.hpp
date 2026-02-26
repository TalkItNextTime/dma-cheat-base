#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../../VisCheckCS2/VisCheck.h"

struct BonePoint
{
    int Index = 0;
    Vector3 World{};
    Vector2 Screen{};
    bool OnScreen = false;
};

struct PlayerEspSnapshot
{
    uint64_t Controller = 0;
    uint64_t Pawn = 0;
    uint64_t SceneNode = 0;
    uint64_t BoneArray = 0;

    int Health = 0;
    int MaxHealth = 100;
    int Team = 0;
    int Armor = 0;
    int Money = 0;
    bool ShowMoney = false;
    int LifeState = 0;

    bool IsScoped = false;
    bool HasDefuser = false;
    bool HasHelmet = false;
    float FlashDuration = 0.0f;
    float FlashOverlayAlpha = 0.0f;
    float FlashMaxAlpha = 255.0f;

    Vector3 Origin{};
    Vector3 EyePosition{};
    Vector3 HeadPosition{};

    std::string Name{};
    std::string WeaponName{};

    ImVec2 BoxMin{};
    ImVec2 BoxMax{};

    std::vector<BonePoint> Bones{};
    bool IsVisible = false;
};

struct C4Snapshot
{
    bool Valid = false;
    bool Planted = false;
    bool BombTicking = false;
    bool BombDefused = false;
    bool BeingDefused = false;
    int BombSite = -1;
    float BlowTime = 0.0f;
    float TimerLength = 0.0f;
    float TimeRemaining = 0.0f;
    float DefuseLength = 0.0f;
    float DefuseCountDown = 0.0f;
    float DefuseProgress = 0.0f;
    bool CanDefuse = false;
    uint64_t BombEntity = 0;
    uint64_t BombDefuserPawn = 0;
    Vector3 Position{};
    Vector2 Screen{};
    bool OnScreen = false;
};

struct SpectatorListSnapshot
{
    bool Valid = false;
    bool LocalIsSpectating = false;
    std::string TargetName{};
    std::vector<std::string> WatcherNames{};
};

struct TriggerBoneSnapshot
{
    uint64_t Pawn = 0;
    int Team = 0;
    int Health = 0;
    int LifeState = 0;
    bool IsVisible = false;
    ImVec2 BoxMin{};
    ImVec2 BoxMax{};

    static constexpr std::size_t BoneCount = 17;
    std::array<BonePoint, BoneCount> Bones{};
    std::array<bool, BoneCount> BoneValid{};
};

struct GrenadeStandRenderItem
{
    Vector2 Screen{};
    std::string Label{};
};

struct GrenadeAimRenderItem
{
    Vector2 Screen{};
    std::string Label{};
    bool IsTarget = false;
    bool DrawGuide = false;
};

struct GrenadeHelperSnapshot
{
    bool Valid = false;
    int SelectedSpotId = 0;
    Vector2 Cross{};
    std::vector<std::string> TopHintTokens{};
    std::string TopHintRemark{};
    std::vector<GrenadeStandRenderItem> StandItems{};
    std::vector<GrenadeAimRenderItem> AimItems{};
};

struct MapDebugTriangle
{
    Vector3 V0{};
    Vector3 V1{};
    Vector3 V2{};
    std::uint8_t SourceKind = 0;
};

struct MapDebugBox
{
    Vector3 Min{};
    Vector3 Max{};
};

struct GrenadeSpotEditorRow
{
    int Id = 0;
    int TypeIndex = 0;
    std::string ThrowType{};
    std::string Remark{};
    std::string Name{};
    Vector3 StandPos{};
    Vector3 AimPos{};
};

class ESP
{
private:
    struct GrenadeSpot
    {
        int Id = 0;
        std::string Type{};
        std::string Name{};
        std::string Remark{};
        std::string ThrowType{};
        Vector3 StandPos{};
        Vector3 AimPos{};
    };

    struct GrenadeMapData
    {
        std::string MapName{};
        std::vector<GrenadeSpot> Spots{};
    };

    struct PendingMapLoad
    {
        uint64_t RequestId = 0;
        std::string MapName{};
        std::string CachePath{};
        std::future<std::unique_ptr<VisCheck>> Future{};
    };

    struct RenderFrame
    {
        std::vector<PlayerEspSnapshot> Players{};
        C4Snapshot C4{};
        SpectatorListSnapshot SpectatorList{};
        GrenadeHelperSnapshot GrenadeHelper{};
        std::string MapStatus = "Map Status: (Waiting)";
        std::uint32_t ResolvedControllers = 0;
    };

    struct ControllerIdentityCache
    {
        std::string Name{};
        int Money = 0;
        std::uint64_t RoundEpoch = 0;
        std::chrono::steady_clock::time_point LastMoneyRead{};
    };

    struct PawnRuntimeCache
    {
        std::string WeaponName{};
        std::chrono::steady_clock::time_point LastWeaponRead{};

        int Armor = 0;
        bool IsScoped = false;
        bool HasDefuser = false;
        bool HasHelmet = false;
        float FlashDuration = 0.0f;
        float FlashOverlayAlpha = 0.0f;
        float FlashMaxAlpha = 255.0f;
        std::chrono::steady_clock::time_point LastStatusRead{};
        int LastAmmoClip = -1;
        std::chrono::steady_clock::time_point LastShotTick{};
    };

    struct VisDebugScreenLine
    {
        Vector2 Start{};
        Vector2 End{};
        ImU32 Color = 0;
        float Thickness = 1.0f;
    };

    void Render(ImDrawList* drawList);
    void RenderWatermark(ImDrawList* drawList) const;
    void RenderPlayer(ImDrawList* drawList, const PlayerEspSnapshot& player) const;
    void RenderSkeleton(ImDrawList* drawList, const PlayerEspSnapshot& player, ImU32 color) const;
    void RenderTriggerHitboxDebug(ImDrawList* drawList, const PlayerEspSnapshot& player) const;
    void RenderC4(ImDrawList* drawList, const C4Snapshot& c4) const;
    void RenderSpectatorList(ImDrawList* drawList, const SpectatorListSnapshot& spectatorList) const;
    void RenderGrenadeHelper(ImDrawList* drawList, const GrenadeHelperSnapshot& helper) const;
    void RenderVisCheckDebug(ImDrawList* drawList) const;
    void BuildVisCheckDebugOverlaySnapshot();
    void ClearVisCheckDebugOverlaySnapshot();

    void EnsureSamplerStarted();
    void EnsureRadarPublisherStarted();
    void SamplerLoop();
    void RadarPublisherLoop();
    void QueueRadarPayload(std::string payload);
    bool SampleFrame(RenderFrame& outFrame);
    void UpdateRoundEpoch(uint64_t localPawn, bool localAlive);

    bool BuildBoneData(uint64_t boneArray, PlayerEspSnapshot& inOutSnapshot) const;
    C4Snapshot ReadC4Snapshot() const;

    bool IsAlive(int health, int lifeState) const;
    ImU32 GetBoxColor(bool isVisible) const;
    ImU32 ToImColor(const ImVec4& color) const;

    std::string ReadPlayerName(uint64_t controller) const;
    std::string ReadWeaponName(uint64_t pawn) const;
    int ReadMoney(uint64_t controller) const;
    int ReadWeaponId(uint64_t pawn) const;
    int ReadActiveWeaponClip(uint64_t pawn) const;
    std::string ReadGrenadeType(uint64_t pawn) const;

    void BuildGrenadeHelperSnapshot(
        const Vector3& localOrigin,
        const Vector3& localEye,
        const Vector3& localViewAngles,
        uint64_t localPawn,
        const std::string& heldGrenadeType,
        GrenadeHelperSnapshot& outHelper);
    void EnsureGrenadeMapLoaded(const std::string& mapName);
    std::string ResolveGrenadeDataPath(const std::string& mapName) const;
    bool LoadGrenadeMapFile(const std::string& filePath, GrenadeMapData& outMap, std::string& outError) const;
    std::string ResolveGrenadeDataWritePath(const std::string& mapName) const;
    bool SaveGrenadeMapFile(const std::string& filePath, const GrenadeMapData& mapData, std::string& outError) const;
    bool ReloadGrenadeMapFromDisk(const std::string& mapName, std::string& outStatus);

    void UpdateVisCheckState();
    void RequestMapLoad(const std::string& mapName, const std::string& cachePath);
    void ConsumeMapLoadResult();
    std::string ResolveCachePath(const std::string& mapName) const;
    std::string BuildMapStatus(const std::string& mapName, const char* suffix) const;
    bool CheckVisibility(const Vector3& src, const Vector3& dst) const;
    void ClearMapDebugCache();
    void UpdateMapDebugCacheFromVisCheck(const VisCheck& visCheck);

private:
    std::unique_ptr<VisCheck> m_VisCheck{};
    std::vector<PendingMapLoad> m_PendingMapLoads{};
    uint64_t m_NextMapRequestId = 0;
    uint64_t m_ActiveMapRequestId = 0;

    std::atomic<bool> m_SamplerStarted{ false };
    std::atomic<bool> m_RadarPublisherStarted{ false };
    mutable std::mutex m_RenderFrameMutex{};
    RenderFrame m_RenderFrame{};
    mutable std::mutex m_RadarPublishMutex{};
    std::condition_variable m_RadarPublishCv{};
    std::string m_RadarPendingPayload{};
    bool m_RadarPendingDirty = false;
    mutable std::mutex m_GrenadeMutex{};
    GrenadeMapData m_GrenadeMap{};
    std::string m_LoadedGrenadeMap{};
    int m_LastGrenadeSelectedSpotId = 0;

    std::string m_CurrentMapName{};
    std::string m_CurrentCachePath{};
    std::string m_MapStatus = "Map Status: (Waiting)";
    std::string m_LastPolledMapName{};
    std::chrono::steady_clock::time_point m_LastMapPoll{};
    bool m_VisCheckEnabled = false;
    mutable std::mutex m_MapDebugMutex{};
    std::vector<MapDebugTriangle> m_MapDebugTriangles{};
    std::vector<MapDebugBox> m_MapDebugBoxes{};
    mutable std::mutex m_VisDebugOverlayMutex{};
    std::vector<VisDebugScreenLine> m_VisDebugOverlayLines{};
    std::string m_GrenadeStatus{};
    std::atomic<bool> m_GrenadeHelperOnlyMode{ false };
    std::atomic<bool> m_GrenadeHelperHoldingUtility{ false };

    std::unordered_map<uint64_t, ControllerIdentityCache> m_ControllerIdentityCache{};
    std::unordered_map<uint64_t, PawnRuntimeCache> m_PawnRuntimeCache{};
    std::unordered_map<uint64_t, int> m_RadarObserverSlots{};
    std::chrono::steady_clock::time_point m_LastRadarCompose{};
    C4Snapshot m_C4Cache{};
    std::chrono::steady_clock::time_point m_LastC4Sample{};
    std::uint64_t m_RoundEpoch = 1;
    std::uint64_t m_LastRoundLocalPawn = 0;
    bool m_LastRoundLocalAlive = false;
    std::chrono::steady_clock::time_point m_LastRoundEpochTick{};
    bool m_LastFreezePeriod = false;
    std::chrono::steady_clock::time_point m_LastFreezeEndTick{};
    std::chrono::steady_clock::time_point m_LastSpectatorDebugLog{};

public:
    bool IsPawnVisibleCached(uint64_t pawn) const;
    std::unordered_set<uint64_t> GetVisiblePawnSetSnapshot() const;
    std::vector<TriggerBoneSnapshot> GetTriggerBoneSnapshots() const;
    std::string GetSuggestedGrenadeMapName() const;
    std::string GetGrenadeStatus() const;
    std::vector<GrenadeSpotEditorRow> GetGrenadeSpotEditorRows() const;
    void UpdateVisCheckDebugOverlayFromDebugThread();
    bool ReloadGrenadeSpots(const std::string& mapName, std::string& outStatus);
    bool SaveGrenadeSpotEditorRows(const std::string& mapName, const std::vector<GrenadeSpotEditorRow>& rows, std::string& outStatus);
    bool RecordCurrentGrenadeSpot(
        const std::string& mapName,
        const std::string& spotName,
        const std::string& throwType,
        const std::string& remark,
        float recordDistance,
        bool manualTypeOverride,
        int manualTypeIndex,
        std::string& outStatus);
    bool DetectCurrentGrenadeTypeIndex(int& outTypeIndex) const;
    int GetCurrentGrenadeFocusedSpotId() const;

    void Update(ImDrawList* drawList)
    {
        TIMER("ESP render");

        Render(drawList);
    }

    static ESP& Get()
    {
        static ESP instance;
        return instance;
    }
};

inline ESP& esp = ESP::Get();
