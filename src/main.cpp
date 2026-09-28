#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <cstring>

#include "app_config.h"
#include "camera_gemini.h"
#include "gameboy_link.h"
#include "gameboy_text.h"

namespace {

GameBoyLink gameBoyLink(GB_CLOCK_PIN, GB_FROM_GAME_BOY_PIN,
                        GB_TO_GAME_BOY_PIN);
std::atomic_bool captureRunning{false};
std::atomic_uint32_t captureSessionCounter{0};
uint32_t gameBoyRxBytes = 0;
uint32_t unexpectedGameBoyBytes = 0;
uint8_t lastGameBoyByte = 0;
bool hasGameBoyByte = false;

enum class StatusLedMode : uint8_t {
  kStarting,
  kReady,
  kBusy,
  kResultReady,
  kError,
};

std::atomic<StatusLedMode> statusLedMode{StatusLedMode::kStarting};

void setStatusLed(bool on) {
  // XIAO ESP32S3's onboard user LED on GPIO21 is active-low.
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

void updateStatusLed() {
  const uint32_t now = millis();
  bool on = false;
  switch (statusLedMode.load()) {
    case StatusLedMode::kStarting:
      on = (now % 200) < 100;
      break;
    case StatusLedMode::kReady:
      on = (now % 2000) < 80;
      break;
    case StatusLedMode::kBusy:
      if (WiFi.status() == WL_CONNECTED) {
        // Two short flashes: Wi-Fi is up and AI processing is in progress.
        const uint32_t phase = now % 1200;
        on = phase < 100 || (phase >= 200 && phase < 300);
      } else {
        // Even blinking: camera start-up or Wi-Fi association is in progress.
        on = (now % 400) < 200;
      }
      break;
    case StatusLedMode::kResultReady:
      on = true;
      break;
    case StatusLedMode::kError: {
      const uint32_t phase = now % 2000;
      on = phase < 100 || (phase >= 200 && phase < 300) ||
           (phase >= 400 && phase < 500);
      break;
    }
  }
  setStatusLed(on);
}

void captureTask(void*) {
  const uint32_t startedAt = millis();
  String caption;
  String errorMessage;
  uint8_t encoded[GB_MAX_CAPTION_BYTES]{};

  const bool captured = captureAndDescribe(caption, errorMessage);
  shutdownCaptureHardware();
  if (captured) {
    size_t unsupported = 0;
    size_t length = encodeGameBoyJapanese(
        caption, encoded, sizeof(encoded), GB_MAX_VISIBLE_GLYPHS,
        GB_FIRST_LINE_GLYPHS, &unsupported);
    if (unsupported != 0) {
      Serial.printf("Caption contained %u unsupported code points; using fallback\n",
                    static_cast<unsigned>(unsupported));
      caption = "なにかが おおきく うつっておる しゃしんじゃ！";
      length = encodeGameBoyJapanese(
          caption, encoded, sizeof(encoded), GB_MAX_VISIBLE_GLYPHS,
          GB_FIRST_LINE_GLYPHS, nullptr);
    }
    if (length > 0) {
      Serial.printf("%s: ", selectedAiProviderName());
      Serial.println(caption);
      Serial.printf("Game Boy caption: %u bytes\n",
                    static_cast<unsigned>(length));
      Serial.print("Game Boy bytes:");
      for (size_t i = 0; i < length; ++i) {
        Serial.printf(" %02X", encoded[i]);
      }
      Serial.println();
      const uint32_t elapsed = millis() - startedAt;
      if (elapsed < GB_MIN_BUSY_TIME_MS) {
        delay(GB_MIN_BUSY_TIME_MS - elapsed);
      }
      gameBoyLink.publishCaption(encoded, length, GB_ACK);
      statusLedMode.store(StatusLedMode::kResultReady);
    } else {
      errorMessage = "Caption encoding returned no bytes";
      gameBoyLink.cancelStream(GB_NAK);
      statusLedMode.store(StatusLedMode::kError);
    }
  } else {
    Serial.print("Capture failed: ");
    Serial.println(errorMessage);
    gameBoyLink.cancelStream(GB_NAK);
    statusLedMode.store(StatusLedMode::kError);
  }

  captureRunning.store(false);
  vTaskDelete(nullptr);
}

void startCapture() {
  bool expected = false;
  if (!captureRunning.compare_exchange_strong(expected, true)) {
    gameBoyLink.setStatus(GB_BUSY);
    return;
  }

  // A new CAPTURE byte always starts a fresh protocol session. This discards
  // any unsent frame from an interrupted previous request before BUSY is sent.
  gameBoyLink.cancelStream(GB_BUSY);
  const uint32_t session = captureSessionCounter.fetch_add(1) + 1;
  Serial.printf("Capture session %lu started; previous stream reset\n",
                static_cast<unsigned long>(session));
  statusLedMode.store(StatusLedMode::kBusy);
  BaseType_t result = xTaskCreatePinnedToCore(captureTask, "camera-ai", 12288,
                                              nullptr, 1, nullptr, 1);
  if (result != pdPASS) {
    captureRunning.store(false);
    gameBoyLink.cancelStream(GB_NAK);
    statusLedMode.store(StatusLedMode::kError);
    Serial.println("Could not start camera task");
  }
}

bool gameBoyTextNormalizationIsValid() {
  const String composed = u8"\u30DA\u304C";  // ペが
  const String decomposed =
      u8"\u30D8\u309A\u304B\u3099";  // ヘ + 半濁点、か + 濁点
  uint8_t composedBytes[8]{};
  uint8_t decomposedBytes[8]{};
  size_t composedUnsupported = 0;
  size_t decomposedUnsupported = 0;
  const size_t composedLength = encodeGameBoyJapanese(
      composed, composedBytes, sizeof(composedBytes), 2, 1,
      &composedUnsupported);
  const size_t decomposedLength = encodeGameBoyJapanese(
      decomposed, decomposedBytes, sizeof(decomposedBytes), 2, 1,
      &decomposedUnsupported);
  return countGameBoyJapaneseGlyphs(composed) == 2 &&
         countGameBoyJapaneseGlyphs(decomposed) == 2 &&
         composedUnsupported == 0 && decomposedUnsupported == 0 &&
         composedLength == 4 && decomposedLength == composedLength &&
         memcmp(composedBytes, decomposedBytes, composedLength) == 0 &&
         composedBytes[0] == 0xE4 && composedBytes[1] == 0xCD &&
         composedBytes[2] == 0x4F && composedBytes[3] == 0x26;
}

}  // namespace

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setStatusLed(false);
  Serial.begin(115200);
  delay(1000);
  Serial.println("GB AI Eye / XIAO ESP32S3 Sense");
  Serial.printf("AI provider: %s\n", selectedAiProviderName());
  Serial.printf("AI model: %s\n", selectedAiModel());
  Serial.printf("Game Boy text normalization: %s\n",
                gameBoyTextNormalizationIsValid() ? "pass" : "FAIL");

  const uint8_t initialStatus = configurationIsReady() ? GB_ACK : GB_NAK;
  gameBoyLink.begin(initialStatus, GB_CAPTURE_COMMAND, GB_BUSY);
  Serial.printf("Game Boy link pins: SCK=%d SOUT->XIAO=%d XIAO->SIN=%d\n",
                GB_CLOCK_PIN, GB_FROM_GAME_BOY_PIN, GB_TO_GAME_BOY_PIN);

  if (!configurationIsReady()) {
    statusLedMode.store(StatusLedMode::kError);
    Serial.println("Edit include/secrets.h before using capture requests");
    return;
  }
  statusLedMode.store(StatusLedMode::kReady);
  Serial.println("Camera and Wi-Fi stay off until a capture request");
  Serial.println("Send 'c' over USB serial to test one capture");
  Serial.println(
      "LED: ready=pulse Wi-Fi wait=blink connected=double result=on "
      "error=triple");
}

void loop() {
  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    if (command == 'c' || command == 'C') {
      Serial.println("USB serial requested a capture");
      if (!configurationIsReady()) {
        gameBoyLink.cancelStream(GB_NAK);
        statusLedMode.store(StatusLedMode::kError);
      } else {
        startCapture();
      }
    }
  }

  uint8_t received = 0;
  while (gameBoyLink.popReceived(received)) {
    ++gameBoyRxBytes;
    lastGameBoyByte = received;
    hasGameBoyByte = true;
    if (received == GB_CAPTURE_COMMAND) {
      Serial.println("Game Boy requested a capture");
      if (!configurationIsReady()) {
        gameBoyLink.cancelStream(GB_NAK);
        statusLedMode.store(StatusLedMode::kError);
      } else {
        startCapture();
      }
    } else {
      ++unexpectedGameBoyBytes;
      // Log the first few values, then powers of two, so a failed polling loop
      // remains diagnosable without flooding the USB serial connection.
      if (unexpectedGameBoyBytes <= 8 ||
          (unexpectedGameBoyBytes & (unexpectedGameBoyBytes - 1)) == 0) {
        Serial.printf("Unexpected Game Boy byte #%lu: %02X\n",
                      static_cast<unsigned long>(unexpectedGameBoyBytes),
                      received);
      }
    }
  }

  static uint32_t previousReport = 0;
  if (millis() - previousReport >= 5000) {
    previousReport = millis();
    const auto stats = gameBoyLink.stats();
    Serial.printf("Link transfers=%lu overflows=%lu resyncs=%lu edgeFaults=%lu "
                  "rx=%lu last=%02X "
                  "rxValid=%s unexpected=%lu capture=%s stream=%s chip=%.1fC\n",
                  static_cast<unsigned long>(stats.transfers),
                  static_cast<unsigned long>(stats.queueOverflows),
                  static_cast<unsigned long>(stats.resyncs),
                  static_cast<unsigned long>(stats.edgeFaults),
                  static_cast<unsigned long>(gameBoyRxBytes),
                  lastGameBoyByte,
                  hasGameBoyByte ? "yes" : "no",
                  static_cast<unsigned long>(unexpectedGameBoyBytes),
                  captureRunning.load() ? "busy" : "idle",
                  gameBoyLink.isStreaming() ? "active" : "idle",
                  temperatureRead());
  }
  if (statusLedMode.load() == StatusLedMode::kResultReady &&
      !gameBoyLink.isStreaming()) {
    statusLedMode.store(StatusLedMode::kReady);
  }
  updateStatusLed();
  delay(1);
}
