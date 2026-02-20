#include <SDK.hpp>

#include <Aimbot/Aimbot.hpp>
#include <ESP/ESP.hpp>

class Features
{
public:
	static constexpr auto kAimbotInterval = chrono::milliseconds(2);
	static constexpr auto kTriggerInterval = chrono::milliseconds(2);

	void InitAimbotThread()
	{
		thread([this]()
		{
			while (Globals::Running)
			{
				this_thread::sleep_for(kAimbotInterval);

				aim.UpdateAimbot();
			}
		}).detach();
	}

	void InitTriggerbotThread()
	{
		thread([this]()
		{
			while (Globals::Running)
			{
				this_thread::sleep_for(kTriggerInterval);

				aim.UpdateTriggerbot();
			}
		}).detach();
	}

	static Features& Get()
	{
		static Features instance;
		return instance;
	}

	bool Init()
	{
		InitAimbotThread();
		InitTriggerbotThread();

		return true;
	}
};

inline Features& features = Features::Get();
