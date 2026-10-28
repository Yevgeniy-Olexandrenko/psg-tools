#pragma once

#include <atomic>

// Draws a fake playlist through Screen to check how the console UI behaves
// in a particular terminal: history scrolling, window resize, line wrapping.
// Plays no audio.
void RunUITest(std::atomic<bool>& termination);
