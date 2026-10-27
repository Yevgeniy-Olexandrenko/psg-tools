#pragma once

#include <atomic>

// Draws a fake playlist through LiveRegion to check how the console UI behaves
// in a particular terminal: scrollback history, window resize, line wrapping.
// Plays no audio.
void RunUITest(std::atomic<bool>& termination);
