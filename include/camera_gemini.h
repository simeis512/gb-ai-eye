#pragma once

#include <Arduino.h>

bool initializeSenseCamera();
bool ensureWiFiConnected();
bool captureAndDescribe(String& caption, String& errorMessage);
bool configurationIsReady();
void shutdownCaptureHardware();
const char* selectedAiProviderName();
const char* selectedAiModel();
