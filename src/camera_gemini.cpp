#include "camera_gemini.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_camera.h>
#include <mbedtls/base64.h>
#include <time.h>

#include "app_config.h"
#include "gameboy_text.h"
#include "google_trust_anchors.h"
#include "openai_trust_anchors.h"

namespace {

bool cameraReady = false;
bool clockReady = false;

constexpr char kPrompt[] =
    "この回答は、初代ポケモン赤を動かすゲームボーイ実機上で、"
    "オーキド博士の台詞としてそのまま表示されます。その前提で、"
    "写真全体を観察し、中心の対象を一つ選んでください。実物、生き物、箱、"
    "商品パッケージ、ポスター、画面を区別し、印刷された人物や乗り物を実物と"
    "取り違えないでください。小型カメラによる色かぶり、暗さ、低解像度、"
    "ピントのずれを考慮し、不明瞭な色や文字を断定の根拠にしないでください。"
    "ただし撮影品質だけを講評せず、確認できる輪郭、図柄、文字から対象を"
    "できるだけ具体的に説明してください。文字と図柄から固有名を確実に読めた場合は、"
    "色や一般名より固有名を優先してください。読めない固有名を推測で作らず、"
    "その場合は見える色、形、数、動き、様子を具体的に述べてください。"
    "ポケモンスナップのオーキド博士が写真を講評するような親しみのある一文を、"
    "必ず15文字以上36文字以内で返してください。感嘆や単語だけは禁止です。"
    "使える文字は、ひらがな、カタカナ、0から9、空白、！、？、。、・、…、ー"
    "だけです。漢字、英字、絵文字、かぎかっこ、読点、改行は使わないでください。"
    "濁点と半濁点は前のカナと結合済みの一文字で書き、結合文字は使わないでください。"
    "固有名は一般的な読みをカタカナで書き、カタカナをひらがなへ変えないでください。"
    "回答前に全ての文字がこの文字一覧に含まれることを確認してください。"
    "説明や前置きを付けず、ゲーム画面へ表示する一文だけを返してください。";

constexpr char kRetryPrompt[] =
    "前の回答は、文字数または使用文字の条件を満たしていません。"
    "写真の内容は保ったまま、15文字以上36文字以内にし、指定された文字だけで"
    "一文を書き直してください。";

bool captionHasValidLength(const String& caption) {
  const size_t glyphs = countGameBoyJapaneseGlyphs(caption);
  const size_t encodedBytes = measureGameBoyJapaneseBytes(
      caption, GB_MAX_VISIBLE_GLYPHS, GB_FIRST_LINE_GLYPHS);
  return glyphs >= AI_MIN_CAPTION_GLYPHS &&
         glyphs <= GB_MAX_VISIBLE_GLYPHS &&
         encodedBytes <= GB_MAX_CAPTION_BYTES;
}

bool isRetryableHttpStatus(int status) {
  return status == 408 || status == 429 || status == 500 || status == 502 ||
         status == 503 || status == 504;
}

bool hasPermanentQuotaError(const String& response) {
  JsonDocument document;
  if (deserializeJson(document, response)) return false;
  const char* code = document["error"]["code"] | "";
  return strcmp(code, "credit_balance_exhausted") == 0 ||
         strcmp(code, "insufficient_quota") == 0 ||
         strcmp(code, "billing_hard_limit_reached") == 0;
}

void waitBeforeAiRetry(uint8_t completedAttempt) {
  const uint32_t delayMs =
      AI_RETRY_BASE_DELAY_MS * static_cast<uint32_t>(completedAttempt + 1);
  Serial.printf("Retrying %s in %lu ms\n", selectedAiProviderName(),
                static_cast<unsigned long>(delayMs));
  delay(delayMs);
}

bool isPlaceholder(const char* value) {
  return value == nullptr || value[0] == '\0' ||
         strcmp(value, "CHANGE_ME") == 0;
}

void trimModelText(String& result) {
  result.trim();
  while (result.startsWith("\"") && result.endsWith("\"") &&
         result.length() >= 2) {
    result = result.substring(1, result.length() - 1);
    result.trim();
  }
}

String extractGeminiText(const String& response, String& errorMessage) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, response);
  if (error) {
    errorMessage = String("Gemini JSON parse failed: ") + error.c_str();
    return {};
  }
  if (document["error"]["message"].is<const char*>()) {
    errorMessage = document["error"]["message"].as<String>();
    return {};
  }

  String result;
  JsonArray parts = document["candidates"][0]["content"]["parts"].as<JsonArray>();
  for (JsonObject part : parts) {
    if (part["thought"].as<bool>()) continue;
    if (part["text"].is<const char*>()) result = part["text"].as<String>();
  }
  trimModelText(result);
  if (result.isEmpty()) errorMessage = "Gemini returned no text";
  return result;
}

String extractOpenAiText(const String& response, String& errorMessage) {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, response);
  if (error) {
    errorMessage = String("OpenAI JSON parse failed: ") + error.c_str();
    return {};
  }
  if (document["error"]["message"].is<const char*>()) {
    errorMessage = document["error"]["message"].as<String>();
    return {};
  }

  String result;
  for (JsonObject output : document["output"].as<JsonArray>()) {
    if (strcmp(output["type"] | "", "message") != 0) continue;
    for (JsonObject content : output["content"].as<JsonArray>()) {
      if (strcmp(content["type"] | "", "output_text") == 0 &&
          content["text"].is<const char*>()) {
        result = content["text"].as<String>();
      }
    }
  }
  trimModelText(result);
  if (result.isEmpty()) {
    if (document["incomplete_details"]["reason"].is<const char*>()) {
      errorMessage = String("OpenAI response incomplete: ") +
                     document["incomplete_details"]["reason"].as<String>();
    } else {
      errorMessage = "OpenAI returned no text";
    }
  }
  return result;
}

String extractModelText(const String& response, String& errorMessage) {
  return AI_PROVIDER == AiProvider::kOpenAI
             ? extractOpenAiText(response, errorMessage)
             : extractGeminiText(response, errorMessage);
}

bool buildRequestBody(const unsigned char* encoded, size_t encodedLength,
                      bool isRetry, String& body) {
  if (!body.reserve(encodedLength + sizeof(kPrompt) + sizeof(kRetryPrompt) +
                    768)) {
    return false;
  }

  if (AI_PROVIDER == AiProvider::kOpenAI) {
    body = "{\"model\":\"";
    body += OPENAI_MODEL_ID;
    body +=
        "\",\"store\":false,\"reasoning\":{\"effort\":\"none\"},"
        "\"max_output_tokens\":128,\"input\":[{\"role\":\"user\","
        "\"content\":[{\"type\":\"input_text\",\"text\":\"";
    body += kPrompt;
    if (isRetry) body += kRetryPrompt;
    body +=
        "\"},{\"type\":\"input_image\",\"image_url\":"
        "\"data:image/jpeg;base64,";
    body.concat(reinterpret_cast<const char*>(encoded), encodedLength);
    body += "\",\"detail\":\"high\"}]}]}";
    return true;
  }

  body =
      "{\"contents\":[{\"parts\":[{\"inline_data\":{\"mime_type\":"
      "\"image/jpeg\",\"data\":\"";
  body.concat(reinterpret_cast<const char*>(encoded), encodedLength);
  body += "\"}},{\"text\":\"";
  body += kPrompt;
  if (isRetry) body += kRetryPrompt;
  body +=
      "\"}]}],\"generationConfig\":{\"thinkingConfig\":{\"thinkingLevel\":"
      "\"low\"},\"maxOutputTokens\":128,\"responseMimeType\":\"text/plain\"}}";
  return true;
}

}  // namespace

const char* selectedAiProviderName() {
  return AI_PROVIDER == AiProvider::kOpenAI ? "OpenAI" : "Gemini";
}

const char* selectedAiModel() {
  return AI_PROVIDER == AiProvider::kOpenAI ? OPENAI_MODEL_ID : GEMINI_MODEL_ID;
}

bool configurationIsReady() {
  const char* apiKey = AI_PROVIDER == AiProvider::kOpenAI ? OPENAI_API_KEY
                                                          : GEMINI_API_KEY;
  return !isPlaceholder(WIFI_SSID) && !isPlaceholder(WIFI_PASSWORD) &&
         !isPlaceholder(apiKey);
}

bool initializeSenseCamera() {
  if (cameraReady) return true;

  camera_config_t config{};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 15;
  config.pin_d1 = 17;
  config.pin_d2 = 18;
  config.pin_d3 = 16;
  config.pin_d4 = 14;
  config.pin_d5 = 12;
  config.pin_d6 = 11;
  config.pin_d7 = 48;
  config.pin_xclk = 10;
  config.pin_pclk = 13;
  config.pin_vsync = 38;
  config.pin_href = 47;
  config.pin_sccb_sda = 40;
  config.pin_sccb_scl = 39;
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // Product names and small package artwork were unreadable at QVGA. SVGA
  // provides 6.25x as many source pixels while fitting the XIAO's PSRAM.
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = psramFound() ? 2 : 1;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    Serial.printf("Camera init failed: 0x%X\n", error);
    return false;
  }
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr && sensor->id.PID == OV3660_PID) {
    sensor->set_vflip(sensor, 1);
    sensor->set_brightness(sensor, 1);
    sensor->set_saturation(sensor, -2);
  }
  cameraReady = true;

  camera_fb_t* warmup = esp_camera_fb_get();
  if (warmup != nullptr) esp_camera_fb_return(warmup);
  return true;
}

bool ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (!configurationIsReady()) return false;

  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true, false);
    delay(250);
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection timed out");
    return false;
  }
  Serial.print("Wi-Fi connected: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Wi-Fi channel=%d RSSI=%d dBm\n", WiFi.channel(), WiFi.RSSI());
  return true;
}

bool ensureClockIsReady() {
  if (clockReady) return true;
  constexpr time_t kReasonableEpoch = 1704067200;
  if (time(nullptr) >= kReasonableEpoch) {
    clockReady = true;
    return true;
  }

  configTime(0, 0, "time.google.com", "pool.ntp.org");
  const uint32_t started = millis();
  while (time(nullptr) < kReasonableEpoch &&
         millis() - started < CLOCK_SYNC_TIMEOUT_MS) {
    delay(100);
  }
  clockReady = time(nullptr) >= kReasonableEpoch;
  if (!clockReady) Serial.println("Network clock synchronization timed out");
  return clockReady;
}

bool captureAndDescribe(String& caption, String& errorMessage) {
  caption.clear();
  errorMessage.clear();
  if (!configurationIsReady()) {
    errorMessage = "Edit include/secrets.h first";
    return false;
  }
  if (!initializeSenseCamera()) {
    errorMessage = "Camera is unavailable";
    return false;
  }
  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr || frame->format != PIXFORMAT_JPEG) {
    if (frame != nullptr) esp_camera_fb_return(frame);
    errorMessage = "Camera capture failed";
    return false;
  }
  Serial.printf("Camera frame: %ux%u JPEG=%u bytes\n",
                static_cast<unsigned>(frame->width),
                static_cast<unsigned>(frame->height),
                static_cast<unsigned>(frame->len));

  const size_t encodedCapacity = 4 * ((frame->len + 2) / 3) + 1;
  auto* encoded = static_cast<unsigned char*>(ps_malloc(encodedCapacity));
  if (encoded == nullptr) {
    encoded = static_cast<unsigned char*>(malloc(encodedCapacity));
  }
  if (encoded == nullptr) {
    esp_camera_fb_return(frame);
    errorMessage = "No memory for JPEG base64";
    return false;
  }

  size_t encodedLength = 0;
  const int base64Result = mbedtls_base64_encode(
      encoded, encodedCapacity, &encodedLength, frame->buf, frame->len);
  esp_camera_fb_return(frame);
  if (base64Result != 0) {
    free(encoded);
    errorMessage = String("JPEG base64 failed: ") + base64Result;
    return false;
  }
  encoded[encodedLength] = '\0';

  String url;
  String openAiTrustAnchors;
  if (AI_PROVIDER == AiProvider::kOpenAI) {
    url = "https://api.openai.com/v1/responses";
    // Cloudflare may serve api.openai.com with either Google Trust Services
    // or Let's Encrypt. Trust both current public root families so a routine
    // certificate rotation does not break the embedded client.
    if (!openAiTrustAnchors.reserve(sizeof(GOOGLE_TRUST_ANCHORS) +
                                    sizeof(OPENAI_TRUST_ANCHORS))) {
      free(encoded);
      errorMessage = "No memory for TLS trust anchors";
      return false;
    }
    openAiTrustAnchors = GOOGLE_TRUST_ANCHORS;
    openAiTrustAnchors += OPENAI_TRUST_ANCHORS;
  } else {
    url = String("https://generativelanguage.googleapis.com/v1beta/models/") +
          GEMINI_MODEL_ID + ":generateContent";
  }

  for (uint8_t attempt = 0; attempt < AI_MAX_REQUEST_ATTEMPTS; ++attempt) {
    const bool canRetry = attempt + 1 < AI_MAX_REQUEST_ATTEMPTS;
    if (!ensureWiFiConnected() || !ensureClockIsReady()) {
      errorMessage = WiFi.status() == WL_CONNECTED
                         ? "Network clock is unavailable"
                         : "Wi-Fi is unavailable";
      Serial.printf("%s attempt %u/%u: %s\n", selectedAiProviderName(),
                    static_cast<unsigned>(attempt + 1),
                    static_cast<unsigned>(AI_MAX_REQUEST_ATTEMPTS),
                    errorMessage.c_str());
      if (canRetry) {
        waitBeforeAiRetry(attempt);
        continue;
      }
      free(encoded);
      return false;
    }

    String body;
    if (!buildRequestBody(encoded, encodedLength, !caption.isEmpty(), body)) {
      free(encoded);
      errorMessage = "No memory for AI request";
      return false;
    }

    WiFiClientSecure secureClient;
    secureClient.setCACert(AI_PROVIDER == AiProvider::kOpenAI
                               ? openAiTrustAnchors.c_str()
                               : GOOGLE_TRUST_ANCHORS);
    HTTPClient http;
    http.setTimeout(AI_HTTP_TIMEOUT_MS);
    if (!http.begin(secureClient, url)) {
      errorMessage = "HTTPS initialization failed";
      Serial.printf("%s attempt %u/%u: %s\n", selectedAiProviderName(),
                    static_cast<unsigned>(attempt + 1),
                    static_cast<unsigned>(AI_MAX_REQUEST_ATTEMPTS),
                    errorMessage.c_str());
      if (canRetry) {
        waitBeforeAiRetry(attempt);
        continue;
      }
      free(encoded);
      return false;
    }
    http.addHeader("Content-Type", "application/json");
    if (AI_PROVIDER == AiProvider::kOpenAI) {
      http.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
    } else {
      http.addHeader("x-goog-api-key", GEMINI_API_KEY);
    }

    const int status = http.POST(body);
    body.clear();
    if (status <= 0) {
      errorMessage = String(selectedAiProviderName()) + " HTTP failed: " +
                     http.errorToString(status);
      http.end();
      Serial.printf("%s attempt %u/%u: %s\n", selectedAiProviderName(),
                    static_cast<unsigned>(attempt + 1),
                    static_cast<unsigned>(AI_MAX_REQUEST_ATTEMPTS),
                    errorMessage.c_str());
      if (canRetry) {
        waitBeforeAiRetry(attempt);
        continue;
      }
      free(encoded);
      return false;
    }

    const String response = http.getString();
    http.end();
    if (status < 200 || status >= 300) {
      String apiError;
      extractModelText(response, apiError);
      errorMessage = String(selectedAiProviderName()) + " HTTP " + status +
                     ": " + apiError;
      Serial.printf("%s attempt %u/%u: HTTP %d: %s\n",
                    selectedAiProviderName(),
                    static_cast<unsigned>(attempt + 1),
                    static_cast<unsigned>(AI_MAX_REQUEST_ATTEMPTS), status,
                    apiError.c_str());
      if (canRetry && isRetryableHttpStatus(status) &&
          !hasPermanentQuotaError(response)) {
        waitBeforeAiRetry(attempt);
        continue;
      }
      free(encoded);
      return false;
    }

    String parseError;
    const String candidate = extractModelText(response, parseError);
    if (candidate.isEmpty()) {
      errorMessage = parseError;
      Serial.printf("%s attempt %u/%u: %s\n", selectedAiProviderName(),
                    static_cast<unsigned>(attempt + 1),
                    static_cast<unsigned>(AI_MAX_REQUEST_ATTEMPTS),
                    errorMessage.c_str());
      if (canRetry) {
        waitBeforeAiRetry(attempt);
        continue;
      }
      free(encoded);
      return false;
    }

    caption = candidate;
    const size_t glyphs = countGameBoyJapaneseGlyphs(caption);
    const size_t unsupported = countUnsupportedGameBoyJapanese(caption);
    const size_t encodedBytes = measureGameBoyJapaneseBytes(
        caption, GB_MAX_VISIBLE_GLYPHS, GB_FIRST_LINE_GLYPHS);
    Serial.printf("%s attempt %u/%u: %u glyphs, %u bytes, %u unsupported\n",
                  selectedAiProviderName(),
                  static_cast<unsigned>(attempt + 1),
                  static_cast<unsigned>(AI_MAX_REQUEST_ATTEMPTS),
                  static_cast<unsigned>(glyphs),
                  static_cast<unsigned>(encodedBytes),
                  static_cast<unsigned>(unsupported));
    if (captionHasValidLength(caption) && unsupported == 0) {
      free(encoded);
      errorMessage.clear();
      return true;
    }
    Serial.println("Caption failed Game Boy text validation; requesting a rewrite");
    if (canRetry) waitBeforeAiRetry(attempt);
  }

  free(encoded);
  if (caption == "しろ" || caption == "まっしろ" || caption == "しろい" ||
      caption == "まっしろ！" || caption == "しろ！") {
    caption = "まっしろな けしきが いっぱいに うつっておるぞ！";
  } else {
    caption = "なにかが おおきく うつっておる しゃしんじゃ！";
  }
  errorMessage.clear();
  Serial.printf("%s did not return display-safe text; using fallback\n",
                selectedAiProviderName());
  return true;
}

void shutdownCaptureHardware() {
  if (cameraReady) {
    esp_camera_deinit();
    cameraReady = false;
  }
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
  }
}
