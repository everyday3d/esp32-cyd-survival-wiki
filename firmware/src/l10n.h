// Thai edition (build flag LANG_TH). The board's fonts only have English letters, so the Thai text is
// drawn ahead of time on a PC (scripts/thai/build-thai-pack.ps1) and the board shows the finished pictures:
//   /th/atlasNN.thb  small pictures for buttons, tiles, headers and titles, found by their English text
//   /th/pNN/<id>.thp whole guide pages
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

  // These draw the Thai picture when one exists for this exact English text, and the English text otherwise.
  int16_t drawString(const String &s, int32_t x, int32_t y);
  int16_t drawString(const char *s, int32_t x, int32_t y);
  int16_t textWidth(const String &s);
  int16_t textWidth(const char *s);
  // The Thai pictures come in five sizes, matched to the font the code asked for.
  void setFreeFont(const GFXfont *f = NULL);
  void setTextFont(uint8_t f);
  uint8_t thStyle = 0;
};
bool l10nBegin();                 // call once the SD card is up. False if there is no Thai pack on the card.
bool l10nHas(const char *s);      // a Thai picture exists for this English text
bool l10nActive();
#else
typedef TFT_eSPI L10nTft;
inline bool l10nBegin() { return false; }
inline bool l10nHas(const char *) { return false; }
inline bool l10nActive() { return false; }
#endif
