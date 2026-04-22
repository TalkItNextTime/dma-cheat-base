#include <cmath>
#include <iostream>
#include <string>

#include "json.hpp"

#include "Features/Radar/ObservRadarProtocol.hpp"

namespace
{
    using nlohmann::json;

    bool ExpectTrue(const bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }

    bool ExpectEqual(const std::string& actual, const std::string& expected, const std::string& message)
    {
        if (actual == expected)
            return true;

        std::cerr << "[FAIL] " << message << "\n"
                  << "  expected: " << expected << "\n"
                  << "  actual  : " << actual << '\n';
        return false;
    }

    bool ExpectEqual(const int actual, const int expected, const std::string& message)
    {
        if (actual == expected)
            return true;

        std::cerr << "[FAIL] " << message << "\n"
                  << "  expected: " << expected << "\n"
                  << "  actual  : " << actual << '\n';
        return false;
    }

    bool ExpectNear(const double actual, const double expected, const double epsilon, const std::string& message)
    {
        if (std::abs(actual - expected) <= epsilon)
            return true;

        std::cerr << "[FAIL] " << message << "\n"
                  << "  expected: " << expected << "\n"
                  << "  actual  : " << actual << '\n';
        return false;
    }
}

int main()
{
    bool ok = true;

    ok &= ExpectEqual(
        NormalizeObservRadarHost(" ws://radar.example.com/ "),
        "radar.example.com",
        "host normalization should strip protocol, whitespace, and trailing slash");

    const ObservRadarEndpointConfig endpoint{
        .Host = "http://10.10.10.2/",
        .StaticPort = 8080,
        .WebSocketPort = 8090,
    };

    ok &= ExpectEqual(
        BuildObservRadarStaticUrl(endpoint),
        "http://10.10.10.2:8080",
        "static page URL should honor custom host and static port");

    ok &= ExpectEqual(
        BuildObservRadarWebSocketUrl(endpoint),
        "ws://10.10.10.2:8090",
        "websocket URL should honor custom host and websocket port");

    ok &= ExpectNear(
        NormalizeObservRadarAngle(180.0f),
        270.0,
        0.001,
        "angle normalization should rotate yaw into the radar coordinate system");

    ok &= ExpectEqual(
        ResolveObservRadarBombState({
            .BombValid = true,
            .BombPlanted = false,
            .BombDefused = false,
            .BombBeingDefused = false,
            .BombTimeRemaining = 0.0f,
            .BombOwnerPresent = true,
            .BombOwnerStartedArming = true,
            .BombOwnerPlantingViaUse = false,
        }),
        "planting",
        "bomb state should report planting when the owner started arming even if weapon heuristics disagree");

    ok &= ExpectEqual(
        ResolveObservRadarBombState({
            .BombValid = true,
            .BombPlanted = false,
            .BombDefused = false,
            .BombBeingDefused = false,
            .BombTimeRemaining = 0.0f,
            .BombOwnerPresent = true,
            .BombOwnerStartedArming = false,
            .BombOwnerPlantingViaUse = false,
        }),
        "carried",
        "bomb state should stay carried when no arming signal is present");

    ObservRadarFrame frame{};
    frame.ProviderName = "project-d";
    frame.ProviderTimestampMs = 123456789ULL;
    frame.MapName = "de_mirage";
    frame.RoundState = "live";
    frame.Score.Ct = 8;
    frame.Score.T = 6;
    frame.CanBuy = true;
    frame.Bomb.State = "planted";
    frame.Bomb.Player = "76561198000000001";
    frame.Bomb.Countdown = 31.2f;
    frame.Bomb.DefuseCountdown = 0.0f;
    frame.Bomb.PlantCountdown = 0.0f;
    frame.Bomb.BombTicking = true;
    frame.Bomb.TimerLength = 40.0f;
    frame.Bomb.PlantLength = 3.2f;
    frame.Bomb.Site = "A";
    frame.Bomb.HasDefuseKit = false;
    frame.Bomb.Position = { 120.0f, -350.0f, 18.0f };

    ObservRadarPlayerPacket player{};
    player.Id = "76561198000000001";
    player.SteamId = "76561198000000001";
    player.Num = 3;
    player.Name = "Player";
    player.Team = "CT";
    player.Health = 87;
    player.Armor = 100;
    player.Money = 5400;
    player.HasHelmet = true;
    player.HasDefuser = true;
    player.Connected = true;
    player.Active = true;
    player.Flashed = 0;
    player.Bomb = false;
    player.Ammo.push_back({ "weapon_m4a1", 25 });
    player.Position = { 10.0f, 20.0f, 5.0f };
    player.Angle = 180.0f;
    player.ActiveWeapon = "weapon_m4a1";
    player.PrimaryWeapon = "weapon_m4a1";
    player.SecondaryWeapon = "weapon_hkp2000";
    player.Utilities = { "weapon_flashbang", "weapon_smokegrenade" };
    frame.Players.push_back(player);

    frame.Smokes.push_back({ "smoke-1", 5.2f, "CT", { 1.0f, 2.0f, 3.0f } });
    frame.Flashbangs.push_back({ "flash-1", { 4.0f, 5.0f, 6.0f } });
    frame.Infernos.push_back({ "inferno-1", { { 7.0f, 8.0f, 9.0f }, { 10.0f, 11.0f, 12.0f } } });
    frame.Projectiles.push_back({ "projectile-1", "smoke", "T", { 13.0f, 14.0f, 15.0f } });

    const std::string payloadText = BuildObservRadarPayload(frame);
    const json payload = json::parse(payloadText, nullptr, false);

    ok &= ExpectTrue(payload.is_array(), "payload should be a JSON array");
    ok &= ExpectEqual(static_cast<int>(payload.size()), 11, "payload should emit the expected event count");
    if (payload.is_array() && payload.size() == 11)
    {
        ok &= ExpectEqual(payload[0]["type"].get<std::string>(), "provider", "event 0 should be provider");
        ok &= ExpectEqual(payload[1]["type"].get<std::string>(), "map", "event 1 should be map");
        ok &= ExpectEqual(payload[2]["type"].get<std::string>(), "score", "event 2 should be score");
        ok &= ExpectEqual(payload[3]["type"].get<std::string>(), "round", "event 3 should be round");
        ok &= ExpectEqual(payload[4]["type"].get<std::string>(), "canbuy", "event 4 should be canbuy");
        ok &= ExpectEqual(payload[5]["type"].get<std::string>(), "bomb", "event 5 should be bomb");
        ok &= ExpectEqual(payload[6]["type"].get<std::string>(), "players", "event 6 should be players");
        ok &= ExpectEqual(payload[7]["type"].get<std::string>(), "smokes", "event 7 should be smokes");
        ok &= ExpectEqual(payload[8]["type"].get<std::string>(), "flashbangs", "event 8 should be flashbangs");
        ok &= ExpectEqual(payload[9]["type"].get<std::string>(), "infernos", "event 9 should be infernos");
        ok &= ExpectEqual(payload[10]["type"].get<std::string>(), "projectiles", "event 10 should be projectiles");
    }

    if (payload.is_array() && payload.size() > 6)
    {
        const json& players = payload[6]["data"]["players"];
        ok &= ExpectTrue(players.is_array(), "players event should contain a player array");
        ok &= ExpectEqual(static_cast<int>(players.size()), 1, "players event should contain the sample player");
        if (players.is_array() && !players.empty())
        {
            const json& encodedPlayer = players[0];
            ok &= ExpectEqual(encodedPlayer["id"].get<std::string>(), "76561198000000001", "player id should be preserved");
            ok &= ExpectEqual(encodedPlayer["num"].get<int>(), 3, "player slot should be preserved");
            ok &= ExpectEqual(encodedPlayer["team"].get<std::string>(), "CT", "player team should be preserved");
            ok &= ExpectNear(encodedPlayer["position"]["x"].get<double>(), 10.0, 0.001, "player position.x should be numeric");
            ok &= ExpectEqual(encodedPlayer["active_weapon"].get<std::string>(), "weapon_m4a1", "active weapon should be preserved");
        }
    }

    if (!ok)
        return 1;

    std::cout << "[PASS] observ_radar_protocol_tests\n";
    return 0;
}
