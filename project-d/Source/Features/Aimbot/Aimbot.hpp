#pragma once
#include <Pch.hpp>
#include <atomic>

class Aimbot
{
public:
	void Update();
	void UpdateAimbot();
	void UpdateTriggerbot();
	void UpdateFlickbot();
	float GetCurrentFovRadiusPx() const;
	float GetCurrentTriggerHitboxRadiusPx() const;
	float GetCurrentTriggerHeadRadiusPx() const;
	std::uint64_t GetCurrentTriggerBoneMask() const;
	int GetCurrentTriggerDetectMode() const;
	bool IsAimbotHotkeyActiveVisual() const;
	bool HasAimbotTargetVisual() const;
	bool IsTriggerHotkeyActiveVisual() const;
	bool HasTriggerTargetVisual() const;
	bool IsFlickHotkeyActiveVisual() const;
	bool HasFlickTargetVisual() const;
	std::uint64_t GetFlickAutowallTargetPawnVisual() const;

	static Aimbot& Get()
	{
		static Aimbot instance;
		return instance;
	}

private:
	struct KeybindState
	{
		bool ToggleState = false;
		bool WasDown = false;
	};

	bool IsKeybindActive(int virtualKey, int mode, bool& toggleState, bool& wasDown);
	bool IsAnyAimbotHotkeyActive();
	bool IsAnyTriggerHotkeyActive();
	void TriggerFireClick(int holdMs);
	void ResetTriggerWindow();
	void ReleaseTriggerMouseIfHeld();
	void UpdateTriggerMouseState(const std::chrono::steady_clock::time_point& now);

private:
	std::mutex m_KmboxMutex{};

	KeybindState m_AimbotPrimaryKey{};
	KeybindState m_AimbotSecondaryKey{};
	KeybindState m_TriggerPrimaryKey{};
	KeybindState m_TriggerSecondaryKey{};
	KeybindState m_FlickPrimaryKey{};

	bool m_AimbotHotkeyWasActive = false;

	uint64_t m_LockedTargetPawn = 0;
	std::chrono::steady_clock::time_point m_TargetSwitchReadyAt{};
	bool m_TargetSwitchDelayActive = false;

	std::chrono::steady_clock::time_point m_LastTriggerClick{};
	std::chrono::steady_clock::time_point m_TriggerCandidateSince{};
	uint64_t m_LastTriggerPawn = 0;
	bool m_TriggerHotkeyWasActive = false;
	std::chrono::steady_clock::time_point m_TriggerHotkeyDownSince{};
	bool m_TriggerShotSinceHotkeyDown = false;
	bool m_TriggerMouseHeld = false;
	std::chrono::steady_clock::time_point m_TriggerMouseReleaseAt{};
	bool m_FlickHotkeyWasActive = false;
	bool m_FlickShotFiredThisHold = false;
	bool m_FlickForceFireThisHold = false;
	std::uint64_t m_FlickLockedTargetPawn = 0;
	std::chrono::steady_clock::time_point m_FlickStartTime{};
	std::chrono::steady_clock::time_point m_FlickNextCycleAt{};
	std::chrono::steady_clock::time_point m_LastFlickScanAt{};
	bool m_FlickRevolverFollowMode = false;
	bool m_FlickOwnsMouseHold = false;

	std::chrono::steady_clock::time_point m_LastTargetScanAt{};
	Vector2 m_RecoilPos{};
	bool m_HasRecoil = false;

	std::atomic<float> m_CurrentFovRadiusPx{ 0.0f };
	std::atomic<float> m_CurrentTriggerHitboxRadiusPx{ 0.0f };
	std::atomic<float> m_CurrentTriggerHeadRadiusPx{ 0.0f };
	std::atomic<std::uint64_t> m_CurrentTriggerBoneMask{ 0ull };
	std::atomic<int> m_CurrentTriggerDetectMode{ Structs::TriggerDetect_BoneHitbox };
	std::atomic<bool> m_AimbotHotkeyActiveVisual{ false };
	std::atomic<bool> m_AimbotHasTargetVisual{ false };
	std::atomic<bool> m_TriggerHotkeyActiveVisual{ false };
	std::atomic<bool> m_TriggerHasTargetVisual{ false };
	std::atomic<bool> m_FlickHotkeyActiveVisual{ false };
	std::atomic<bool> m_FlickHasTargetVisual{ false };
	std::atomic<std::uint64_t> m_FlickAutowallTargetPawnVisual{ 0ull };
};

inline Aimbot& aim = Aimbot::Get();
