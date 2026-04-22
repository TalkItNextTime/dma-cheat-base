#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Math/Vector.hpp"

struct ObservRadarEndpointConfig
{
    std::string Host = "127.0.0.1";
    int StaticPort = 36364;
    int WebSocketPort = 36365;
};

struct ObservRadarAmmoEntry
{
    std::string Weapon{};
    int Count = 0;
};

struct ObservRadarPlayerPacket
{
    std::string Id{};
    std::string SteamId{};
    int Num = 0;
    std::string Name{};
    std::string Team{};
    int Health = 0;
    int Armor = 0;
    int Money = 0;
    bool HasHelmet = false;
    bool HasDefuser = false;
    bool Connected = false;
    bool Active = false;
    int Flashed = 0;
    bool Bomb = false;
    std::vector<ObservRadarAmmoEntry> Ammo{};
    Vector3 Position{};
    float Angle = 0.0f;
    std::string ActiveWeapon{};
    std::string PrimaryWeapon{};
    std::string SecondaryWeapon{};
    std::vector<std::string> Utilities{};
};

struct ObservRadarScorePacket
{
    int Ct = 0;
    int T = 0;
};

struct ObservRadarBombPacket
{
    std::string State{};
    std::string Player{};
    float Countdown = 0.0f;
    float DefuseCountdown = 0.0f;
    float PlantCountdown = 0.0f;
    bool BombTicking = false;
    float TimerLength = 40.0f;
    float PlantLength = 3.2f;
    std::string Site{};
    bool HasDefuseKit = false;
    Vector3 Position{};
};

struct ObservRadarSmokePacket
{
    std::string Id{};
    float Time = 0.0f;
    std::string Team{};
    Vector3 Position{};
};

struct ObservRadarFlashbangPacket
{
    std::string Id{};
    Vector3 Position{};
};

struct ObservRadarInfernoPacket
{
    std::string Id{};
    std::vector<Vector3> FlamesPosition{};
};

struct ObservRadarProjectilePacket
{
    std::string Id{};
    std::string Type{};
    std::string Team{};
    Vector3 Position{};
};

struct ObservRadarFrame
{
    std::string ProviderName = "project-d";
    std::uint64_t ProviderTimestampMs = 0;
    std::string MapName{};
    std::string RoundState = "live";
    ObservRadarScorePacket Score{};
    bool CanBuy = false;
    ObservRadarBombPacket Bomb{};
    std::vector<ObservRadarPlayerPacket> Players{};
    std::vector<ObservRadarSmokePacket> Smokes{};
    std::vector<ObservRadarFlashbangPacket> Flashbangs{};
    std::vector<ObservRadarInfernoPacket> Infernos{};
    std::vector<ObservRadarProjectilePacket> Projectiles{};
};

struct ObservRadarBombBuildState
{
    bool BombValid = false;
    bool BombPlanted = false;
    bool BombDefused = false;
    bool BombBeingDefused = false;
    float BombTimeRemaining = 0.0f;
    bool BombOwnerPresent = false;
    bool BombOwnerStartedArming = false;
    bool BombOwnerPlantingViaUse = false;
};

std::string NormalizeObservRadarHost(const std::string& rawHost);
std::string BuildObservRadarStaticUrl(const ObservRadarEndpointConfig& config);
std::string BuildObservRadarWebSocketUrl(const ObservRadarEndpointConfig& config);
float NormalizeObservRadarAngle(const float yawDegrees);
std::string ResolveObservRadarRoundState(bool freezePeriod, bool bombPlanted, bool beingDefused, int gamePhaseRaw);
std::string ResolveObservRadarBombState(const ObservRadarBombBuildState& state);
std::string BuildObservRadarPayload(const ObservRadarFrame& frame);
