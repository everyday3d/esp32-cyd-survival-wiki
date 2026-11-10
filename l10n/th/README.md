# Thai edition (beta)

ขอบคุณที่ช่วยทดสอบ! Thank you for testing the Thai edition. The screen, the buttons, all 118 guides and the US Army manual are in Thai.

## Credit
The Thai translation (guides, titles, category names, the translated manual, the Thai diagrams and many of the screen labels) and the idea of drawing Thai text on the board from glyph tables are the work of **[zmikiz1423](https://github.com/zmikiz1423/esp32-cyd-survival-wiki-thai)**, who built a full Thai edition and agreed to merge it into this project (issue #35). The first screen strings and the first nine guides were written here and are replaced by his versions where both exist. The font is Sarabun (SIL Open Font License, `firmware/fonts/OFL-Sarabun.txt`). Province offline maps are still only in his repository.

## How it works
The board's built-in fonts only have English letters, and Thai needs letters stacked above and below each other. So a PC program (`scripts/thai/build_thai_font.py`, HarfBuzz) works out the shape of every Thai letter group ahead of time and the firmware carries the finished pictures in flash. The board can then draw any Thai text it meets: a guide, a title, a mesh message typed in Thai.
- Text on the SD card (guides, titles, categories, books) is already Thai and is drawn as it is.
- Text built into the firmware is English. It is looked up in `firmware/src/th_strings.h` and the Thai is drawn instead. Text with no entry stays English.
- Numbers, times and distances stay as normal digits.

## Install (about 10 minutes)
1. Flash the Thai firmware for your board (a full image, written at offset 0). Tick **Erase device**. Needs Chrome or Edge.
2. Format a microSD card as **FAT32**. Unzip **survival-wiki-sdcard-thai.zip** so the folders `wiki`, `web` and `library` are in the root of the card.
3. Put the card in the board and power it on. Tap the calibration markers when asked.

## Please check (and tell us what you see)
1. **Can you read it?** Is the text sharp enough, and big enough, on the 2.8" or 3.5" screen? Are the tone marks and vowels clear?
2. **Is it correct?** Does the Thai read naturally? Are words broken in odd places at the end of a line? Please check the first aid guides especially: nobody should rely on a first aid guide until a Thai speaker has checked it.
3. **Does it work?** Open a guide from the home screen and from Triage. Scroll up and down. Does it scroll smoothly? Does the screen flicker? Do the buttons respond?
4. **Anything still in English** that should be Thai? Counts like "5 satellites" and lines that mix words and numbers may still be English.

A photo of the screen with a short note is perfect. Open an issue at https://github.com/everyday3d/esp32-cyd-survival-wiki/issues

## For contributors
- Screen labels: `l10n/th/strings.tsv` and `strings-titles.tsv` (English, a tab, then Thai) and `community/thai_ui_strings.json` (wins when both have an entry). The English must match the firmware text exactly. A `|` or a zero width space marks where a line may break between words. After a change run `powershell -File scripts/thai/build-thai-strings.ps1`, which rewrites `firmware/src/th_strings.h`.
- Guides, titles, search index, diagrams and books: `l10n/th/sdcard/`, the same layout as `sdcard/`. Put a zero width space (U+200B) between Thai words so lines wrap at word ends.
- Build the card: `powershell -File scripts/thai/build-thai-sdcard.ps1 -Out <folder> [-Pdfs <folder with the two PDFs>] [-Zip <file.zip>]`.
- New letters or sizes: edit and run `scripts/thai/build_thai_font.py` (needs Python with uharfbuzz, freetype-py, fonttools, pillow).
- The build flag is `LANG_TH`; the PlatformIO environments are `survival-wiki-28-th` and `survival-wiki-35-th`.
