// =====================================================
//  config.h – Pins, Settings
// =====================================================
#pragma once

// WiFi credentials are stored in NVS via Preferences
// Configure them via the web interface (AP mode on first boot)
#define WIFI_AP_SSID    "KidsBoard"
#define WIFI_AP_PASS    ""           // Open AP, no password

#define PIN_BAT_ADC 34

// ── SPI Pins (shared by Display + RFID) ───────────
//    ESP32 WROOM-32 Standard SPI (VSPI)
#define PIN_MOSI    23
#define PIN_MISO    19
#define PIN_SCK     18

// ── ILI9341 Display ───────────────────────────────
#define PIN_TFT_CS  22
#define PIN_TFT_DC  21
#define PIN_TFT_LED 16

// ── Touch XPT2046 ─────────────────────────────────
#define PIN_TOUCH_CS  26

// ── RC522 RFID ────────────────────────────────────
#define PIN_RFID_CS   5
#define PIN_RFID_RST 17

// ── Settings ──────────────────────────────────────
#define MAX_KIDS          4
#define MAX_TASKS_PER_DAY 5
#define DAYS_COUNT        5   // Monday to Friday
#define AUTO_LOGOUT_MS    60000  // 60 seconds

// ── Colors (RGB565) ───────────────────────────────
#define COLOR_BG        0x0841
#define COLOR_CARD      0x18C3
#define COLOR_BORDER    0x2945
#define COLOR_TEXT      0xFFFF
#define COLOR_MUTED     0x8410
#define COLOR_SUCCESS   0x07E0
#define COLOR_KID_0     0xF81F
#define COLOR_KID_1     0x07FF
#define COLOR_KID_2     0xFFE0
#define COLOR_KID_3     0xF800

// ── Screens ───────────────────────────────────────
enum Screen {
  SCREEN_HOME,
  SCREEN_PLANNER,
  SCREEN_WEEKEND,
  SCREEN_SCREENSAVER,
};

#define SCREENSAVER_MS  120000   // 2 minutes
#define DEEP_SLEEP_MS  1800000   // 30 minutes