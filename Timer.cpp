#include "Timer.h"

void Timer::reset() noexcept
{
	m_paused = false;
	m_baseTime = std::chrono::system_clock::now();
	m_prevTime = m_baseTime;
	m_frameStartTime = m_baseTime;
	m_deltaTime = (std::chrono::duration<double>)0;
	m_pausedTime = (std::chrono::duration<double>)0;
	m_fps = 0;
	m_frameCount = 0;
}

void Timer::toggle() noexcept
{
	if (m_paused) start();
	else stop();
}

unsigned int Timer::getFps() const noexcept
{
	return m_fps;
}

float Timer::getDeltaTime() const noexcept
{
	return (float)(m_deltaTime.count());
}

bool Timer::isPaused() const noexcept
{
	return m_paused;
}

Timer::Timer()
{
	/*reset timer on init to initialize member fields
	and allow use of timer right away without explicilty
	calling reset*/
	reset();
}

void Timer::tick() noexcept
{
	if (m_paused)
		return;
		
	m_currTime = std::chrono::system_clock::now();
	m_deltaTime = (m_currTime - m_prevTime);

	if ((std::chrono::duration<double>(m_currTime - m_frameStartTime)).count() >= 1.0)
	{
		m_fps = m_frameCount;
		m_frameCount = 0;
		m_frameStartTime = m_currTime;
	}
	else
	{
		m_frameCount++;
	}

	m_prevTime = m_currTime;
}

void Timer::start() noexcept
{
	m_paused = false;
	m_currTime = std::chrono::system_clock::now();
	m_pausedTime += std::chrono::duration<double>(m_currTime - m_pauseStartTime);
	m_prevTime = m_currTime;
	m_frameStartTime = m_currTime;
	m_deltaTime = (std::chrono::duration<double>)0;
}

void Timer::stop() noexcept
{
	m_paused = true;
	m_pauseStartTime = std::chrono::system_clock::now();
}

float Timer::getTotalTime() noexcept
{
	return (float)((std::chrono::duration<double>(std::chrono::system_clock::now() - m_baseTime - m_pausedTime)).count());
}