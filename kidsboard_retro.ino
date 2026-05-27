// =====================================================
//  KidsBoard – Family weekly planner
//  Mini D1 ESP32 + ILI9341 2.8" + RC522 RFID
//  v2: WiFi via AP setup, Display LED via IO16
// =====================================================

#include <SPI.h>
#include <TFT_eSPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <esp_task_wdt.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "config.h"
#include "wifi_manager.h"
#include "data.h"
#include "display_ui.h"
#include "rfid_reader.h"
#include "webserver.h"

#define PWM_CHANNEL    0
#define PWM_FREQ    5000
#define PWM_RES        8
#define BRIGHT_FULL  255
#define BRIGHT_DIM   128
#define BAT_LOW       20   // % for dimmed display
#define BAT_CRIT      10   // % for shutdown

// ── State ─────────────────────────────────────────
AppState state;
int cachedDayIndex = -1;

// ── Delayed Save ──────────────────────────────────
bool savePending = false;
unsigned long saveTimer = 0;

// ── Battery status refresh ────────────────────────
unsigned long lastBatRefresh = 0;
unsigned long lastBatSample = 0;
#define BAT_SAMPLE_MS   (5UL * 1000UL)   // Sample every 5 seconds
#define BAT_REFRESH_MS  (60UL * 1000UL)  // Redraw status bar every 1 minute

// ── Hardware ──────────────────────────────────────
TFT_eSPI       tft  = TFT_eSPI();
MFRC522        rfid(PIN_RFID_CS, PIN_RFID_RST);
AsyncWebServer server(80);

// ── NTP in background ─────────────────────────────
void ntpTask(void* parameter) {
  configTime(3600, 3600, "pool.ntp.org");
  struct tm ti;
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&ti, 500)) {
      int d = ti.tm_wday;
      cachedDayIndex = (d == 0 || d == 6) ? 0 : d - 1;
      Serial.printf("NTP sync: day %i\n", cachedDayIndex);
      break;
    }
  }
  vTaskDelete(NULL);
}

// ── Brightness ────────────────────────────────────
void setBrightness(int brightness) {
  ledcWrite(PIN_TFT_LED, brightness);
}

// ── Battery percent with 32-value rolling average ─
#define BAT_SAMPLES 32
static int batRolling[BAT_SAMPLES] = {0};
static int batRollingIdx = 0;
static bool batRollingFilled = false;

int readBatteryPercent() {
  // Add new reading to rolling buffer
  batRolling[batRollingIdx] = analogRead(PIN_BAT_ADC);
  batRollingIdx = (batRollingIdx + 1) % BAT_SAMPLES;
  if (batRollingIdx == 0) batRollingFilled = true;

  // Average over filled samples
  int count = batRollingFilled ? BAT_SAMPLES : batRollingIdx;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += batRolling[i];
  float raw = sum / (float)count;

  float voltage = (raw / 4095.0) * 3.3 * 2.0 * 1.117;
  int pct = (int)((voltage - 3.0) / (4.2 - 3.0) * 100.0);
  pct = constrain(pct, 0, 100);

  // Only allow value to rise if voltage clearly increased (charging detected)
  // Prevents ADC noise from showing false battery increases
  static int lastPct = -1;
  static float lastVoltage = 0.0f;
  if (lastPct < 0) {
    lastPct = pct;
    lastVoltage = voltage;
  } else if (pct > lastPct) {
    // Only accept increase if voltage rose by at least 0.05V (charging)
    if (voltage > lastVoltage + 0.05f) {
      lastPct = pct;
      lastVoltage = voltage;
    } else {
      pct = lastPct;  // Suppress noise-induced increase
    }
  } else {
    lastPct = pct;
    lastVoltage = voltage;
  }
  return pct;
}

// Returns smoothed battery voltage using same rolling buffer
float readBatteryVoltage() {
  int count = batRollingFilled ? BAT_SAMPLES : max(1, batRollingIdx);
  long sum = 0;
  for (int i = 0; i < count; i++) sum += batRolling[i];
  float raw = sum / (float)count;
  return (raw / 4095.0) * 3.3 * 2.0 * 1.117;
}

// ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n== KidsBoard booting ==");

  // PWM for backlight
  ledcAttach(PIN_TFT_LED, PWM_FREQ, PWM_RES);
  setBrightness(0);

  // ADC setup for battery measurement
  analogSetAttenuation(ADC_11db);  // Allows reading up to ~3.3V

  // Pre-fill rolling average buffer at boot
  for (int i = 0; i < BAT_SAMPLES; i++) {
    batRolling[i] = analogRead(PIN_BAT_ADC);
    delay(2);
  }
  batRollingFilled = true;

  // Check battery before anything else
  int batPct = readBatteryPercent();
  if (batPct < BAT_CRIT) {
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);
    setBrightness(BRIGHT_DIM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    int tw = tft.textWidth(STR_BAT_EMPTY);
    tft.setCursor((240 - tw) / 2, 110);
    tft.print(STR_BAT_EMPTY);
    tft.setTextFont(2);
    tft.setTextColor(0xFFE0, TFT_BLACK);
    tw = tft.textWidth(STR_BAT_CHARGE);
    tft.setCursor((240 - tw) / 2, 155);
    tft.print(STR_BAT_CHARGE);
    tft.setTextFont(1);
    tft.setTextColor(COLOR_MUTED, TFT_BLACK);
    tw = tft.textWidth(STR_BAT_SHUTDOWN);
    tft.setCursor((240 - tw) / 2, 185);
    tft.print(STR_BAT_SHUTDOWN);
    delay(5000);
    setBrightness(0);
    esp_deep_sleep_start();
  }

  // Watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,
    .idle_core_mask = 0,
    .trigger_panic = false
  };
  esp_task_wdt_reconfigure(&wdt_config);

  // WiFi: try stored credentials, fall back to AP
  // Backlight is still off – no visible reboot if brownout occurs
  KBWiFiMode wfMode = startWiFi();
  state.wifiOk = isWiFiConnected();
  state.apMode = isAPMode();

  if (state.wifiOk) {
    xTaskCreate(ntpTask, "ntp", 4096, NULL, 1, NULL);
  }

  // CS Pins HIGH before SPI
  pinMode(PIN_RFID_CS, OUTPUT);
  digitalWrite(PIN_RFID_CS, HIGH);
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);

  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed!");
    return;
  }

  // RFID first – before tft.init()!
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
  bool rfidOk = false;
  for (int attempt = 0; attempt < 5; attempt++) {
    rfid.PCD_Init();
    delay(50);
    byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
    if (version != 0x00 && version != 0xFF) {
      rfidOk = true;
      Serial.printf("RFID OK after %d tries: 0x%02X\n", attempt + 1, version);
      break;
    }
    Serial.printf("RFID try %d failed, retry...\n", attempt + 1);
    delay(100);
  }
  if (!rfidOk) Serial.println("RFID Init failed!");
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  Serial.println("RFID ready");

  // Display after RFID
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  showBootScreen(tft);

  rfid.PCD_Init();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);

  // Data
  loadData(state);

  // Webserver + ElegantOTA
  setupWebserver(server, state, tft);
  ElegantOTA.begin(&server);
  server.begin();
  if (state.wifiOk) {
    Serial.println("OTA: http://" + WiFi.localIP().toString() + "/update");
  } else {
    Serial.println("AP mode: http://" + WiFi.softAPIP().toString());
  }

  // Backlight on
  setBrightness(batPct < BAT_LOW ? BRIGHT_DIM : BRIGHT_FULL);

  // Home Screen
  state.screen = SCREEN_HOME;
  drawHomeScreen(tft, state);

  Serial.println("== Ready ==");
}

// ─────────────────────────────────────────────────
void loop() {
  ElegantOTA.loop();

  // Delayed saving
  if (savePending && millis() - saveTimer > 2000) {
    saveData(state);
    savePending = false;
  }

  // Sample battery every 5 seconds to keep rolling average fresh
  if (millis() - lastBatSample > BAT_SAMPLE_MS) {
    lastBatSample = millis();
    readBatteryPercent();  // Updates rolling buffer
  }

  // Redraw status bar every minute
  if (state.screen == SCREEN_HOME &&
      millis() - lastBatRefresh > BAT_REFRESH_MS) {
    lastBatRefresh = millis();
    drawStatusBar(tft, state);
  }

  // RFID Card assignment pending (non-blocking)
  static bool rfidAssignShown = false;
  if (state.rfidAssignPending) {
    if (!rfidAssignShown) {
      if (state.screen == SCREEN_SCREENSAVER) {
        stopScreensaver();
        state.screen = SCREEN_HOME;
      }
      state.lastInteraction = millis();
      drawHomeScreen(tft, state);
      setBrightness(BRIGHT_FULL);
      tft.fillRect(0, 260, 240, 60, COLOR_CARD);
      tft.setTextFont(1);
      tft.setTextColor(TFT_YELLOW, COLOR_CARD);
      tft.setCursor(10, 270);
      tft.print(STR_RFID_SCAN_NOW);
      rfidAssignShown = true;
    }
    if (millis() - state.rfidAssignStartTime > 10000) {
      state.rfidAssignPending = false;
      state.rfidAssignUID = "";
      state.screen = SCREEN_HOME;
      rfidAssignShown = false;
      drawHomeScreen(tft, state);
    } else {
      yield();
      if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
          if (i > 0) uid += ":";
          if (rfid.uid.uidByte[i] < 0x10) uid += "0";
          uid += String(rfid.uid.uidByte[i], HEX);
        }
        uid.toUpperCase();
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();

        int ki = state.rfidAssignKid;
        if (ki >= 0 && ki < state.kidCount) {
          strlcpy(state.kids[ki].rfidUID, uid.c_str(), 16);
          saveData(state);
          Serial.printf("RFID assigned: %s -> %s\n", uid.c_str(), state.kids[ki].name);
        }
        state.rfidAssignUID     = uid;
        state.rfidAssignPending = false;
        state.screen = SCREEN_HOME;
        rfidAssignShown = false;
        drawHomeScreen(tft, state);
      }
    }
  } else {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = getCardUID(rfid);
      Serial.printf("Card: %s\n", uid.c_str());
      handleCardTap(uid, state, tft);
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  handleTouch(tft, state);

  // Screensaver after 2 minutes
  if (state.screen == SCREEN_HOME &&
      millis() - state.lastInteraction > SCREENSAVER_MS) {
    state.screen = SCREEN_SCREENSAVER;
  }

  // Deep Sleep after 30 minutes
  if (state.screen == SCREEN_SCREENSAVER &&
      millis() - state.lastInteraction > DEEP_SLEEP_MS) {
    for (int b = 255; b >= 0; b -= 8) { setBrightness(b); delay(8); }
    setBrightness(0);
    esp_deep_sleep_start();
  }

  // Draw screensaver frame
  if (state.screen == SCREEN_SCREENSAVER) {
    handleTouch(tft, state);
    if (state.screen == SCREEN_SCREENSAVER) {
      drawScreensaverFrame(tft);
    }
  } else {
    if (state.screen != SCREEN_HOME && state.activeKid >= 0) {
      if (millis() - state.lastInteraction > AUTO_LOGOUT_MS) {
        state.activeKid = -1;
        state.screen    = SCREEN_HOME;
        drawHomeScreen(tft, state);
      }
    }
    delay(20);
  }
}
