#pragma once
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
    float FlashDuration = 0.0f;

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
    Vector3 Position{};
    Vector2 Screen{};
    bool OnScreen = false;
};

class ESP
{
private:
    struct PendingMapLoad
    {
        uint64_t RequestId = 0;
        std::string MapName{};
        std::string OptPath{};
        std::future<std::unique_ptr<VisCheck>> Future{};
    };

    struct RenderFrame
    {
        std::vector<PlayerEspSnapshot> Players{};
        C4Snapshot C4{};
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
        float FlashDuration = 0.0f;
        std::chrono::steady_clock::time_point LastStatusRead{};
    };

    void Render(ImDrawList* drawList);
    void RenderWatermark(ImDrawList* drawList) const;
    void RenderPlayer(ImDrawList* drawList, const PlayerEspSnapshot& player) const;
    void RenderSkeleton(ImDrawList* drawList, const PlayerEspSnapshot& player, ImU32 color) const;
    void RenderC4(ImDrawList* drawList, const C4Snapshot& c4) const;

    void EnsureSamplerStarted();
    void SamplerLoop();
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

    void UpdateVisCheckState();
    void RequestMapLoad(const std::string& mapName, const std::string& optPath);
    void ConsumeMapLoadResult();
    std::string ResolveOptPath(const std::string& mapName) const;
    std::string BuildMapStatus(const std::string& mapName, const char* suffix) const;
    bool CheckVisibility(const Vector3& src, const Vector3& dst) const;

private:
    std::unique_ptr<VisCheck> m_VisCheck{};
    std::vector<PendingMapLoad> m_PendingMapLoads{};
    uint64_t m_NextMapRequestId = 0;
    uint64_t m_ActiveMapRequestId = 0;

    std::atomic<bool> m_SamplerStarted{ false };
    mutable std::mutex m_RenderFrameMutex{};
    RenderFrame m_RenderFrame{};

    std::string m_CurrentMapName{};
    std::string m_CurrentOptPath{};
    std::string m_MapStatus = "Map Status: (Waiting)";
    std::string m_LastPolledMapName{};
    std::chrono::steady_clock::time_point m_LastMapPoll{};
    bool m_VisCheckEnabled = false;

    std::unordered_map<uint64_t, ControllerIdentityCache> m_ControllerIdentityCache{};
    std::unordered_map<uint64_t, PawnRuntimeCache> m_PawnRuntimeCache{};
    C4Snapshot m_C4Cache{};
    std::chrono::steady_clock::time_point m_LastC4Sample{};
    std::uint64_t m_RoundEpoch = 1;
    std::uint64_t m_LastRoundLocalPawn = 0;
    bool m_LastRoundLocalAlive = false;
    std::chrono::steady_clock::time_point m_LastRoundEpochTick{};
    bool m_LastFreezePeriod = false;
    std::chrono::steady_clock::time_point m_LastFreezeEndTick{};

public:
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
