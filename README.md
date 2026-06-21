# KidsBoard – Family Weekly Planner

![DrPi](tools/assets/drpi.png)

A tactile digital weekly planner for families, built with an ESP32, a 2.8" touch display, and RFID card login. Each family member gets their own RFID card to log in and check off their tasks for the week. Rewards are automatically calculated based on the weekly score.

![KidsBoard](docs/kidsboard.jpg)

---

## AI Disclosure

This project was developed almost entirely with the help of [Claude](https://claude.ai) (Anthropic). Pixel art assets were generated with ChatGPT (OpenAI). The hardware concept and overall direction came from me – but the vast majority of the code, including the display driver integration, WiFi management, web interface, screensaver animations, and battery monitoring, was written by Claude in an iterative back-and-forth conversation. I believe in being transparent about this: AI-assisted development is a legitimate and powerful approach in non-safety-critical contexts for makers who want to go beyond their current skill level. I could eventually have figured all of this out myself – but I almost certainly would never have started.

---

## Features

- 🃏 **RFID login** – each family member has their own card
- ✅ **Weekly task planner** – Monday to Friday, configurable per person
- 🧠 **Dr. Pi quiz mode** – tasks can require a quiz to be completed before they're checked off, with optional AI-generated questions via the Anthropic API
- 🏆 **Reward system** – minutes of screen time, items, money, or mystery rewards
- 🎮 **Retro pixel art UI** – day/night background, animated cloud screensaver, star screensaver at night
- 📱 **Web interface** – configure tasks, rewards and RFID cards from any browser
- 📡 **WiFi setup via Access Point** – no hardcoded credentials, configure via browser on first boot
- 🔋 **Battery monitoring** – percentage and voltage display, low battery warning, deep sleep after 30 minutes
- 💡 **PWM backlight** – dims on low battery
- 🔄 **OTA updates** – flash new firmware wirelessly

---

## Hardware

| Part | Description | Approx. Price |
|---|---|---|
| Mini D1 ESP32 (AZDelivery) | Microcontroller | ~5€ |
| ILI9341 2.8" Touch Display (TPM408-2.8) | Display with XPT2046 touch | ~8€ |
| RC522 RFID Reader | 13.56 MHz SPI | ~2€ |
| TP5400 Module | LiPo charger + 5V boost | ~2€ |
| Sony VTC6 18650 or LiPo 3.7V | Battery | ~6€ |
| Reed switch (NC) | Power on/off when box opens | ~1€ |
| Voltage divider (2× 100kΩ) | Battery level monitoring | <1€ |
| Delock panel-mounted Micro-USB extension | External USB jack for charging | ~5€ |
| Wooden or 3D printed box | Housing | ~5€ |

**Total: ~35€**

---

## Wiring

### ILI9341 2.8" Display → ESP32

```
Display Pin    ESP32 Pin    Notes
───────────────────────────────────────────
VCC        →   3.3V
GND        →   GND
CS         →   GPIO 22      Display chip select
RESET      →   RST          Connect to ESP32 RST pin directly
DC/RS      →   GPIO 21      Data/Command
MOSI/SDI   →   GPIO 23      Shared SPI bus
SCK/CLK    →   GPIO 18      Shared SPI bus
LED        →   GPIO 16      PWM backlight control
MISO/SDO   →   GPIO 19      Shared SPI bus
T_CS       →   GPIO 26      Touch chip select
```

### RC522 RFID → ESP32

```
RC522 Pin    ESP32 Pin    Notes
──────────────────────────────────────────
VCC      →   3.3V         ⚠️ 3.3V only, NOT 5V!
GND      →   GND
MOSI     →   GPIO 23      Shared with display
MISO     →   GPIO 19      Shared with display
SCK      →   GPIO 18      Shared with display
SDA/CS   →   GPIO 5       RFID chip select
RST      →   GPIO 17
```

> 💡 Display and RFID share the SPI bus (MOSI/MISO/SCK). Each device has its own CS pin.

> 💡 Some display modules (like the TPM408-2.8) have separate SPI pins for the touch controller that are not internally connected to the display SPI pins. In that case, connect the touch SPI pins (MOSI, MISO, SCK) to the same ESP32 GPIO pins as the display.

### Voltage Divider (Battery Monitoring)

```
Battery+ ── 100kΩ ── GPIO34 ── 100kΩ ── GND
```

GPIO34 measures half the battery voltage. The ADC calibration factor in `kidsboard_retro.ino` may need adjustment – see [Battery Calibration](#battery-calibration) below.

If you want to improve ADC accuracy, consider using a different resistor ratio and attenuation setting as described here: https://esp32.com/viewtopic.php?t=38340. Note that this requires minor code changes. I learned about this too late and kept the 50% voltage divider as-is.

### Power Supply

```
18650 Battery
    │
    ├── TP5400 (B+/B-)     ← Charge via Micro-USB
         │
        USB-A out
         │
        USB cable
         │
    Reed switch (NC)
         │
        ESP32 USB port
```

**Logic:**
- Box open → magnet away from reed switch → NC contacts closed → **power on**
- Box closed → magnet close to reed switch → NC contacts open → **power off**

To charge: route a panel-mounted Micro-USB extension to the outside of the box.

---

## Setup

### 1. Configure TFT_eSPI

Copy the included `User_Setup.h` into the TFT_eSPI library folder, replacing the existing file:

- **Windows:** `Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`
- **Mac/Linux:** `~/Arduino/libraries/TFT_eSPI/User_Setup.h`

### 2. Select partition scheme

In Arduino IDE, select:
- Board: `ESP32 Dev Module`
- Partition Scheme: `Minimal SPIFFS (Large APP with OTA)`
- Upload Speed: `921600`

### 3. Flash the firmware

Upload `kidsboard_retro.ino` via USB.

### 4. Configure WiFi

On first boot, KidsBoard starts as a WiFi Access Point named **KidsBoard**.

1. Connect your phone or laptop to the `KidsBoard` WiFi network
2. Open a browser and go to `http://192.168.4.1/wifi`
3. Enter your home network name and password
4. KidsBoard restarts and connects to your network

The status bar at the bottom shows:
- `WiFi` – connected to home network (tap to show IP address)
- `AP: KidsBoard` – running as access point (tap to show IP)

> 💡 The web interface is accessible in both modes – you can use KidsBoard completely without a home network by connecting directly to the `KidsBoard` access point.

### 5. Calibrate touch

Flash the example `TFT_eSPI → examples → Generic → Touch_calibrate`, tap the four corners, note the values, and enter them in `display_ui.h`:

```cpp
#define TOUCH_CAL_DATA { 395, 3227, 305, 3361, 4 }
//                        ↑ your values here
```

> ⚠️ Make sure `tft.setRotation(2)` is used in the calibration sketch to match KidsBoard's orientation.

### 6. Assign RFID cards

1. Open a browser and go to `http://[KidsBoard IP]`
2. Click "Assign next card" next to a family member
3. Hold an RFID card on the RC522 reader
4. Save

### 7. Configure tasks and rewards

Use the web interface to set up tasks for each day and family member, and define rewards (screen time, items, money, mystery surprises).

---

## Battery Calibration

KidsBoard displays the battery percentage in the bottom status bar. Because the ESP32 ADC is not perfectly linear, a calibration factor is applied.

To calibrate:
1. Charge the battery fully
2. Tap the battery percentage in the status bar – it switches to voltage display (e.g. `3.97V`)
3. Measure the actual battery voltage with a multimeter
4. Calculate the correction factor:
   ```
   new_factor = (multimeter_voltage / displayed_voltage) × current_factor
   ```
5. Update `1.117` in `kidsboard_retro.ino`:
   ```cpp
   float voltage = (raw / 4095.0) * 3.3 * 2.0 * 1.117; // ← adjust this
   ```
6. Flash again via OTA

> 💡 Tap the battery display again to switch back to percentage.

---

## WiFi Notes

KidsBoard connects to WiFi before initializing the display. This is intentional – some ESP32 boards experience a brief brownout during the first WiFi connection which would cause a reboot. By keeping the display off during WiFi init, the reboot is invisible to the user.

If your power supply is stable, you can move the WiFi setup after `showBootScreen()` in `setup()` for a faster boot experience (the comment in the code explains this).

---

## Usage

| Action | Result |
|---|---|
| Place RFID card | Log in as that family member |
| Place same card again | Log out |
| Tap a task | Check / uncheck |
| Tap a quiz task | Start the Dr. Pi quiz |
| Answer a question | Tap A / B / C / D |
| Wrong answer | Dr. Pi looks skeptical – try again |
| All correct on first try | Task marked as done, Dr. Pi celebrates |
| Tap a completed quiz task | Replay it, or reset it |
| Tap a day tab | Switch day |
| Tap "Week" button | Show reward screen |
| 60s no interaction | Auto logout |
| Tap WiFi label | Toggle IP address display |
| Tap battery % | Toggle voltage display |
| Box open for 30 min | Deep sleep (close/open box to wake) |

---

## Creating Custom Avatars

Each family member is represented by a 32×32 pixel art avatar. Here's how to create your own:

### Step 1 – Take a photo
Take a portrait photo of the family member (or use any image you like).

### Step 2 – Generate pixel art with ChatGPT
Upload the photo to ChatGPT and prompt:

> *"Convert this photo to 16-bit pixel art style, 32x32 pixels, simple colors, clear face features, transparent or solid background"*

Download the result as a PNG.

### Step 3 – Convert to RGB565 C array
Use the included `tools/convert_avatar.py` script:

```bash
# Install dependencies (once)
pip install Pillow numpy

# Convert your avatar
python3 convert_avatar.py my_avatar.png --name AVATAR_MYNAME
```

The script outputs a C array ready to paste into `retro_assets.h`:

```cpp
// AVATAR_MYNAME: 32x32px, 2048 bytes
const uint16_t AVATAR_MYNAME[1024] PROGMEM = {
  0x1234, 0x5678, ...
};
```

You can adjust the enhancement settings if needed:
```bash
python3 convert_avatar.py my_avatar.png --name AVATAR_MYNAME --saturation 2.0 --contrast 1.3
```

> 💡 The ILI9341 display renders colors darker than a monitor. The default settings (`saturation=2.2, contrast=1.4`) compensate for this. Adjust if the result looks too washed out or oversaturated on your display.

### Step 4 – Add to the project

1. Paste the C array into `retro_assets.h`
2. Add it to `AVATAR_DATA[]` in `display_ui.h`:
```cpp
const uint16_t* AVATAR_DATA[] = {
  AVATAR_MILA,
  AVATAR_FELIX,
  AVATAR_MAMA,
  AVATAR_MYNAME,  // ← add here, matching the order of kids[] in data.h
};
```
3. Flash via OTA

---

## Dr. Pi Quiz Mode

Any task can optionally require a quiz before it can be checked off – useful for learning tasks like "study vocabulary" or "practice math" where you want proof of actual learning, not just a tapped checkbox.

### How it works

1. In the web interface, open a task's quiz editor and enter a topic (e.g. `Times tables up to 10` or `English 5th grade: going to – future tense`)
2. Click **🤖 Generate with AI** to create 20 questions via the Anthropic API, or add questions manually – no API key required for manual entry
3. When the child taps the task, Dr. Pi 🐙🎓 appears and asks 5 randomly selected questions with shuffled answer order
4. Only questions answered correctly on the **first try** count
5. A perfect score (5/5) marks the task as done; anything less and the child can try again

> 💡 The more specific the topic, the better the questions. `English 5th grade: going to – future tense` works much better than `English`.

If a task has the same quiz topic on multiple days, the question bank is shared across all of them – you only need to generate or enter questions once per task.

### Setting up AI question generation

1. Get an Anthropic API key at [console.anthropic.com](https://console.anthropic.com)
2. Consider creating a separate workspace with a monthly spending limit – a few cents per month covers occasional question generation
3. Open the KidsBoard web interface, go to **Settings** (⚙ icon, top right)
4. Enter your API key and optionally change the model (default: `claude-haiku-4-5-20251001` – fast and cheap; `claude-sonnet-4-6` gives noticeably better questions for less common topics)

Questions are generated once and stored locally on the device. After generation, the quiz works completely offline. The API key is stored in SPIFFS and never transmitted to the browser – the ESP32 calls the Anthropic API directly over HTTPS.

### Manual question entry

No API key needed – open a task's quiz editor and add questions one at a time. Each needs exactly 4 answer options with one marked correct. There's no minimum: with fewer than 5 questions, all of them are shown every round instead of a random subset.

### Dr. Pi sprites

Dr. Pi is a 64×64 pixel art octopus professor, rendered at 128×128 on the display. Three variants are used:

| Sprite | Used when |
|---|---|
| `DR_PI` | Asking a question |
| `DR_PI_HAPPY` | Correct answer / quiz complete |
| `DR_PI_SKEPTICAL` | Wrong answer |

To create custom Dr. Pi sprites, use `tools/convert_drpi.py`:

```bash
python3 tools/convert_drpi.py my_sprite.png DR_PI
python3 tools/convert_drpi.py my_sprite.png DR_PI_HAPPY
python3 tools/convert_drpi.py my_sprite.png DR_PI_SKEPTICAL
```

The script handles transparent backgrounds automatically (with a flood-fill fallback if the source PNG has no alpha channel) and applies the byte-swap needed for the ILI9341 display. Paste the output into `retro_assets.h`.

---

## Creating a Custom Skin

While `convert_avatar.py` adds a single avatar, `tools/generate_assets.py` regenerates the entire visual theme at once – background (day + night), all four avatars, the cloud sprite, and the sky mask used for the screensaver animation.

This is useful if you want to replace the whole look (e.g. a winter theme, a space theme, etc.) rather than just adding a family member.

### How it works

1. Create a new background image (240×320px, pixel art style) with a single flat sky color
2. **Note the ground line.** Dr. Pi stands on your background during the quiz, so the display needs to know where the "ground" is. Measure how many pixels from the **bottom** of your 240×320 background the ground/horizon line sits (e.g. grass meeting sky), then update `QUIZ_BG_OFFSET` in `display_ui.h` accordingly – the comment above the constant explains the calculation. If you skip this, Dr. Pi may appear to float above or sink below the ground in the quiz screen.
3. Place your source images in `tools/assets/`:
   ```
   tools/assets/
   ├── bg_day.png          ← your new background
   ├── cloud.png           ← cloud sprite (can stay the same)
   ├── avatar_mila.png
   ├── avatar_felix.png
   ├── avatar_mama.png
   └── avatar_papa.png
   ```
4. Run the generator, specifying the sky color of your new background:
   ```bash
   cd tools
   python3 generate_assets.py --bg assets/bg_day.png --sky-rgb 91,198,232 \
     --cloud assets/cloud.png \
     --mila assets/avatar_mila.png --felix assets/avatar_felix.png \
     --mama assets/avatar_mama.png --papa assets/avatar_papa.png \
     --output ../retro_assets.h --mask ../sky_mask.h
   ```
5. Copy the generated `retro_assets.h` and `sky_mask.h` into the sketch folder, replacing the existing ones
6. Flash via OTA

> 💡 The script automatically samples the sky color from the *enhanced* background (top-right corner) and applies it to the cloud asset too, so cloud sky pixels blend seamlessly into the background without visible seams. No manual color matching needed.

> 💡 `--sky-rgb` should match the flat sky color in your *source* background image (before enhancement). Use a pixel color picker to find the exact value if you're not sure.

---

## Localization

The project is set up in German but changing the language should be straightforward. All display strings are in `i18n.h`. All strings for the web interface are in the `S` object at the top of the `<script>` block in `webserver.h` – edit there to translate, everything else is logic.

---

## Project Structure

```
kidsboard_retro/
├── kidsboard_retro.ino   Main sketch
├── config.h              Pins, colors, timing constants
├── i18n.h                All display strings (localization)
├── data.h                Data structures, SPIFFS storage, quiz helpers
├── display_ui.h          All screens + touch handlers (incl. Dr. Pi quiz screens)
├── retro_gfx.h           Pixel art rendering functions
├── retro_assets.h        RGB565 pixel art assets (generated)
├── sky_mask.h            1-bit sky mask for cloud/star animation
├── rfid_reader.h         RFID card reading + assignment
├── webserver.h           Web interface + REST API
├── wifi_manager.h        WiFi connection + AP setup mode
├── TFT_eSPI/
│   └── User_Setup.h      TFT_eSPI config (copy to library folder!)
└── tools/
    ├── convert_avatar.py Add a single custom avatar
    ├── convert_drpi.py   Convert Dr. Pi sprite PNGs to RGB565 C arrays
    ├── generate_assets.py Regenerate the full visual theme (skin)
    └── assets/           Source images used by generate_assets.py
```

---

## Troubleshooting

**Display stays white/black**
→ Did you copy `User_Setup.h` correctly? Try enabling and/or swapping `TFT_RGB_ORDER` (RGB ↔ BGR) or disabling `TFT_INVERSION` in `User_Setup.h`

**Touch doesn't respond**
→ Run the `Touch_calibrate` example and update `TOUCH_CAL_DATA` in `display_ui.h`. Make sure `tft.setRotation(2)` is used in the calibration sketch.

**RFID not detected**
→ Check 3.3V supply (never 5V!), check CS pin is GPIO 5

**WiFi won't connect**
→ Connect to the `KidsBoard` AP, go to `192.168.4.1/wifi` and re-enter credentials

**Colors look wrong**
→ Change `TFT_RGB_ORDER` in `User_Setup.h` from `TFT_RGB` to `TFT_BGR`, or try disabling `TFT_INVERSION`

**Battery shows wrong percentage**
→ Tap the battery display to see the voltage, compare with a multimeter and adjust the calibration factor (see [Battery Calibration](#battery-calibration))

**Boot loop / constant rebooting**
→ The ESP32 may brownout during WiFi init. This is normal on battery power – the display stays off during the reboot so it's invisible. If it keeps looping, try reducing WiFi TX power in `wifi_manager.h`: `WiFi.setTxPower(WIFI_POWER_2dBm)` or run with a stronger USB power supply.

**Device shows battery empty even though it is fully charged**
→ Check that your voltage divider is connected to GPIO34. If you don't want to measure battery level at all, change `readBatteryPercent()` to always return 100, and remove the battery display code after `// Battery right` in `display_ui.h`.

**Dr. Pi appears to float or sink into the ground on a custom skin**
→ Adjust `QUIZ_BG_OFFSET` in `display_ui.h` to match your background's ground line – see [Creating a Custom Skin](#creating-a-custom-skin).

**Quiz generation fails or times out**
→ Larger/slower models (e.g. `claude-sonnet-4-6`) can take longer than the default 90s timeout for 20 questions. Increase the timeout in `callAnthropicForQuiz()` in `webserver.h` if needed.

---

## License

MIT License – feel free to use, modify and share.
