#include <cmath>
#include <iostream>
#include <string>

#include "Features/ESP/SoundEspModel.hpp"

namespace
{
    bool Expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }
}

int main()
{
    bool ok = true;

    const auto weapon = SoundEspModel::ClassifySoundName("weapon_ak47_fire");
    ok &= Expect(weapon.Kind == SoundEspModel::SoundKind::WeaponFire, "weapon fire should classify as WeaponFire");
    ok &= Expect(weapon.Color.x > 0.9f && weapon.Color.y < 0.25f, "weapon fire should use red ripple");
    ok &= Expect(weapon.SpeedPxPerSecond >= 420.0f, "weapon fire should expand quickly");

    const auto footstep = SoundEspModel::ClassifySoundName("player_footstep_left");
    ok &= Expect(footstep.Kind == SoundEspModel::SoundKind::Footstep, "footstep should classify as Footstep");
    ok &= Expect(footstep.Color.z > 0.8f, "footstep should use blue ripple");
    ok &= Expect(footstep.LifetimeSeconds < weapon.LifetimeSeconds, "footstep should be shorter than fire");

    const auto jump = SoundEspModel::ClassifySoundName("player_jump");
    ok &= Expect(jump.Kind == SoundEspModel::SoundKind::Jump, "jump should classify as Jump");
    ok &= Expect(jump.VerticalLiftPx > 0.0f, "jump should render upward lift");

    const auto land = SoundEspModel::ClassifySoundName("player_land");
    ok &= Expect(land.Kind == SoundEspModel::SoundKind::Land, "land should classify as Land");
    ok &= Expect(land.Color.x > 0.9f && land.Color.y > 0.35f, "land should use orange ripple");

    const auto pinpull = SoundEspModel::ClassifySoundName("grenade_pinpull");
    ok &= Expect(pinpull.Kind == SoundEspModel::SoundKind::Grenade, "pinpull should classify as Grenade");
    ok &= Expect(pinpull.Color.x > 0.9f && pinpull.Color.y > 0.8f, "grenade should use yellow ripple");

    ok &= Expect(SoundEspModel::ShouldAcceptDistance(1999.0f, 2000.0f), "distance under max should be accepted");
    ok &= Expect(!SoundEspModel::ShouldAcceptDistance(2001.0f, 2000.0f), "distance over max should be rejected");
    ok &= Expect(std::abs(SoundEspModel::GroundRippleRadius(footstep, 1.0f) - 120.5f) < 0.01f, "ground ripple radius should use reduced animation diameter");
    ok &= Expect(SoundEspModel::ShouldPollPawnSoundsForVisualState(true, false), "sound esp should poll pawn sounds when ripple rendering is enabled");
    ok &= Expect(SoundEspModel::ShouldPollPawnSoundsForVisualState(false, true), "legit mode should poll pawn sounds even when ripple rendering is disabled");
    ok &= Expect(!SoundEspModel::ShouldPollPawnSoundsForVisualState(false, false), "sound polling should stop when both sound esp and legit mode are disabled");
    ok &= Expect(SoundEspModel::ShouldRenderPlayerInfo(false, false, false), "normal mode should render player info without sound");
    ok &= Expect(SoundEspModel::ShouldRenderPlayerInfo(true, true, false), "legit mode should render player info while sound ripple is active");
    ok &= Expect(SoundEspModel::ShouldRenderPlayerInfo(true, false, true), "legit mode should render visible player info without active sound ripple");
    ok &= Expect(!SoundEspModel::ShouldRenderPlayerInfo(true, false, false), "legit mode should hide player info without active sound ripple");
    ok &= Expect(SoundEspModel::ShouldRenderSoundRipples(true, false, false), "sound esp on with esp off and legit off should render only ripples");
    ok &= Expect(SoundEspModel::ShouldRenderSoundRipples(true, true, true), "sound esp on should render ripples independently of esp and legit state");
    ok &= Expect(!SoundEspModel::ShouldRenderSoundRipples(false, true, true), "sound esp off should not render ripples even when legit mode samples sounds");
    ok &= Expect(SoundEspModel::ResolveSoundReferencePawn(0x11110000, 0x22220000) == 0x22220000, "spectating should use observer target pawn as sound reference");
    ok &= Expect(SoundEspModel::ResolveSoundReferencePawn(0x11110000, 0) == 0x11110000, "non-spectating should use local pawn as sound reference");
    ok &= Expect(SoundEspModel::ShouldPollPawnSoundsFromLocalState(true, 0, 1), "dead local player should not stop enemy sound polling");
    ok &= Expect(!SoundEspModel::ShouldPollPawnSoundsFromLocalState(false, 100, 0), "failed local state read should stop sound polling");

    return ok ? 0 : 1;
}
