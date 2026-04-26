#include <SDK.hpp>

#include <Aimbot/Aimbot.hpp>
#include <ESP/ESP.hpp>

class Features
{
public:
	static constexpr auto kAimbotInterval = chrono::milliseconds(2);
	static constexpr auto kTriggerInterval = chrono::milliseconds(2);
	static constexpr auto kFlickInterval = chrono::milliseconds(2);

	void InitAimbotThread()
	{
		m_AimbotThread = thread([this]()
		{
			while (Globals::Running)
			{
				this_thread::sleep_for(kAimbotInterval);

				if (!Globals::Running)
					break;

				aim.UpdateAimbot();
			}
		});
	}

	void InitTriggerbotThread()
	{
		m_TriggerThread = thread([this]()
		{
			while (Globals::Running)
			{
				this_thread::sleep_for(kTriggerInterval);

				if (!Globals::Running)
					break;

				aim.UpdateTriggerbot();
			}
		});
	}

	void InitFlickThread()
	{
		m_FlickThread = thread([this]()
		{
			while (Globals::Running)
			{
				this_thread::sleep_for(kFlickInterval);

				if (!Globals::Running)
					break;

				aim.UpdateFlickbot();
			}
		});
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
		InitFlickThread();

		return true;
	}

	void Shutdown()
	{
		if (m_AimbotThread.joinable())
			m_AimbotThread.join();
		if (m_TriggerThread.joinable())
			m_TriggerThread.join();
		if (m_FlickThread.joinable())
			m_FlickThread.join();
	}

private:
	thread m_AimbotThread{};
	thread m_TriggerThread{};
	thread m_FlickThread{};
};

inline Features& features = Features::Get();
