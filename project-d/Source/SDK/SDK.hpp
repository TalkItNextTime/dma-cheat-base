#pragma once
#include <Overlay/Overlay.hpp>
#include <cstdint>
#include <mutex>
#include <string>

#include "Offsets.hpp"

class SDK
{
public:
	struct CoreCache
	{
		uint64_t LocalController = 0;
		uint64_t LocalPawn = 0;
		uint64_t EntityList = 0;
		Matrix ViewMatrix{};
		bool IsValid = false;
	};

	bool Init();
	void InitUpdateSdk();
	bool LoadOffsets();

	bool RefreshCoreCache();
	CoreCache GetCoreCache() const;
	std::string GetCurrentMapName() const;

	uint64_t ResolveEntityFromHandle(uint32_t handle) const;
	uint64_t ResolveEntityFromHandle(uint32_t handle, uint64_t entityList) const;
	uint64_t ResolvePawnFromController(uint64_t controller) const;
	bool ReadBasicEntityState(uint64_t entity, int& health, int& team, int& lifeState) const;

	static SDK& Get()
	{
		static SDK instance;
		return instance;
	}

	bool WorldToScreen(const Vector3& WorldPos, Vector2& ScreenPos, const Matrix& Matrix = Globals::ViewMatrix);

private:
	mutable std::mutex m_CoreMutex;
	CoreCache m_CoreCache;
	bool m_LoggedMissingOffsets = false;
};

inline SDK& sdk = SDK::Get();
