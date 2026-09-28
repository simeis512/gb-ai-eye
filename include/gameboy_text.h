#pragma once

#include <Arduino.h>

size_t countUnsupportedGameBoyJapanese(const String& text);
size_t countGameBoyJapaneseGlyphs(const String& text);
size_t measureGameBoyJapaneseBytes(const String& text,
                                   size_t maxVisibleGlyphs,
                                   size_t firstLineGlyphs);

size_t encodeGameBoyJapanese(const String& text, uint8_t* output,
                             size_t outputCapacity, size_t maxVisibleGlyphs,
                             size_t firstLineGlyphs,
                             size_t* unsupportedCodePoints = nullptr);
