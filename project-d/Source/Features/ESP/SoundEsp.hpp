#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Math/Vector.hpp"
#include "SoundEspModel.hpp"
#include "imgui/imgui.h"

struct SoundRippleSnapshot
{
    Vector3 Origin{};
    std::string Name{};
    std::uint32_t EntityHandle = 0;
    std::uint64_t Pawn = 0;
    float Volume = 1.0f;
    float AgeSeconds = 0.0f;
    SoundEspModel::RippleStyle Style{};
};

class SoundEsp
{
public:
    static SoundEsp& Get();

    void EnsureStarted();
    void Shutdown();
    std::vector<SoundRippleSnapshot> GetRipplesSnapshot();
    bool HasRecentSound(std::uint64_t pawn) const;
    void RenderRipples(ImDrawList* drawList, const std::vector<SoundRippleSnapshot>& ripples) const;

private:
    struct TrackedPawnSound
    {
        float LastEmitTime = 0.0f;
        std::chrono::steady_clock::time_point LastAccepted{};
    };

    struct ActiveRipple
    {
        Vector3 Origin{};
        std::string Name{};
        std::uint32_t EntityHandle = 0;
        std::uint64_t Pawn = 0;
        float Volume = 1.0f;
        std::chrono::steady_clock::time_point CreatedAt{};
        SoundEspModel::RippleStyle Style{};
    };

    void PollLoop();
    void PollPawnEmitSoundTimes();
    void PushRipple(
        const Vector3& origin,
        std::string name,
        std::uint32_t entityHandle,
        std::uint64_t pawn,
        float volume);

    static float Distance3D(const Vector3& a, const Vector3& b);
    static bool IsAlive(int health, int lifeState);
    static bool IsLikelyUserAddress(std::uint64_t address);

private:
    std::atomic<bool> m_Started{ false };
    std::thread m_PollThread{};
    mutable std::mutex m_Mutex{};
    std::unordered_map<std::uint64_t, TrackedPawnSound> m_PawnSoundTimes{};
    std::vector<ActiveRipple> m_Ripples{};
};

inline SoundEsp& soundEsp = SoundEsp::Get();
