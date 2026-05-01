#pragma once
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../../VisCheckCS2/VisCheck.h"
#include "GrenadeEntityEspModel.hpp"
#include "EspFrameSyncModel.hpp"
#include "PlayerBoxModel.hpp"
#include "VisWorldDebugRender.hpp"

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
    std::string WeaponIconToken{};

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
    uint64_t BombOwnerPawn = 0;
    bool StartedArming = false;
    bool PlantingViaUse = false;
    float ArmedTime = 0.0f;
    float PlantCountdown = 0.0f;
    float PlantLength = 3.2f;
    Vector3 Position{};
    Vector2 Screen{};
    bool OnScreen = false;
};

struct GrenadeEntitySnapshot
{
    bool Valid = false;
    std::uint64_t Entity = 0;
    GrenadeEntityEspModel::Type Type = GrenadeEntityEspModel::Type::Unknown;
    std::string Id{};
    std::string Team{};
    Vector3 Position{};
    Vector2 Screen{};
    bool OnScreen = false;
    float Countdown = -1.0f;
    float DistanceMeters = 0.0f;
    bool Exploded = false;
    bool IsInferno = false;
    std::vector<Vector3> FlamePositions{};
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

    struct RadarPlayerFrameItem
    {
        std::string PlayerId{};
        std::string SteamId{};
        uint64_t Controller = 0;
        uint64_t Pawn = 0;
        int Team = 0;
        int Health = 0;
        int Armor = 0;
        int Money = 0;
        int Flashed = 0;
        int AmmoClip = -1;
        int CompTeammateColor = -1;
        Vector3 Position{};
        Vector3 EyeAngles{};
        std::string Name{};
        std::string WeaponName{};
        std::string PrimaryWeaponName{};
        std::string SecondaryWeaponName{};
        std::vector<std::string> UtilityNames{};
        bool HasDefuser = false;
        bool HasHelmet = false;
        bool Connected = false;
        bool InBuyZone = false;
        bool Active = false;
        bool HasBomb = false;
        bool IsLocal = false;
        bool IsAlive = false;
    };

    struct RadarPublishFrame
    {
        std::vector<RadarPlayerFrameItem> Players{};
        C4Snapshot C4{};
        std::vector<GrenadeEntitySnapshot> GrenadeEntities{};
        Vector3 LocalViewAngles{};
        std::string MapName{};
        int CtScore = 0;
        int TScore = 0;
        bool CanBuy = false;
        bool FreezePeriod = false;
        int GamePhaseRaw = -1;
        float PhaseEndsIn = 0.0f;
    };

    struct RenderFrame
    {
        std::uint64_t FrameId = 0;
        std::chrono::steady_clock::time_point SampleStart{};
        std::chrono::steady_clock::time_point SampleEnd{};
        std::chrono::steady_clock::time_point PublishTime{};

        std::vector<PlayerEspSnapshot> Players{};
        C4Snapshot C4{};
        std::vector<GrenadeEntitySnapshot> GrenadeEntities{};
        RadarPublishFrame Radar{};
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
        bool InBuyZone = false;
        float FlashDuration = 0.0f;
        float FlashOverlayAlpha = 0.0f;
        float FlashMaxAlpha = 255.0f;
        std::chrono::steady_clock::time_point LastStatusRead{};
        std::string RadarActiveWeapon{};
        std::string RadarPrimaryWeapon{};
        std::string RadarSecondaryWeapon{};
        std::vector<std::string> RadarUtilities{};
        bool RadarHasBomb = false;
        std::chrono::steady_clock::time_point LastInventoryRead{};
    };

    using VisDebugScreenTriangle = WorldDebugScreenTriangle;
    using RenderFramePtr = std::shared_ptr<const RenderFrame>;

    void Render(ImDrawList* drawList);
    void RenderWatermark(ImDrawList* drawList) const;
    void RenderPlayer(ImDrawList* drawList, const PlayerEspSnapshot& player, const Matrix& viewMatrix) const;
    void RenderSkeleton(ImDrawList* drawList, const PlayerEspSnapshot& player, ImU32 color) const;
    void RenderTriggerHitboxDebug(ImDrawList* drawList, const PlayerEspSnapshot& player, const Matrix& viewMatrix) const;
    void RenderC4(ImDrawList* drawList, const C4Snapshot& c4) const;
    void RenderGrenadeEntityEsp(ImDrawList* drawList, const GrenadeEntitySnapshot& grenade) const;
    void RenderSpectatorList(ImDrawList* drawList, const SpectatorListSnapshot& spectatorList) const;
    void RenderGrenadeHelper(ImDrawList* drawList, const GrenadeHelperSnapshot& helper) const;
    void RenderVisCheckDebug(ImDrawList* drawList) const;
    void BuildVisCheckDebugOverlaySnapshot();
    void ClearVisCheckDebugOverlaySnapshot();

    void EnsureSamplerStarted();
    void EnsureRadarPublisherStarted();
    void SamplerLoop();
    void RadarPublisherLoop();
    bool BuildRadarPublishFrameFromMemory(RadarPublishFrame& outFrame);
    std::string BuildRadarPayload(const RadarPublishFrame& frame);
    bool SampleFrame(RenderFrame& outFrame);
    void PublishRenderFrame(RenderFrame&& frame);
    RenderFramePtr LoadRenderFrameSnapshot() const;
    void UpdateRoundEpoch(uint64_t localPawn, bool localAlive);

    bool BuildBoneData(uint64_t boneArray, PlayerEspSnapshot& inOutSnapshot) const;
    C4Snapshot ReadC4Snapshot() const;
    void BuildGrenadeEntitySnapshots(RenderFrame& outFrame, const Vector3& localOrigin);

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
    mutable std::shared_mutex m_VisCheckMutex{};
    std::vector<PendingMapLoad> m_PendingMapLoads{};
    uint64_t m_NextMapRequestId = 0;
    uint64_t m_ActiveMapRequestId = 0;

    std::atomic<bool> m_SamplerStarted{ false };
    std::atomic<bool> m_RadarPublisherStarted{ false };
    std::thread m_SamplerThread{};
    std::thread m_RadarPublisherThread{};
    mutable std::mutex m_RenderFrameMutex{};
    RenderFramePtr m_RenderFrameSnapshot{};
    std::uint64_t m_NextRenderFrameId = 0;
    EspFrameSyncModel::StaleFrameTracker m_RenderStaleTracker{};
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
    std::vector<VisDebugScreenTriangle> m_VisDebugOverlayTriangles{};
    std::string m_GrenadeStatus{};
    std::atomic<bool> m_GrenadeHelperOnlyMode{ false };
    std::atomic<bool> m_GrenadeHelperHoldingUtility{ false };

    std::unordered_map<uint64_t, ControllerIdentityCache> m_ControllerIdentityCache{};
    std::unordered_map<uint64_t, PawnRuntimeCache> m_PawnRuntimeCache{};
    std::unordered_map<uint64_t, int> m_RadarObserverSlots{};
    C4Snapshot m_C4Cache{};
    std::chrono::steady_clock::time_point m_LastC4Sample{};
    std::vector<GrenadeEntitySnapshot> m_GrenadeEntityCache{};
    std::chrono::steady_clock::time_point m_LastGrenadeEntityFrameTick{};
    std::chrono::steady_clock::time_point m_LastGrenadeEntitySample{};
    std::chrono::steady_clock::time_point m_LastGrenadeEntityDiscovery{};
    int m_NextGrenadeEntityScanIndex = 65;
    std::uint64_t m_RoundEpoch = 1;
    std::uint64_t m_LastRoundLocalPawn = 0;
    bool m_LastRoundLocalAlive = false;
    std::chrono::steady_clock::time_point m_LastRoundEpochTick{};
    bool m_LastFreezePeriod = false;
    std::chrono::steady_clock::time_point m_LastFreezeEndTick{};
    std::chrono::steady_clock::time_point m_LastSpectatorDebugLog{};
    std::chrono::steady_clock::time_point m_LastSpectatorSample{};
    SpectatorListSnapshot m_SpectatorListCache{};
    std::chrono::steady_clock::time_point m_LastRadarScoreRead{};
    int m_RadarCtScore = 0;
    int m_RadarTScore = 0;

public:
    void Shutdown();
    bool QueryPenetrationSegments(const Vector3& src, const Vector3& dst, std::vector<VisCheck::PenetrationSegment>& outSegments) const;
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
