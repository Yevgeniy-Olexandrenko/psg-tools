#include <algorithm>

#include "KeyboardInput.h"

namespace
{
    // Some terminals do not report key releases, and the console makes up the
    // key-up right after each key-down, so a key is considered held a bit
    // longer, which covers the interval between its auto-repeated key-downs
    const auto k_tapHoldTime = std::chrono::milliseconds(150);

    // A held key keeps sending auto-repeated key-downs, the first of them comes
    // after at most a second. Without them the key-up got lost, which happens
    // when another window takes the focus while the key is held.
    const auto k_repeatTimeout = std::chrono::milliseconds(1200);
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
            if (records[i].EventType == FOCUS_EVENT && !records[i].Event.FocusEvent.bSetFocus)
            {
                // the key-ups go to the window that took the focus
                for (int vk = 0; vk < 256; ++vk) Release(vk);
                continue;
            }

            if (records[i].EventType != KEY_EVENT) continue;
            const KEY_EVENT_RECORD& event = records[i].Event.KeyEvent;
            int vk = (event.wVirtualKeyCode & 0xFF);

            if (event.bKeyDown)
            {
                if (!m_keys[vk].held) m_keys[vk].pressed = true;
                m_keys[vk].held = true;
                m_down[vk] = true;
                m_downTime[vk] = now;
                downNow[vk] = true;
            }
            else if (m_down[vk])
            {
                // the key-up made up right after the key-down keeps the key held
                // for a while, a real one releases it at once
                m_down[vk] = false;
                if (!downNow[vk]) Release(vk);
            }
        }
    }

    for (int vk = 0; vk < 256; ++vk)
    {
        if (m_keys[vk].held)
        {
            auto timeout = (m_down[vk] ? k_repeatTimeout : k_tapHoldTime);
            if (now - m_downTime[vk] >= timeout) Release(vk);
        }
    }
}

const KeyState& KeyboardInput::GetKeyState(int key) const
{
    static KeyState s_dummy;
    return (key >= 0 && key <= 255 ? m_keys[key] : s_dummy);
}

void KeyboardInput::Release(int vk)
{
    if (m_keys[vk].held)
    {
        m_keys[vk].held = false;
        m_keys[vk].released = true;
    }
    m_down[vk] = false;
}
