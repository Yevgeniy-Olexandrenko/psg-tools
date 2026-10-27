#include <algorithm>

#include "KeyboardInput.h"

namespace
{
    // Some terminals do not report key releases, and the console makes up the
    // key-up right after each key-down, so a key is considered held a bit
    // longer, which covers the interval between its auto-repeated key-downs
    const auto k_tapHoldTime = std::chrono::milliseconds(150);
}

KeyboardInput::KeyboardInput()
    : m_keys{}
    , m_down{}
{
}

void KeyboardInput::Update()
{
    for (KeyState& key : m_keys)
    {
        key.pressed = false;
        key.released = false;
    }

    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    auto now = Clock::now();
    bool downNow[256]{};

    INPUT_RECORD records[64];
    DWORD count = 0;
    while (GetNumberOfConsoleInputEvents(handle, &count) && count > 0)
    {
        DWORD read = 0;
        if (!ReadConsoleInputW(handle, records, std::min<DWORD>(count, 64), &read) || !read) break;

        for (DWORD i = 0; i < read; ++i)
        {
            if (records[i].EventType != KEY_EVENT) continue;
            const KEY_EVENT_RECORD& event = records[i].Event.KeyEvent;
            int vk = (event.wVirtualKeyCode & 0xFF);

            if (event.bKeyDown)
            {
                if (!m_keys[vk].held) m_keys[vk].pressed = true;
                m_keys[vk].held = true;
                m_down[vk] = true;
                downNow[vk] = true;
            }
            else if (m_down[vk])
            {
                m_down[vk] = false;
                if (downNow[vk])
                {
                    m_tapTime[vk] = now;
                }
                else
                {
                    m_keys[vk].held = false;
                    m_keys[vk].released = true;
                }
            }
        }
    }

    for (int vk = 0; vk < 256; ++vk)
    {
        if (m_keys[vk].held && !m_down[vk] && now - m_tapTime[vk] >= k_tapHoldTime)
        {
            m_keys[vk].held = false;
            m_keys[vk].released = true;
        }
    }
}

const KeyState& KeyboardInput::GetKeyState(int key) const
{
    static KeyState s_dummy;
    return (key >= 0 && key <= 255 ? m_keys[key] : s_dummy);
}
