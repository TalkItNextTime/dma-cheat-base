#include <Pch.hpp>
#include <SDK.hpp>

#include "SoundEsp.hpp"

namespace
{
    constexpr auto kPollInterval = std::chrono::milliseconds(12);
    constexpr auto kDuplicateWindow = std::chrono::milliseconds(180);
    constexpr float kMaxSoundDistance = 2000.0f;
    constexpr float kMinEmitDelta = 0.0001f;
    constexpr float kPawnSoundOriginLift = 8.0f;
    constexpr int kMaxControllerSlots = 64;
    constexpr int kGroundCircleSegments = 64;
}

SoundEsp& SoundEsp::Get()
{
    static SoundEsp instance;
    return instance;
}

void SoundEsp::EnsureStarted()
{
    bool expected = false;
    if (!m_Started.compare_exchange_strong(expected, true))
        return;

    m_PollThread = std::thread([this]()
    {
        PollLoop();
    });
}

void SoundEsp::Shutdown()
{
    if (m_PollThread.joinable())
        m_PollThread.join();

    m_Started.store(false);
}

std::vector<SoundRippleSnapshot> SoundEsp::GetRipplesSnapshot()
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<SoundRippleSnapshot> snapshots{};

    std::lock_guard lock(m_Mutex);
    for (auto it = m_Ripples.begin(); it != m_Ripples.end();)
    {
        const float age = std::chrono::duration<float>(now - it->CreatedAt).count();
        if (age >= it->Style.LifetimeSeconds)
        {
            it = m_Ripples.erase(it);
            continue;
        }

        SoundRippleSnapshot snapshot{};
        snapshot.Origin = it->Origin;
        snapshot.Name = it->Name;
        snapshot.EntityHandle = it->EntityHandle;
        snapshot.Pawn = it->Pawn;
        snapshot.Volume = it->Volume;
        snapshot.AgeSeconds = age;
        snapshot.Style = it->Style;
        snapshots.push_back(std::move(snapshot));
        ++it;
    }

    return snapshots;
}

bool SoundEsp::HasRecentSound(const std::uint64_t pawn) const
{
    if (!pawn)
        return false;

    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(m_Mutex);
    for (const ActiveRipple& ripple : m_Ripples)
    {
        if (ripple.Pawn != pawn)
            continue;

        const float age = std::chrono::duration<float>(now - ripple.CreatedAt).count();
        if (age >= 0.0f && age < ripple.Style.LifetimeSeconds)
            return true;
    }

    return false;
}

void SoundEsp::RenderRipples(ImDrawList* drawList, const std::vector<SoundRippleSnapshot>& ripples) const
{
    if (!drawList || !config.Visuals.SoundEsp)
        return;

    for (const SoundRippleSnapshot& ripple : ripples)
    {
        const float lifetime = (std::max)(0.05f, ripple.Style.LifetimeSeconds);
        const float t = std::clamp(ripple.AgeSeconds / lifetime, 0.0f, 1.0f);
        const float alpha = (1.0f - t) * ripple.Style.Color.w * std::clamp(ripple.Volume, 0.35f, 1.4f);
        if (alpha <= 0.01f)
            continue;

        ImVec4 color = ripple.Style.Color;
        color.w = std::clamp(alpha, 0.0f, 1.0f);
        const ImU32 lineColor = ImGui::GetColorU32(color);
        const float radius = SoundEspModel::GroundRippleRadius(ripple.Style, ripple.AgeSeconds);
        const float verticalLift = ripple.Style.VerticalLiftPx * t;
        const Vector3 center = ripple.Origin + Vector3{ 0.0f, 0.0f, kPawnSoundOriginLift + verticalLift };

        std::vector<ImVec2> points{};
        points.reserve(kGroundCircleSegments);
        for (int index = 0; index < kGroundCircleSegments; ++index)
        {
            const float angle = (static_cast<float>(index) / static_cast<float>(kGroundCircleSegments)) * 6.28318530718f;
            const Vector3 worldPoint = center + Vector3{ std::cos(angle) * radius, std::sin(angle) * radius, 0.0f };
            Vector2 screenPoint{};
            if (sdk.WorldToScreen(worldPoint, screenPoint))
                points.emplace_back(screenPoint.x, screenPoint.y);
        }

        if (points.size() >= 3)
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()), lineColor, ImDrawFlags_Closed, ripple.Style.Thickness);
    }
}

void SoundEsp::PollLoop()
{
    while (Globals::Running)
    {
        if (config.Visuals.SoundEsp)
            PollPawnEmitSoundTimes();

        std::this_thread::sleep_for(kPollInterval);
    }
}

void SoundEsp::PollPawnEmitSoundTimes()
{
    if (!Offsets::Schema::m_flEmitSoundTime || !Offsets::Schema::m_vOldOrigin)
        return;

    const auto core = sdk.GetCoreCache();
    if (!core.IsValid || !core.LocalPawn || !core.EntityList)
        return;

    int localHealth = 0;
    int localTeam = 0;
    int localLifeState = 0;
    const bool localStateReadOk = sdk.ReadBasicEntityState(core.LocalPawn, localHealth, localTeam, localLifeState);
    if (!SoundEspModel::ShouldPollPawnSoundsFromLocalState(localStateReadOk, localHealth, localLifeState))
    {
        return;
    }

    const Vector3 localOrigin = mem.Read<Vector3>(core.LocalPawn + Offsets::Schema::m_vOldOrigin);
    const auto now = std::chrono::steady_clock::now();

    for (int slot = 1; slot <= kMaxControllerSlots; ++slot)
    {
        const std::uint64_t controller = sdk.ResolveEntityFromHandle(static_cast<std::uint32_t>(slot), core.EntityList);
        if (!IsLikelyUserAddress(controller))
            continue;

        const std::uint64_t pawn = sdk.ResolvePawnFromController(controller);
        if (!IsLikelyUserAddress(pawn) || pawn == core.LocalPawn)
            continue;

        int health = 0;
        int team = 0;
        int lifeState = 0;
        if (!sdk.ReadBasicEntityState(pawn, health, team, lifeState) || !IsAlive(health, lifeState))
            continue;

        if (config.Visuals.TeamCheck && team == localTeam)
            continue;

        const Vector3 origin = mem.Read<Vector3>(pawn + Offsets::Schema::m_vOldOrigin);
        const float distance = Distance3D(localOrigin, origin);
        if (!SoundEspModel::ShouldAcceptDistance(distance, kMaxSoundDistance))
            continue;

        const float emitTime = mem.Read<float>(pawn + Offsets::Schema::m_flEmitSoundTime);
        if (!std::isfinite(emitTime) || emitTime <= 0.0f)
            continue;

        TrackedPawnSound& tracked = m_PawnSoundTimes[pawn];
        const bool changed = std::abs(emitTime - tracked.LastEmitTime) > kMinEmitDelta;
        const bool duplicateWindow = tracked.LastAccepted.time_since_epoch().count() != 0 &&
            now - tracked.LastAccepted < kDuplicateWindow;

        if (!changed || duplicateWindow)
            continue;

        tracked.LastEmitTime = emitTime;
        tracked.LastAccepted = now;

        PushRipple(origin, "player_emit_sound", static_cast<std::uint32_t>(slot), pawn, 0.75f);
    }
}

void SoundEsp::PushRipple(
    const Vector3& origin,
    std::string name,
    const std::uint32_t entityHandle,
    const std::uint64_t pawn,
    const float volume)
{
    SoundEspModel::RippleStyle style = SoundEspModel::ClassifySoundName(name);
    if (style.Kind == SoundEspModel::SoundKind::Unknown)
    {
        style.Kind = SoundEspModel::SoundKind::Footstep;
        style.Color = ImVec4(0.25f, 0.62f, 1.0f, 1.0f);
        style.SpeedPxPerSecond = 210.0f;
        style.LifetimeSeconds = 1.15f;
        style.Thickness = 1.3f;
        style.Strength = 0.55f;
    }

    ActiveRipple ripple{};
    ripple.Origin = origin;
    ripple.Name = std::move(name);
    ripple.EntityHandle = entityHandle;
    ripple.Pawn = pawn;
    ripple.Volume = std::clamp(volume, 0.25f, 1.5f);
    ripple.CreatedAt = std::chrono::steady_clock::now();
    ripple.Style = style;

    std::lock_guard lock(m_Mutex);
    m_Ripples.push_back(std::move(ripple));
    if (m_Ripples.size() > 128)
        m_Ripples.erase(m_Ripples.begin(), m_Ripples.begin() + static_cast<std::ptrdiff_t>(m_Ripples.size() - 128));
}

float SoundEsp::Distance3D(const Vector3& a, const Vector3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool SoundEsp::IsAlive(const int health, const int lifeState)
{
    return health > 0 && health <= 200 && (lifeState == 0 || lifeState == 256);
}

bool SoundEsp::IsLikelyUserAddress(const std::uint64_t address)
{
    return address > 0x10000ULL && address < 0x00007FFFFFFFFFFFULL;
}
