#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <chrono>
#include <windows.h>

struct KeyState
{
	bool pressed;
	bool released;
	bool held;
};

// Keyboard state built from the console input events: unlike polling the
// keyboard, it works in any console host (conhost, Windows Terminal) and
// only sees the keys pressed while the console window has the focus.
class KeyboardInput
{
public:
	KeyboardInput();

	void Update();
	const KeyState& GetKeyState(int key) const;
	int GetWheel() const { return m_wheel; } // notches since the last Update(), up is positive

private:
	using Clock = std::chrono::steady_clock;

	void Release(int vk);

	KeyState m_keys[256];
	bool m_down[256];                  // the key-down came, the key-up did not yet
	Clock::time_point m_downTime[256]; // the last key-down, auto-repeats included
	int m_wheel;
};
