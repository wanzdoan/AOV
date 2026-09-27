#pragma once
#include <ctime>

// Initialize once after app specialization, before any feature/render thread.
// Subsequent calls belong to the render thread only.
namespace ConfigStore {
void initialize(const char* appDataDirectory);
bool enabled();
bool setEnabled(bool enabled);
void saveIfChanged(); // Called after a completed UI interaction, not during drag.
const char* status();
bool hasError();
std::time_t lastSaved();
}
