#pragma once

enum class C4CardMode
{
    Hidden,
    Planting,
    Planted,
};

struct C4CardInput
{
    bool Valid = false;
    bool Planted = false;
    bool StartedArming = false;
    bool PlantingViaUse = false;
    int BombSite = -1;
    bool BeingDefused = false;
    float TimeRemaining = 0.0f;
    float DefuseCountDown = 0.0f;
    bool CanDefuse = false;
    float PlantCountdown = 0.0f;
};

struct C4CardModel
{
    C4CardMode Mode = C4CardMode::Hidden;
    int BombSite = -1;
    bool BeingDefused = false;
    float TimeRemaining = 0.0f;
    float DefuseCountDown = 0.0f;
    bool CanDefuse = false;
    float PlantCountdown = 0.0f;
};

C4CardModel BuildC4CardModel(const C4CardInput& input);
