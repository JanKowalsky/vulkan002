#ifndef TIMER_H
#define TIMER_H

#include <chrono>

class Timer
{
public:
	~Timer() {};
	Timer();

	void reset() noexcept;
	void tick() noexcept;

	void toggle() noexcept;
	void stop() noexcept;
	void start() noexcept;

	unsigned int getFps() const noexcept;
	float getDeltaTime() const noexcept;
	float getTotalTime() noexcept;
	bool isPaused() const noexcept;

private:
	std::chrono::duration<double> m_deltaTime;
	std::chrono::time_point<std::chrono::system_clock> m_currTime;
	std::chrono::time_point<std::chrono::system_clock> m_prevTime;
	std::chrono::time_point<std::chrono::system_clock> m_baseTime;
	std::chrono::duration<double> m_pausedTime;
	std::chrono::time_point<std::chrono::system_clock> m_pauseStartTime;
	std::chrono::time_point<std::chrono::system_clock> m_frameStartTime;

	uint64_t m_fps = 0;
	uint64_t m_frameCount = 0;
	bool m_paused = false;
};

#endif //TIMER_H