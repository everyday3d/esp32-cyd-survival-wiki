# Thai edition (beta)

ขอบคุณที่ช่วยทดสอบ! Thank you for testing the Thai edition. This is an early beta: the screen, the buttons and the first batch of guides are in Thai, and everything that is not translated yet stays in English.

## How it works
The board's built-in fonts only have English letters, and Thai needs letters stacked above and below each other. So the Thai text is drawn ahead of time on a PC with the open-license Noto Sans Thai font (SIL Open Font License), and the board shows the finished pictures. The Thai guides are scrolling pages. Because the text is pre-drawn:
- Mesh chat messages and names typed in Thai will **not** show on the board's screen (they show as `?`). The phone page shows them fine.
- Numbers, times and distances stay as normal digits.
- Some lines that mix words and numbers (for example "5 satellites, 123 m up") are still English for now.

## Install (about 10 minutes)
1. Open the web flasher, **https://everyday3d.github.io/esp32-cyd-survival-wiki/**, pick your board, then pick **ไทย (Thai, beta)**, plug the board in with a USB data cable and click **Install**. Tick **Erase device**. Needs Chrome or Edge.
2. Format a microSD card as **FAT32**. Download **survival-wiki-sdcard-thai.zip** from the release page and unzip it so the folders `wiki`, `web`, `library` and `th` are in the root of the card.
3. Put the card in the board and power it on. Tap the calibration markers when asked.

## What is translated so far
The home screen, footer buttons, category names, the Triage questions, Settings, Mesh, Wi-Fi, Map, GPS screens, and these guides: CPR, First Aid Priorities, Severe Bleeding, Choking, Shock, Hypothermia and Frostbite, Heat Exhaustion and Heat Stroke, Burns, Broken Bones and Sprains. More guides are coming.

## Please check (and tell us what you see)
1. **Can you read it?** Is the text sharp enough, and big enough, on the 2.8" or 3.5" screen? Are the tone marks and vowels (above and below the letters) clear?
2. **Is it correct?** Does the Thai read naturally? Are words broken in odd places at the end of a line? Are any terms wrong or unnatural, especially in the first aid guides? **Please do not rely on a first aid guide until a Thai speaker has checked it.**
3. **Does it work?** Open a guide from the home screen and from Triage. Scroll up and down. Does it scroll smoothly? Does the screen flicker? Do the buttons respond?
4. **Anything still in English** that should be Thai?

A photo of the screen with a short note is perfect. Send it to Jim, or open an issue at https://github.com/everyday3d/esp32-cyd-survival-wiki/issues

## For contributors
- Interface text: `l10n/th/strings.tsv` (English, a tab, then Thai).
- Guides: `l10n/th/articles/<name>.md`, the same layout as `sdcard/wiki/articles/<name>.md`.
- In the Thai text put `|` where a line may break between words (Thai has no spaces between words). A normal space stays a space.
- Build the pack: `powershell -File scripts/thai/build-thai-pack.ps1`, then copy the `th` folder it makes to the SD card.
- The build flag is `LANG_TH`; the PlatformIO environments are `survival-wiki-28-th` and `survival-wiki-35-th`.
