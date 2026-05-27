# KidsBoard – Family Weekly Planner

A tactile digital weekly planner for families, built with an ESP32, a 2.8" touch display, and RFID card login. Each family member gets their own RFID card to log in and check off their tasks for the week. Rewards are automatically calculated based on the weekly score.

Demo: https://drrsatzteil.github.io/KidsBoard/

The demo is a web interface that simulates the functionality of the device and is not an exact replica of the actual device. The colors of the visual assets are highly saturated to compensate the limited color palette of the display and therefore look overly saturated on a normal computer display.

---

## AI Disclosure

This project was developed almost entirely with the help of [Claude](https://claude.ai) (Anthropic). Pixel art assets were generated with ChatGPT (OpenAI). The hardware concept and overall direction came from me – but the vast majority of the code, including the display driver integration, WiFi management, web interface, screensaver animations, and battery monitoring, was written by Claude in an iterative back-and-forth conversation. I believe in being transparent about this: AI-assisted development is a legitimate and powerful approach in non-safety-critical contexts for makers who want to go beyond their current skill level. I could eventually have figured all of this out myself – but I almost certainly would never have started.

---

## Features

- 🃏 **RFID login** – each family member has their own card
- ✅ **Weekly task planner** – Monday to Friday, configurable per person
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
RESET      →   GPIO 4
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

## Localization

The project is set up in German but changing the language should be straightforward. All display strings are in `i18n.h`. All strings for the web interface are in the `STRINGS` and `DAYS` arrays in `webserver.h`.

---

## Project Structure

```
kidsboard_retro/
├── kidsboard_retro.ino   Main sketch
├── config.h              Pins, colors, timing constants
├── i18n.h                All display strings (localization)
├── data.h                Data structures, SPIFFS storage
├── display_ui.h          All screens + touch handlers
├── retro_gfx.h           Pixel art rendering functions
├── retro_assets.h        RGB565 pixel art assets (generated)
├── sky_mask.h            1-bit sky mask for cloud/star animation
├── rfid_reader.h         RFID card reading + assignment
├── webserver.h           Web interface + REST API
├── wifi_manager.h        WiFi connection + AP setup mode
├── TFT_eSPI/
│   └── User_Setup.h      TFT_eSPI config (copy to library folder!)
└── tools/
    ├── convert_avatar.py Python script to convert PNG to C array
    └── demo.png          Example avatar image
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

---

## License

MIT License – feel free to use, modify and share.