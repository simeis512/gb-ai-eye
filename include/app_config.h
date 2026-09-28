#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

#define GEMINI_MODEL_ID "gemini-3.8-flash"
#define OPENAI_MODEL_ID "gpt-5.6-sol"

// Select the image-description backend here. Keep credentials for both
// providers in secrets.h, then change only this line when switching.
enum class AiProvider : uint8_t {
  kGemini,
  kOpenAI,
};
constexpr AiProvider AI_PROVIDER = AiProvider::kOpenAI;

// Older local secrets.h files may predate OpenAI support. This fallback keeps
// those configurations buildable until OPENAI_API_KEY is added locally.
#ifndef OPENAI_API_KEY
#define OPENAI_API_KEY "CHANGE_ME"
#endif

// Free XIAO pins that do not overlap the Sense camera or onboard SD interface.
// D0/GPIO1 receives Game Boy SCK, D1/GPIO2 receives SOUT, and D3/GPIO4
// drives SIN. Use a 5 V <-> 3.3 V level shifter between the devices.
constexpr int GB_CLOCK_PIN = 1;
constexpr int GB_FROM_GAME_BOY_PIN = 2;
constexpr int GB_TO_GAME_BOY_PIN = 4;

constexpr uint8_t GB_CAPTURE_COMMAND = 0x43;  // ASCII 'C'
constexpr uint8_t GB_ACK = 0x06;
constexpr uint8_t GB_NAK = 0x15;
constexpr uint8_t GB_BUSY = 0x17;

constexpr size_t GB_MAX_CAPTION_BYTES = 63;
constexpr size_t GB_MAX_VISIBLE_GLYPHS = 36;
constexpr size_t GB_FIRST_LINE_GLYPHS = 18;
constexpr size_t AI_MIN_CAPTION_GLYPHS = 15;
static_assert(GB_MAX_CAPTION_BYTES <= 63,
              "The Game Boy frame header has a 6-bit length field");
static_assert(GB_MAX_VISIBLE_GLYPHS <= GB_FIRST_LINE_GLYPHS * 2,
              "The dialogue box has two visible text lines");
constexpr uint8_t AI_MAX_REQUEST_ATTEMPTS = 3;

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t CLOCK_SYNC_TIMEOUT_MS = 15000;
constexpr uint32_t AI_HTTP_TIMEOUT_MS = 25000;
constexpr uint32_t AI_RETRY_BASE_DELAY_MS = 1500;
constexpr uint32_t GB_MIN_BUSY_TIME_MS = 4000;
