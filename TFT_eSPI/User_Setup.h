// ==========================================================
//  User_Setup.h – TFT_eSPI Configuration
//  IMPORTANT: Copy this file to the TFT_eSPI Library folder
//  and replace the existing User_Setup.h file!
//
//  Path (Windows): 
//    Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//  Path (Mac/Linux):
//    ~/Arduino/libraries/TFT_eSPI/User_Setup.h
// ==========================================================

// ── Display Driver ───────────────────────────────
#define ILI9341_2_DRIVER

// ── Display Size ─────────────────────────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ── Mini D1 ESP32 Pins ───────────────────────────
#define TFT_MISO  19
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_CS    22
#define TFT_DC    21
#define TFT_RST   -1   // Reset directly to ESP32 RST Pin

// ── Touch Controller XPT2046 ─────────────────────
#define TOUCH_CS  26

// ── SPI Frequency ────────────────────────────────
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

// ── Color Settings ───────────────────────────────
// This might need some tweaking for different displays
// #define TFT_RGB_ORDER TFT_RGB
#define TFT_INVERSION_ON

// ── Load Fonts ──────────────────────────────────
#define LOAD_GLCD    // Font 1. Original Adafruit 8 Pixel Font
#define LOAD_FONT2   // Font 2. Small 16 pixel high font
#define LOAD_FONT4   // Font 4. Medium 26 pixel high font
#define LOAD_FONT6   // Font 6. Large 48 pixel high font
#define LOAD_FONT7   // Font 7. 7 Segment 48 pixel high font
#define LOAD_FONT8   // Font 8. Large 75 pixel high font
#define LOAD_GFXFF   // FreeFonts
#define SMOOTH_FONT
