#include "gameboy_text.h"

namespace {

struct Glyph {
  uint32_t codePoint;
  uint8_t value;
};

struct MarkedKana {
  uint32_t codePoint;
  uint32_t baseCodePoint;
  uint8_t mark;
};

constexpr Glyph kGlyphs[] = {
    // Japanese Red/Green store most precomposed voiced kana as single bytes.
    {0x304C, 0x26}, {0x304E, 0x27}, {0x3050, 0x28}, {0x3052, 0x29},
    {0x3054, 0x2A}, {0x3056, 0x2B}, {0x3058, 0x2C}, {0x305A, 0x2D},
    {0x305C, 0x2E}, {0x305E, 0x2F}, {0x3060, 0x30}, {0x3062, 0x31},
    {0x3065, 0x32}, {0x3067, 0x33}, {0x3069, 0x34}, {0x3070, 0x3A},
    {0x3073, 0x3B}, {0x3076, 0x3C}, {0x3079, 0x3D}, {0x307C, 0x3E},
    {0x3071, 0x44}, {0x3074, 0x45}, {0x3077, 0x46}, {0x307A, 0x47},
    {0x307D, 0x48},

    {0x30AC, 0x05}, {0x30AE, 0x06}, {0x30B0, 0x07}, {0x30B2, 0x08},
    {0x30B4, 0x09}, {0x30B6, 0x0A}, {0x30B8, 0x0B}, {0x30BA, 0x0C},
    {0x30BC, 0x0D}, {0x30BE, 0x0E}, {0x30C0, 0x0F}, {0x30C2, 0x10},
    {0x30C5, 0x11}, {0x30C7, 0x12}, {0x30C9, 0x13}, {0x30D0, 0x19},
    {0x30D3, 0x1A}, {0x30D6, 0x1B}, {0x30DC, 0x1C}, {0x30D1, 0x40},
    {0x30D4, 0x41}, {0x30D7, 0x42}, {0x30DD, 0x43},

    {0x3041, 0x76}, {0x3042, 0xB1}, {0x3043, 0x6E}, {0x3044, 0xB2},
    {0x3045, 0x6F}, {0x3046, 0xB3}, {0x3047, 0x77}, {0x3048, 0xB4},
    {0x3049, 0x78}, {0x304A, 0xB5}, {0x304B, 0xB6}, {0x304D, 0xB7},
    {0x304F, 0xB8}, {0x3051, 0xB9}, {0x3053, 0xBA}, {0x3055, 0xBB},
    {0x3057, 0xBC}, {0x3059, 0xBD}, {0x305B, 0xBE}, {0x305D, 0xBF},
    {0x305F, 0xC0}, {0x3061, 0xC1}, {0x3063, 0xDF}, {0x3064, 0xC2},
    {0x3066, 0xC3}, {0x3068, 0xC4}, {0x306A, 0xC5}, {0x306B, 0xC6},
    {0x306C, 0xC7}, {0x306D, 0xC8}, {0x306E, 0xC9}, {0x306F, 0xCA},
    {0x3072, 0xCB}, {0x3075, 0xCC}, {0x3078, 0xCD}, {0x307B, 0xCE},
    {0x307E, 0xCF}, {0x307F, 0xD0}, {0x3080, 0xD1}, {0x3081, 0xD2},
    {0x3082, 0xD3}, {0x3083, 0xE0}, {0x3084, 0xD4}, {0x3085, 0xE1},
    {0x3086, 0xD5}, {0x3087, 0xE2}, {0x3088, 0xD6}, {0x3089, 0xD7},
    {0x308A, 0xD8}, {0x308B, 0xD9}, {0x308C, 0xDA}, {0x308D, 0xDB},
    {0x308F, 0xDC}, {0x3092, 0xDD}, {0x3093, 0xDE},

    {0x30A1, 0xE9}, {0x30A2, 0x80}, {0x30A3, 0xB0}, {0x30A4, 0x81},
    {0x30A5, 0xEA}, {0x30A6, 0x82}, {0x30A7, 0xEB}, {0x30A8, 0x83},
    {0x30A9, 0xF4}, {0x30AA, 0x84}, {0x30AB, 0x85}, {0x30AD, 0x86},
    {0x30AF, 0x87}, {0x30B1, 0x88}, {0x30B3, 0x89}, {0x30B5, 0x8A},
    {0x30B7, 0x8B}, {0x30B9, 0x8C}, {0x30BB, 0x8D}, {0x30BD, 0x8E},
    {0x30BF, 0x8F}, {0x30C1, 0x90}, {0x30C3, 0xAC}, {0x30C4, 0x91},
    {0x30C6, 0x92}, {0x30C8, 0x93}, {0x30CA, 0x94}, {0x30CB, 0x95},
    {0x30CC, 0x96}, {0x30CD, 0x97}, {0x30CE, 0x98}, {0x30CF, 0x99},
    {0x30D2, 0x9A}, {0x30D5, 0x9B}, {0x30D8, 0xCD}, {0x30DB, 0x9C},
    {0x30DE, 0x9D}, {0x30DF, 0x9E}, {0x30E0, 0x9F}, {0x30E1, 0xA0},
    {0x30E2, 0xA1}, {0x30E3, 0xAD}, {0x30E4, 0xA2}, {0x30E5, 0xAE},
    {0x30E6, 0xA3}, {0x30E7, 0xAF}, {0x30E8, 0xA4}, {0x30E9, 0xA5},
    // Katakana RI shares the same glyph as hiragana ri in Japanese Gen I.
    // RU/RE/RO/WA then continue at A6/A7/A8/A9 respectively.
    {0x30EA, 0xD8}, {0x30EB, 0xA6}, {0x30EC, 0xA7}, {0x30ED, 0xA8},
    {0x30EF, 0xA9}, {0x30F2, 0xAA}, {0x30F3, 0xAB},
};

constexpr MarkedKana kMarkedKana[] = {
    {0x304C, 0x304B, 0xE5}, {0x304E, 0x304D, 0xE5},
    {0x3050, 0x304F, 0xE5}, {0x3052, 0x3051, 0xE5},
    {0x3054, 0x3053, 0xE5}, {0x3056, 0x3055, 0xE5},
    {0x3058, 0x3057, 0xE5}, {0x305A, 0x3059, 0xE5},
    {0x305C, 0x305B, 0xE5}, {0x305E, 0x305D, 0xE5},
    {0x3060, 0x305F, 0xE5}, {0x3062, 0x3061, 0xE5},
    {0x3065, 0x3064, 0xE5}, {0x3067, 0x3066, 0xE5},
    {0x3069, 0x3068, 0xE5}, {0x3070, 0x306F, 0xE5},
    {0x3073, 0x3072, 0xE5}, {0x3076, 0x3075, 0xE5},
    {0x3079, 0x3078, 0xE5}, {0x307C, 0x307B, 0xE5},
    {0x3071, 0x306F, 0xE4}, {0x3074, 0x3072, 0xE4},
    {0x3077, 0x3075, 0xE4}, {0x307A, 0x3078, 0xE4},
    {0x307D, 0x307B, 0xE4}, {0x3094, 0x3046, 0xE5},

    {0x30AC, 0x30AB, 0xE5}, {0x30AE, 0x30AD, 0xE5},
    {0x30B0, 0x30AF, 0xE5}, {0x30B2, 0x30B1, 0xE5},
    {0x30B4, 0x30B3, 0xE5}, {0x30B6, 0x30B5, 0xE5},
    {0x30B8, 0x30B7, 0xE5}, {0x30BA, 0x30B9, 0xE5},
    {0x30BC, 0x30BB, 0xE5}, {0x30BE, 0x30BD, 0xE5},
    {0x30C0, 0x30BF, 0xE5}, {0x30C2, 0x30C1, 0xE5},
    {0x30C5, 0x30C4, 0xE5}, {0x30C7, 0x30C6, 0xE5},
    {0x30C9, 0x30C8, 0xE5}, {0x30D0, 0x30CF, 0xE5},
    {0x30D3, 0x30D2, 0xE5}, {0x30D6, 0x30D5, 0xE5},
    {0x30D9, 0x30D8, 0xE5}, {0x30DC, 0x30DB, 0xE5},
    {0x30D1, 0x30CF, 0xE4}, {0x30D4, 0x30D2, 0xE4},
    {0x30D7, 0x30D5, 0xE4}, {0x30DA, 0x30D8, 0xE4},
    {0x30DD, 0x30DB, 0xE4}, {0x30F4, 0x30A6, 0xE5},
};

bool lookupGlyph(uint32_t codePoint, uint8_t& value) {
  for (const auto& glyph : kGlyphs) {
    if (glyph.codePoint == codePoint) {
      value = glyph.value;
      return true;
    }
  }
  return false;
}

bool lookupMarkedKana(uint32_t codePoint, uint8_t& base, uint8_t& mark) {
  for (const auto& kana : kMarkedKana) {
    if (kana.codePoint == codePoint) {
      mark = kana.mark;
      return lookupGlyph(kana.baseCodePoint, base);
    }
  }
  return false;
}

uint8_t combiningMarkValue(uint32_t codePoint) {
  switch (codePoint) {
    case 0x3099:  // combining dakuten
    case 0x309B:  // spacing dakuten
      return 0xE5;
    case 0x309A:  // combining handakuten
    case 0x309C:  // spacing handakuten
      return 0xE4;
    default:
      return 0;
  }
}

bool composeMarkedKana(uint32_t baseCodePoint, uint32_t markCodePoint,
                       uint32_t& composedCodePoint) {
  const uint8_t mark = combiningMarkValue(markCodePoint);
  if (mark == 0) return false;
  for (const auto& kana : kMarkedKana) {
    if (kana.baseCodePoint == baseCodePoint && kana.mark == mark) {
      composedCodePoint = kana.codePoint;
      return true;
    }
  }
  return false;
}

uint32_t nextCodePoint(const char*& cursor, const char* end) {
  if (cursor >= end) return 0;
  const uint8_t first = static_cast<uint8_t>(*cursor++);
  if (first < 0x80) return first;
  if ((first & 0xE0) == 0xC0 && cursor < end) {
    return ((first & 0x1F) << 6) |
           (static_cast<uint8_t>(*cursor++) & 0x3F);
  }
  if ((first & 0xF0) == 0xE0 && cursor + 1 < end) {
    const uint32_t value =
        ((first & 0x0F) << 12) |
        ((static_cast<uint8_t>(cursor[0]) & 0x3F) << 6) |
        (static_cast<uint8_t>(cursor[1]) & 0x3F);
    cursor += 2;
    return value;
  }
  if ((first & 0xF8) == 0xF0 && cursor + 2 < end) {
    const uint32_t value =
        ((first & 0x07) << 18) |
        ((static_cast<uint8_t>(cursor[0]) & 0x3F) << 12) |
        ((static_cast<uint8_t>(cursor[1]) & 0x3F) << 6) |
        (static_cast<uint8_t>(cursor[2]) & 0x3F);
    cursor += 3;
    return value;
  }
  return 0xFFFD;
}

uint32_t nextDisplayCodePoint(const char*& cursor, const char* end) {
  const uint32_t codePoint = nextCodePoint(cursor, end);
  if (cursor >= end || combiningMarkValue(codePoint) != 0) return codePoint;

  const char* afterMark = cursor;
  const uint32_t possibleMark = nextCodePoint(afterMark, end);
  uint32_t composed = 0;
  if (composeMarkedKana(codePoint, possibleMark, composed)) {
    cursor = afterMark;
    return composed;
  }
  return codePoint;
}

bool isSupportedCodePoint(uint32_t codePoint) {
  uint8_t value = 0;
  uint8_t base = 0;
  uint8_t mark = 0;
  if (lookupGlyph(codePoint, value) ||
      lookupMarkedKana(codePoint, base, mark)) {
    return true;
  }
  if ((codePoint >= '0' && codePoint <= '9') ||
      (codePoint >= 0xFF10 && codePoint <= 0xFF19)) {
    return true;
  }
  switch (codePoint) {
    case ' ':
    case 0x3000:
    case '!':
    case 0xFF01:
    case '?':
    case 0xFF1F:
    case 0x3002:
    case 0x30FB:
    case 0x2026:
    case 0x30FC:
      return true;
    default:
      return false;
  }
}

}  // namespace

size_t countUnsupportedGameBoyJapanese(const String& text) {
  size_t unsupported = 0;
  const char* cursor = text.c_str();
  const char* end = cursor + text.length();
  while (cursor < end) {
    if (!isSupportedCodePoint(nextDisplayCodePoint(cursor, end))) ++unsupported;
  }
  return unsupported;
}

size_t countGameBoyJapaneseGlyphs(const String& text) {
  size_t glyphs = 0;
  const char* cursor = text.c_str();
  const char* end = cursor + text.length();
  while (cursor < end) {
    nextDisplayCodePoint(cursor, end);
    ++glyphs;
  }
  return glyphs;
}

size_t measureGameBoyJapaneseBytes(const String& text,
                                   size_t maxVisibleGlyphs,
                                   size_t firstLineGlyphs) {
  size_t bytes = 0;
  size_t visible = 0;
  bool insertedLineBreak = false;
  const char* cursor = text.c_str();
  const char* end = cursor + text.length();

  while (cursor < end && visible < maxVisibleGlyphs) {
    const uint32_t codePoint = nextDisplayCodePoint(cursor, end);
    if (!isSupportedCodePoint(codePoint)) continue;

    if (visible >= firstLineGlyphs && !insertedLineBreak) {
      ++bytes;
      insertedLineBreak = true;
    }
    uint8_t base = 0;
    uint8_t mark = 0;
    bytes += lookupMarkedKana(codePoint, base, mark) ? 2 : 1;
    ++visible;
  }
  return bytes;
}

size_t encodeGameBoyJapanese(const String& text, uint8_t* output,
                             size_t outputCapacity, size_t maxVisibleGlyphs,
                             size_t firstLineGlyphs,
                             size_t* unsupportedCodePoints) {
  if (unsupportedCodePoints != nullptr) *unsupportedCodePoints = 0;
  if (output == nullptr || outputCapacity == 0) return 0;

  size_t written = 0;
  size_t visible = 0;
  bool insertedLineBreak = false;
  const char* cursor = text.c_str();
  const char* end = cursor + text.length();

  auto append = [&](uint8_t value) {
    if (written >= outputCapacity) return false;
    output[written++] = value;
    return true;
  };

  while (cursor < end && visible < maxVisibleGlyphs) {
    const uint32_t codePoint = nextDisplayCodePoint(cursor, end);
    if (codePoint == '\r' || codePoint == '\n' || codePoint == 0x3001) {
      if (unsupportedCodePoints != nullptr) ++*unsupportedCodePoints;
      continue;
    }

    uint8_t value = 0;
    bool hasDirectGlyph = lookupGlyph(codePoint, value);
    uint8_t base = 0;
    uint8_t mark = 0;
    const bool hasMark =
        !hasDirectGlyph && lookupMarkedKana(codePoint, base, mark);
    if (!hasDirectGlyph && !hasMark) {
      if (codePoint >= '0' && codePoint <= '9') {
        value = static_cast<uint8_t>(0xF6 + codePoint - '0');
      } else if (codePoint >= 0xFF10 && codePoint <= 0xFF19) {
        value = static_cast<uint8_t>(0xF6 + codePoint - 0xFF10);
      } else {
        switch (codePoint) {
          case ' ':
          case 0x3000:
            value = 0x7F;
            break;
          case '!':
          case 0xFF01:
            value = 0xE7;
            break;
          case '?':
          case 0xFF1F:
            value = 0xE6;
            break;
          case 0x3002:
            value = 0xE8;
            break;
          case 0x30FB:
            value = 0x74;
            break;
          case 0x2026:
            value = 0x75;
            break;
          case 0x30FC:
            value = 0xE3;
            break;
          default:
            if (unsupportedCodePoints != nullptr) ++*unsupportedCodePoints;
            continue;
        }
      }
    }

    if (visible >= firstLineGlyphs && !insertedLineBreak) {
      if (!append(0x4F)) break;
      insertedLineBreak = true;
    }
    const size_t encodedBytes = hasMark ? 2 : 1;
    if (written + encodedBytes > outputCapacity) break;
    // The Gen I renderer places a standalone mark above the current cursor
    // without advancing it, so a fallback pair must be mark then base.
    if (hasMark && !append(mark)) break;
    if (!append(hasMark ? base : value)) break;
    ++visible;
  }

  while (written > 0 && output[written - 1] == 0x7F) --written;
  if (written == 0) {
    constexpr uint8_t fallback[] = {0x9E, 0x83, 0x94, 0x81};  // ミエナイ
    const size_t count = min(outputCapacity, sizeof(fallback));
    memcpy(output, fallback, count);
    return count;
  }
  return written;
}
