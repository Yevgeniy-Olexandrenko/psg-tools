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

private:
	using Clock = std::chrono::steady_clock;

	KeyState m_keys[256];
	bool m_down[256];                 // the key-down came, the key-up did not yet
	Clock::time_point m_tapTime[256]; // the key-up came together with its key-down
};
