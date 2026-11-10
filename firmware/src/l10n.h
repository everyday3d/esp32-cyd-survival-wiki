// Thai edition (build flag LANG_TH). The board's own fonts only have English letters, so Thai is drawn from
// glyph tables in flash (thai_font_data.*, Sarabun, made on a PC by scripts/thai/build_thai_font.py).
// Any Thai text can be drawn, including text typed by a phone or read from the SD card.
//   - Text from the SD card (guides, titles, categories) is already Thai and is drawn as it is.
//   - Text typed in the firmware is English. It is looked up in th_strings.h (scripts/thai/build-thai-strings.ps1)
//     and the Thai is drawn instead. Text with no entry stays English.
// In the normal build none of this exists and L10nTft is just TFT_eSPI.
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#ifdef LANG_TH
class L10nTft : public TFT_eSPI {
public:
  using TFT_eSPI::drawString;
  using TFT_eSPI::textWidth;
  using TFT_eSPI::setFreeFont;
  using TFT_eSPI::setTextFont;

  // Draw and measure Thai when the text is Thai or has a Thai entry, the normal English font otherwise.
  int16_t drawString(const String &s, int32_t x, int32_t y);
  int16_t drawString(const char *s, int32_t x, int32_t y);
  int16_t textWidth(const String &s);
  int16_t textWidth(const char *s);
  // The Thai glyphs come in five sizes, matched to the font the code asked for.
  void setFreeFont(const GFXfont *f = NULL);
  void setTextFont(uint8_t f);
  void setThaiSize(uint8_t px);          // 12, 14, 16, 18 or 22
  int thaiLineHeight() const;            // tallest stack of vowels and tone marks, for line spacing
  uint8_t thStyle = 2;
private:
  int thaiRender(const char *s, int x, int y, bool paint);
};
bool l10nBegin();                        // kept for the SD-up call: nothing to load, the glyphs are in flash
bool l10nHas(const char *s);             // the text is Thai, or has a Thai entry
bool l10nActive();
const char *l10nTr(const char *s);       // the Thai for this English text, or s itself
bool l10nIsThai(const char *s);          // has at least one non-ASCII byte
#else
typedef TFT_eSPI L10nTft;
inline bool l10nBegin() { return false; }
inline bool l10nHas(const char *) { return false; }
inline bool l10nActive() { return false; }
#endif
