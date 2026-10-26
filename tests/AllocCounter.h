#pragma once

// Counts global operator new calls on the calling thread between begin() and end().
// Used to prove the audio path does not touch the heap.
namespace alloc_counter {
void begin();
long end();
}  // namespace alloc_counter
