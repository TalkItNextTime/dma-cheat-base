#include <Pch.hpp>

#include "ObservRadarProtocol.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "json.hpp"

namespace
{
    std::string TrimAscii(const std::string& value)
    {
        const auto isSpace = [](const unsigned char ch) { return std::isspace(ch) != 0; };

        std::size_t begin = 0;
        while (begin < value.size() && isSpace(static_cast<unsigned char>(value[begin])))
            ++begin;

        std::size_t end = value.size();
        while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1])))
            --end;

        return value.substr(begin, end - begin);
    }

    int ClampPort(const int value, const int fallback)
    {
        if (value < 1 || value > 65535)
            return fallback;
        return value;
    }

    nlohmann::json EncodeVec3(const Vector3& value)
    {
        return {
            { "x", value.x },
            { "y", value.y },
            { "z", value.z },
        };
    }

    float NormalizeAngleImpl(const float yawDegrees)
    {
        float angle = std::fmod(90.0f - yawDegrees, 360.0f);
        if (angle < 0.0f)
            angle += 360.0f;
        return angle;
    }
}

std::string NormalizeObservRadarHost(const std::string& rawHost)
{
    std::string host = TrimAscii(rawHost);
    if (host.empty())
        return "127.0.0.1";

    const char* prefixes[] = { "http://", "https://", "ws://", "wss://" };
    for (const char* prefix : prefixes)
    {
        const std::string prefixText(prefix);
        if (host.rfind(prefixText, 0) == 0)
        {
            host.erase(0, prefixText.size());
            break;
        }
    }

    while (!host.empty() && (host.back() == '/' || host.back() == '\\'))
        host.pop_back();

    if (host.empty())
        return "127.0.0.1";

    return host;
}

std::string BuildObservRadarStaticUrl(const ObservRadarEndpointConfig& config)
{
    return "http://" + NormalizeObservRadarHost(config.Host) + ":" + std::to_string(ClampPort(config.StaticPort, 36364));
}

std::string BuildObservRadarWebSocketUrl(const ObservRadarEndpointConfig& config)
{
    return "ws://" + NormalizeObservRadarHost(config.Host) + ":" + std::to_string(ClampPort(config.WebSocketPort, 36365));
}

float NormalizeObservRadarAngle(const float yawDegrees)
{
    return NormalizeAngleImpl(yawDegrees);
}

std::string ResolveObservRadarRoundState(
    const bool freezePeriod,
    const bool bombPlanted,
    const bool beingDefused,
    const int gamePhaseRaw)
{
    if (freezePeriod)
        return "freezetime";
    if (bombPlanted || beingDefused)
        return "live";

    switch (gamePhaseRaw)
    {
    case 0:
        return "freezetime";
    case 1:
        return "live";
    case 2:
        return "over";
    default:
        break;
    }

    return "live";
}

std::string ResolveObservRadarBombState(const ObservRadarBombBuildState& state)
{
    if (state.BombPlanted)
    {
        if (state.BombDefused)
            return "defused";
        if (state.BombBeingDefused)
            return "defusing";
        if (state.BombTimeRemaining <= 0.0f)
            return "exploded";
        return "planted";
    }

    if (state.BombOwnerPresent)
    {
        if (state.BombOwnerStartedArming || state.BombOwnerPlantingViaUse)
            return "planting";
        return "carried";
    }

    if (!state.BombValid)
        return "carried";

    return "dropped";
}

std::string BuildObservRadarPayload(const ObservRadarFrame& frame)
{
    using nlohmann::json;

    json players = json::array();
    for (const ObservRadarPlayerPacket& player : frame.Players)
    {
        json ammo = json::object();
        for (const ObservRadarAmmoEntry& entry : player.Ammo)
        {
            if (!entry.Weapon.empty())
                ammo[entry.Weapon] = entry.Count;
        }

        players.push_back({
            { "id", player.Id },
            { "steamid", player.SteamId.empty() ? player.Id : player.SteamId },
            { "num", player.Num },
            { "name", player.Name },
            { "team", player.Team },
            { "health", player.Health },
            { "armor", player.Armor },
            { "money", player.Money },
            { "hasHelmet", player.HasHelmet },
            { "hasDefuser", player.HasDefuser },
            { "connected", player.Connected },
            { "active", player.Active },
            { "flashed", player.Flashed },
            { "bomb", player.Bomb },
            { "ammo", std::move(ammo) },
            { "position", EncodeVec3(player.Position) },
            { "angle", player.Angle },
            { "active_weapon", player.ActiveWeapon },
            { "primaryWeapon", player.PrimaryWeapon },
            { "secondaryWeapon", player.SecondaryWeapon },
            { "utilities", player.Utilities },
        });
    }

    json smokes = json::array();
    for (const ObservRadarSmokePacket& smoke : frame.Smokes)
    {
        smokes.push_back({
            { "id", smoke.Id },
            { "time", smoke.Time },
            { "team", smoke.Team },
            { "position", EncodeVec3(smoke.Position) },
        });
    }

    json flashbangs = json::array();
    for (const ObservRadarFlashbangPacket& flashbang : frame.Flashbangs)
    {
        flashbangs.push_back({
            { "id", flashbang.Id },
            { "position", EncodeVec3(flashbang.Position) },
        });
    }

    json infernos = json::array();
    for (const ObservRadarInfernoPacket& inferno : frame.Infernos)
    {
        json flames = json::array();
        for (const Vector3& flame : inferno.FlamesPosition)
            flames.push_back(EncodeVec3(flame));

        infernos.push_back({
            { "id", inferno.Id },
            { "flamesNum", static_cast<int>(inferno.FlamesPosition.size()) },
            { "flamesPosition", std::move(flames) },
        });
    }

    json projectiles = json::array();
    for (const ObservRadarProjectilePacket& projectile : frame.Projectiles)
    {
        projectiles.push_back({
            { "id", projectile.Id },
            { "type", projectile.Type },
            { "team", projectile.Team },
            { "position", EncodeVec3(projectile.Position) },
        });
    }

    json packets = json::array();
    packets.push_back({
        { "type", "provider" },
        { "data", {
            { "name", frame.ProviderName },
            { "appid", 730 },
            { "timestamp", frame.ProviderTimestampMs },
        } },
    });
    packets.push_back({
        { "type", "map" },
        { "data", frame.MapName },
    });
    packets.push_back({
        { "type", "score" },
        { "data", {
            { "ct", frame.Score.Ct },
            { "t", frame.Score.T },
        } },
    });
    packets.push_back({
        { "type", "round" },
        { "data", frame.RoundState },
    });
    packets.push_back({
        { "type", "canbuy" },
        { "data", frame.CanBuy },
    });
    packets.push_back({
        { "type", "bomb" },
        { "data", {
            { "state", frame.Bomb.State },
            { "player", frame.Bomb.Player },
            { "countdown", frame.Bomb.Countdown },
            { "defuseCountdown", frame.Bomb.DefuseCountdown },
            { "plantCountdown", frame.Bomb.PlantCountdown },
            { "bombTicking", frame.Bomb.BombTicking },
            { "timerLength", frame.Bomb.TimerLength },
            { "plantLength", frame.Bomb.PlantLength },
            { "site", frame.Bomb.Site },
            { "hasDefuseKit", frame.Bomb.HasDefuseKit },
            { "position", EncodeVec3(frame.Bomb.Position) },
        } },
    });
    packets.push_back({
        { "type", "players" },
        { "data", {
            { "players", std::move(players) },
        } },
    });
    packets.push_back({
        { "type", "smokes" },
        { "data", std::move(smokes) },
    });
    packets.push_back({
        { "type", "flashbangs" },
        { "data", std::move(flashbangs) },
    });
    packets.push_back({
        { "type", "infernos" },
        { "data", std::move(infernos) },
    });
    packets.push_back({
        { "type", "projectiles" },
        { "data", std::move(projectiles) },
    });

    return packets.dump();
}
