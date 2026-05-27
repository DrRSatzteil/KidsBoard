// =====================================================
//  display_ui.h – All Screens + Touch Handlers
// =====================================================
#pragma once
#include <TFT_eSPI.h>
#include "config.h"
#include "data.h"
#include "retro_gfx.h"

// ── Touch Calibration ─────────────────────────────
#define TOUCH_CAL_DATA { 395, 3227, 305, 3361, 4 }
uint16_t touchCalData[5] = TOUCH_CAL_DATA;

// ── Helper Functions ──────────────────────────────

void drawRoundRect(TFT_eSPI& tft, int x, int y, int w, int h,
                   int r, uint16_t fill, uint16_t border = 0) {
  tft.fillRoundRect(x, y, w, h, r, fill);
  if (border) tft.drawRoundRect(x, y, w, h, r, border);
}

void drawProgressBar(TFT_eSPI& tft, int x, int y, int w, int h,
                     int pct, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, h/2, COLOR_BORDER);
  if (pct > 0) {
    int filled = max(1, (w * pct) / 100);
    tft.fillRoundRect(x, y, filled, h, h/2, color);
  }
}

void centerText(TFT_eSPI& tft, const char* text,
                int y, int size, uint16_t color, int screenW = 240) {
  tft.setTextSize(size);
  tft.setTextColor(color, COLOR_BG);
  int tw = tft.textWidth(text);
  tft.setCursor((screenW - tw) / 2, y);
  tft.print(text);
}

// ── Back Arrow Button ─────────────────────────────

void drawBackArrow(TFT_eSPI& tft, int x, int y, uint16_t color) {
  tft.fillTriangle(x,      y + 5,
                   x + 7,  y,
                   x + 7,  y + 10,
                   color);
  tft.fillRect(x + 7, y + 3, 10, 4, color);
}

void drawBackButton(TFT_eSPI& tft, int x, int y, int w, int h, uint16_t borderColor) {
  drawRoundRect(tft, x, y, w, h, 5, COLOR_BG, borderColor);
  drawBackArrow(tft, x + (w - 17) / 2, y + (h - 10) / 2, borderColor);
}

// ── Draw Star ─────────────────────────────────────

void drawStar(TFT_eSPI& tft, int cx, int cy, int r, uint16_t color) {
  int rInner = (int)(r * 0.45f);
  int16_t px[10], py[10];
  for (int i = 0; i < 10; i++) {
    float angle = (i * 36 - 90) * 3.14159f / 180.0f;
    int radius = (i % 2 == 0) ? r : rInner;
    px[i] = cx + (int)(radius * cos(angle));
    py[i] = cy + (int)(radius * sin(angle));
  }
  for (int i = 0; i < 10; i++) {
    tft.fillTriangle(cx, cy, px[i], py[i], px[(i+1)%10], py[(i+1)%10], color);
  }
}

int countActiveKids(AppState& state) {
  int count = 0;
  for (int i = 0; i < state.kidCount; i++)
    if (state.kids[i].active) count++;
  return count;
}

// ── Boot Screen ───────────────────────────────────

void showBootScreen(TFT_eSPI& tft) {
  tft.fillScreen(COLOR_BG);
  tft.setTextFont(2);

  uint16_t calData[5] = TOUCH_CAL_DATA;
  tft.setTouch(calData);
  Serial.println("Touch calibrated");

  centerText(tft, STR_APP_NAME, 120, 2, COLOR_KID_0);
  centerText(tft, STR_BOOT_LOADING, 150, 1, COLOR_MUTED);

  // Progress bar + fade in simultaneously
  for (int i = 0; i <= 100; i += 5) {
    drawProgressBar(tft, 40, 180, 160, 6, i, COLOR_KID_1);
    extern void setBrightness(int);
    setBrightness((int)(i * 2.55f)); // 0-100 → 0-255
    delay(30);
  }
}

// ── Status Bar ────────────────────────────────────
void drawStatusBar(TFT_eSPI& tft, AppState& state) {
  tft.fillRect(0, 298, 240, 22, 0x0841);
  tft.setTextFont(1);

  // WiFi status left – tap to toggle IP/AP address
  tft.setCursor(5, 306);
  if (state.wifiOk) {
    tft.setTextColor(COLOR_SUCCESS, 0x0841);
    tft.print(state.showIP ? WiFi.localIP().toString().c_str() : "WiFi");
  } else if (state.apMode) {
    tft.setTextColor(0xFD20, 0x0841);  // Orange for AP mode
    tft.print(state.showIP ? WiFi.softAPIP().toString().c_str() : "AP: KidsBoard");
  }

  // Battery right – tap to toggle voltage display. Remove this if you don't measure the battery level
  extern int readBatteryPercent();
  int batPct = readBatteryPercent();
  uint16_t batColor;
  if (batPct > 50)      batColor = COLOR_SUCCESS;
  else if (batPct > 20) batColor = 0xFFE0;
  else                  batColor = TFT_RED;

  char batBuf[12];
  if (state.showVoltage) {
    // Show smoothed voltage for calibration
    extern float readBatteryVoltage();
    sprintf(batBuf, "%.2fV", readBatteryVoltage());
  } else {
    sprintf(batBuf, "%d%%", batPct);
  }
  tft.setTextColor(batColor, 0x0841);
  int tbw = tft.textWidth(batBuf);
  tft.setCursor(235 - tbw, 306);
  tft.print(batBuf);
}

// ── Home Screen (Retro) ───────────────────────────

// Avatar arrays per kid index
// Order must match data.h:
// kids[0]=Mila, kids[1]=Felix, kids[2]=Mum, kids[3]=Dad
const uint16_t* AVATAR_DATA[] = {
  AVATAR_MILA,
  AVATAR_FELIX,
  AVATAR_MAMA,
  AVATAR_PAPA,
};

void drawHomeScreen(TFT_eSPI& tft, AppState& state) {
  // Background (auto day/night)
  drawBackgroundAuto(tft);

  // Header: dark panel at top
  uint16_t HDR_BG  = 0x0841;
  uint16_t ACCENT  = 0xFFE0;  // yellow
  tft.fillRect(0, 0, 240, 22, HDR_BG);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT, HDR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(STR_HOME_HEADER, 120, 11);
  tft.setTextDatum(TL_DATUM);

  // RFID scan box
  uint16_t BOX_BG = 0x0020;
  int tw;
  tft.fillRoundRect(10, 28, 220, 52, 6, BOX_BG);
  tft.drawRoundRect(10, 28, 220, 52, 6, ACCENT);
  tft.setTextFont(1);
  tft.setTextColor(ACCENT, BOX_BG);
  tw = tft.textWidth(STR_HOME_SCAN_CARD);
  tft.setCursor((240 - tw) / 2, 36);
  tft.print(STR_HOME_SCAN_CARD);
  // RFID wave icon
  int cx = 120, cy = 60;
  tft.fillCircle(cx, cy, 4, ACCENT);
  tft.drawCircle(cx, cy, 9,  ACCENT);
  tft.drawCircle(cx, cy, 14, 0x8410);

  // "or select" label
  tft.setTextFont(1);
  tft.setTextColor(0x2945);  // dark gray, no background
  tft.setTextDatum(MC_DATUM);
  tft.drawString(STR_HOME_OR_SELECT, 120, 91);
  tft.setTextDatum(TL_DATUM);

  // Kid buttons with avatars
  int activeCount = countActiveKids(state);
  if (activeCount > 0) {
    int btnW = 220, btnH = 40;
    int bx = 10;
    int by_start = 102;
    int gap = 4;

    int btnIdx = 0;
    for (int i = 0; i < state.kidCount; i++) {
      if (!state.kids[i].active) continue;
      int by = by_start + btnIdx * (btnH + gap);

      // Button background
      tft.fillRoundRect(bx, by, btnW, btnH, 6, 0x0841);
      tft.drawRoundRect(bx, by, btnW, btnH, 6, state.kids[i].color);

      // Avatar on left
      if (AVATAR_DATA[i] != nullptr) {
        drawAvatar(tft, AVATAR_DATA[i], bx + 4, by + 4);
      } else {
        // Placeholder if no avatar defined
        tft.fillRoundRect(bx + 4, by + 4, 32, 32, 4, 0x2945);
        tft.setTextColor(state.kids[i].color, 0x2945);
        tft.setCursor(bx + 13, by + 13);
        tft.print("?");
      }

      // Name next to avatar
      tft.setTextFont(2);
      tft.setTextColor(state.kids[i].color, 0x0841);
      tft.setCursor(bx + 44, by + 12);
      tft.print(state.kids[i].name);

      // Arrow on right
      tft.setTextColor(state.kids[i].color, 0x0841);
      tft.setCursor(bx + btnW - 14, by + 12);
      tft.print("►");

      btnIdx++;
    }
  }

  // Status bar at bottom
  drawStatusBar(tft, state);
}

// ── Planner Screen ────────────────────────────────

void drawPlannerScreen(TFT_eSPI& tft, AppState& state) {
  if (state.activeKid < 0) return;
  Kid& kid = state.kids[state.activeKid];

  tft.fillScreen(COLOR_BG);
  tft.fillRect(0, 0, 240, 52, COLOR_CARD);

  tft.setTextFont(2);
  tft.setTextColor(kid.color, COLOR_CARD);
  tft.setCursor(10, 6);
  tft.print(kid.name);

  int total, done = getWeekScore(kid, total);
  int pct = total > 0 ? (done * 100) / total : 0;
  tft.setTextFont(1);
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(10, 24);
  tft.printf("%d/%d  %d%%", done, total, pct);
  drawProgressBar(tft, 10, 36, 130, 6, pct, kid.color);

  drawRoundRect(tft, 148, 10, 44, 24, 5, COLOR_BG, kid.color);
  tft.setTextFont(1);
  tft.setTextColor(kid.color, COLOR_BG);
  int tw = tft.textWidth(STR_PLANNER_WEEK);
  tft.setCursor(148 + (44 - tw) / 2, 19);
  tft.print(STR_PLANNER_WEEK);

  drawBackButton(tft, 197, 10, 36, 24, COLOR_MUTED);

  // Day tabs
  int tabW = 240 / DAYS_COUNT;
  for (int d = 0; d < DAYS_COUNT; d++) {
    bool isActive = (d == state.activeDay);
    uint16_t tabBg = isActive ? COLOR_CARD : COLOR_BG;
    tft.fillRect(d * tabW, 52, tabW, 22, tabBg);
    if (isActive) tft.fillRect(d * tabW, 72, tabW, 2, kid.color);

    tft.setTextFont(1);
    tft.setTextColor(isActive ? kid.color : COLOR_MUTED, tabBg);
    tw = tft.textWidth(DAY_SHORT[d]);
    tft.setCursor(d * tabW + (tabW - tw) / 2, 59);
    tft.print(DAY_SHORT[d]);

    // Green dot if all tasks done for this day
    DayPlan& dp = kid.week[d];
    int dayDone = 0;
    for (int t = 0; t < dp.taskCount; t++) if (dp.tasks[t].done) dayDone++;
    if (dayDone == dp.taskCount && dp.taskCount > 0)
      tft.fillCircle(d * tabW + tabW - 5, 57, 3, COLOR_SUCCESS);
  }

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.setCursor(10, 80);
  tft.print(DAY_LONG[state.activeDay]);

  // Task list
  DayPlan& dp = kid.week[state.activeDay];
  int yStart = 102, rowH = 34;

  for (int t = 0; t < dp.taskCount; t++) {
    bool isDone = dp.tasks[t].done;
    int  ty     = yStart + t * rowH;

    uint16_t rowBg = isDone
      ? (uint16_t)((kid.color & 0xF7DE) >> 1)
      : COLOR_CARD;
    drawRoundRect(tft, 10, ty, 220, rowH - 4, 6, rowBg,
                  isDone ? kid.color : COLOR_BORDER);

    int cbx = 22, cby = ty + 7;
    if (isDone) {
      tft.fillRoundRect(cbx, cby, 18, 18, 4, kid.color);
      tft.setTextFont(1);
      tft.setTextColor(TFT_WHITE, kid.color);
      tft.setCursor(cbx + 3, cby + 2);
      tft.print("OK");
    } else {
      tft.drawRoundRect(cbx, cby, 18, 18, 4, COLOR_MUTED);
    }

    tft.setTextFont(2);
    // Done tasks: dark text on light kid-color background = always good contrast
    tft.setTextColor(isDone ? 0x2945 : TFT_WHITE, rowBg);
    tft.setCursor(48, ty + 9);
    tft.print(dp.tasks[t].name);
  }

  // Daily progress bar at bottom
  int dayDone = 0;
  for (int t = 0; t < dp.taskCount; t++) if (dp.tasks[t].done) dayDone++;
  int dayPct = dp.taskCount > 0 ? (dayDone * 100) / dp.taskCount : 0;

  int barY = 294;
  drawRoundRect(tft, 10, barY, 220, 22, 6, COLOR_CARD, COLOR_BORDER);
  tft.setTextFont(1);
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(16, barY + 7);
  tft.printf("%s: %d/%d", STR_PLANNER_TODAY, dayDone, dp.taskCount);
  drawProgressBar(tft, 110, barY + 7, 110, 8, dayPct, kid.color);
}

// ── Weekend / Reward Screen ───────────────────────

void drawWeekendScreen(TFT_eSPI& tft, AppState& state) {
  if (state.activeKid < 0) return;
  Kid& kid = state.kids[state.activeKid];

  tft.fillScreen(COLOR_BG);
  drawBackButton(tft, 197, 8, 36, 24, COLOR_MUTED);

  tft.setTextFont(4);
  tft.setTextColor(kid.color, COLOR_BG);
  int tw = tft.textWidth(kid.name);
  tft.setCursor((240 - tw) / 2, 8);
  tft.print(kid.name);

  int total, done = getWeekScore(kid, total);
  int pct   = total > 0 ? (done * 100) / total : 0;
  int stars = getStars(done, total);

  // Decorative circles
  for (int i = 0; i < 3; i++)
    tft.drawCircle(120, 88, 38 + i * 12, (uint16_t)(kid.color & 0xF7DE) >> 1);

  // Large percentage display
  char scoreBuf[8];
  // sprintf(scoreBuf, "%d%%", pct); <- The font does not draw a % sign
  sprintf(scoreBuf, "%d", pct);
  tft.setTextFont(7);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tw = tft.textWidth(scoreBuf);
  tft.setCursor(120 - tw / 2, 52);
  tft.print(scoreBuf);

  tft.setTextFont(1);
  tft.setTextColor(COLOR_MUTED, COLOR_BG);
  char subBuf[24];
  sprintf(subBuf, STR_WEEKEND_TASKS, done, total);
  tw = tft.textWidth(subBuf);
  tft.setCursor((240 - tw) / 2, 116);
  tft.print(subBuf);

  // Star rating
  int starR = 11, starSpacing = 36, starY = 138;
  int starStartX = 120 - (2 * starSpacing);
  for (int i = 0; i < 5; i++) {
    uint16_t sc = (i < stars) ? 0xFFE0 : COLOR_BORDER;
    drawStar(tft, starStartX + i * starSpacing, starY, starR, sc);
  }

  // Reward box
  int boxY = 166, rowH = 24;
  int boxH = 18 + kid.rewardCount * rowH + 8;
  drawRoundRect(tft, 15, boxY, 210, boxH, 10, COLOR_CARD, kid.color);

  tft.setTextFont(1);
  tft.setTextColor(kid.color, COLOR_CARD);
  tw = tft.textWidth(STR_WEEKEND_REWARD);
  tft.setCursor(15 + (210 - tw) / 2, boxY + 5);
  tft.print(STR_WEEKEND_REWARD);

  for (int r = 0; r < kid.rewardCount; r++) {
    int ry = boxY + 18 + r * rowH;
    tft.setTextFont(2);

    bool isTxt = (strcmp(kid.rewards[r].type, "txt") == 0);
    bool isMys = (strcmp(kid.rewards[r].type, "mys") == 0);

    if (isMys || isTxt) {
      bool unlocked = (stars >= 5);
      tft.setTextColor(unlocked ? kid.color : COLOR_MUTED, COLOR_CARD);
      tft.setCursor(25, ry);
      if (isMys && !unlocked) tft.print("???");
      else                    tft.print(kid.rewards[r].name);
      tft.setTextColor(unlocked ? COLOR_SUCCESS : 0xF800, COLOR_CARD);
      int tbw = tft.textWidth(unlocked ? "OK" : "X");
      tft.setCursor(215 - tbw, ry);
      tft.print(unlocked ? "OK" : "X");
    } else {
      char valBuf[16];
      formatReward(kid.rewards[r], stars, valBuf, sizeof(valBuf));
      bool hasValue = (strcmp(valBuf, "-") != 0);
      tft.setTextColor(hasValue ? kid.color : COLOR_MUTED, COLOR_CARD);
      tft.setCursor(25, ry);
      tft.print(kid.rewards[r].name);
      if (!hasValue) {
        tft.setTextColor(0xF800, COLOR_CARD);
        tft.setCursor(210, ry);
        tft.print("X");
      } else {
        tft.setTextColor(kid.color, COLOR_CARD);
        int tbw = tft.textWidth(valBuf);
        tft.setCursor(215 - tbw, ry);
        tft.print(valBuf);
      }
    }

    if (r < kid.rewardCount - 1)
      tft.drawLine(25, ry + 20, 215, ry + 20, COLOR_BORDER);
  }
}

// ── Confetti Celebration Animation ───────────────

struct Particle {
  int16_t x, y;
  int8_t  vx, vy;
  uint8_t size;
  uint16_t color;
  bool active;
};

#define NUM_PARTICLES 35

void showCelebration(TFT_eSPI& tft, uint16_t kidColor, const char* kidName) {
  uint16_t confettiColors[] = {
    TFT_RED, TFT_GREEN, TFT_BLUE,
    TFT_YELLOW, TFT_CYAN, TFT_MAGENTA,
    0xFD20, 0xF81F
  };

  Particle particles[NUM_PARTICLES];
  for (int i = 0; i < NUM_PARTICLES; i++) {
    particles[i].x      = random(20, 220);
    particles[i].y      = random(-80, -4);
    particles[i].vx     = random(-2, 3);
    particles[i].vy     = random(2, 6);
    particles[i].size   = random(2, 5);
    particles[i].color  = confettiColors[random(8)];
    particles[i].active = true;
  }

  tft.fillScreen(COLOR_BG);

  // Modal overlay
  drawRoundRect(tft, 25, 108, 190, 104, 14, COLOR_BG, kidColor);
  tft.setTextFont(4);
  tft.setTextColor(kidColor, COLOR_BG);
  int tw = tft.textWidth(STR_CELEBRATE_TITLE);
  tft.setCursor(25 + (190 - tw) / 2, 118);
  tft.print(STR_CELEBRATE_TITLE);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  char buf[32];
  sprintf(buf, STR_CELEBRATE_DONE);
  tw = tft.textWidth(buf);
  tft.setCursor(25 + (190 - tw) / 2, 158);
  tft.print(buf);

  sprintf(buf, STR_CELEBRATE_CONGRATS, kidName);
  tw = tft.textWidth(buf);
  tft.setTextColor(kidColor, COLOR_BG);
  tft.setCursor(25 + (190 - tw) / 2, 180);
  tft.print(buf);

  // Confetti animation loop
  unsigned long start = millis();
  while (millis() - start < 3000) {
    for (int i = 0; i < NUM_PARTICLES; i++) {
      if (!particles[i].active) continue;

      int16_t oldX = particles[i].x;
      int16_t oldY = particles[i].y;

      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;

      if (particles[i].y > 325) {
        particles[i].y = random(-40, -4);
        particles[i].x = random(20, 220);
        particles[i].vx = random(-2, 3);
        particles[i].vy = random(2, 5);
        continue;
      }

      if (particles[i].x < 2 || particles[i].x > 238)
        particles[i].vx = -particles[i].vx;

      // Don't draw over the modal
      int r = particles[i].size + 2;
      bool oldOverModal = (oldX > 25 - r && oldX < 215 + r &&
                           oldY > 108 - r && oldY < 212 + r);
      bool newOverModal = (particles[i].x > 25 - r && particles[i].x < 215 + r &&
                           particles[i].y > 108 - r && particles[i].y < 212 + r);

      if (!oldOverModal) tft.fillCircle(oldX, oldY, particles[i].size, COLOR_BG);
      if (!newOverModal) tft.fillCircle(particles[i].x, particles[i].y,
                                        particles[i].size, particles[i].color);
    }
    delay(33);
  }
}

// ── Screensaver – Cloud Animation ────────────────
#include "sky_mask.h"

// Returns true if pixel (x,y) is sky according to mask
bool isSky(int x, int y) {
  if (y < SKY_MASK_Y_OFFSET) return true;
  if (y >= SKY_MASK_Y_OFFSET + SKY_MASK_HEIGHT) return false;
  int row = y - SKY_MASK_Y_OFFSET;
  uint8_t b = pgm_read_byte(&SKY_MASK[row * SKY_MASK_WIDTH + x / 8]);
  return (b & (1 << (7 - x % 8))) != 0;
}

struct SSCloud {
  float    x;
  float    speed;
  int      y;
  uint8_t  scale;        // Scale factor applied to cloud width
  uint16_t buf[CLOUD_W * CLOUD_H];  // Cloud pixels in RAM
};

#define SS_NUM_CLOUDS 3
SSCloud ssClouds[SS_NUM_CLOUDS];
bool ssInitialized = false;

void updateAllClouds(TFT_eSPI& tft, float oldX[SS_NUM_CLOUDS]);  // forward declaration

void initScreensaver(TFT_eSPI& tft) {
  extern void setBrightness(int);
  for (int b = 255; b >= 0; b -= 8) { setBrightness(b); delay(8); }

  drawBackgroundAuto(tft);

  uint16_t cloudBuf[CLOUD_W * CLOUD_H];
  memcpy_P(cloudBuf, SPRITE_CLOUD, CLOUD_W * CLOUD_H * 2);

  // Three clouds at different heights, no overlap:
  // Cloud 0: high in sky  (y=8,   no masking needed)
  // Cloud 1: mid height   (y=65,  passes behind tree via mask)
  // Cloud 2: lower        (y=120, passes behind tree via mask)
  int yPositions[3] = { 8, 65, 120 };
  uint8_t scales[3] = { 2, 1, 2  };   // large, small, medium
  float speeds[3]   = { 1.2f, 1.8f, 0.9f };

  for (int i = 0; i < SS_NUM_CLOUDS; i++) {
    ssClouds[i].y     = yPositions[i];
    ssClouds[i].scale = scales[i];
    ssClouds[i].speed = speeds[i];
    memcpy(ssClouds[i].buf, cloudBuf, CLOUD_W * CLOUD_H * 2);
  }

  // All clouds start off-screen left and fly in
  for (int i = 0; i < SS_NUM_CLOUDS; i++) {
    ssClouds[i].x = -(float)(CLOUD_W * ssClouds[i].scale);
  }

  ssInitialized = true;
  for (int b = 0; b <= 255; b += 8) { setBrightness(b); delay(8); }
}

// Redraw only the X-strip affected by each cloud movement
// Much faster than redrawing all rows – prevents lag and keeps touch responsive
void updateAllClouds(TFT_eSPI& tft, float oldX[SS_NUM_CLOUDS]) {
  uint16_t lineBuf[240];

  for (int i = 0; i < SS_NUM_CLOUDS; i++) {
    int nx = (int)ssClouds[i].x;
    int ox = (int)oldX[i];
    int w  = CLOUD_W * ssClouds[i].scale;

    // X range: union of old and new position
    int minX = max(0, min(ox, nx));
    int maxX = min(240, max(ox + w, nx + w));
    if (minX >= maxX) continue;

    for (int sy = ssClouds[i].y; sy < ssClouds[i].y + CLOUD_H; sy++) {
      if (sy < 0 || sy >= 320) continue;

      // Fill strip with background
      for (int x = minX; x < maxX; x++) {
        lineBuf[x - minX] = getBgPixel(x, sy);
      }

      // Blend new cloud position over background
      int cy = sy - ssClouds[i].y;
      for (int cx = 0; cx < w; cx++) {
        int sx = nx + cx;
        if (sx < minX || sx >= maxX || sx < 0 || sx >= 240) continue;
        if (!isSky(sx, sy)) continue;  // mask: skip tree pixels
        int srcX = cx / ssClouds[i].scale;
        if (srcX >= CLOUD_W) srcX = CLOUD_W - 1;
        uint16_t px = ssClouds[i].buf[cy * CLOUD_W + srcX];
        // Skip sky-colored pixels (transparent)
        int diff = abs((int)(px >> 11)         - (int)(CLOUD_SKY >> 11))
                 + abs((int)((px >> 5) & 0x3F) - (int)((CLOUD_SKY >> 5) & 0x3F))
                 + abs((int)(px & 0x1F)        - (int)(CLOUD_SKY & 0x1F));
        if (diff > 8) lineBuf[sx - minX] = px;
      }

      tft.pushImage(minX, sy, maxX - minX, 1, lineBuf);
    }
  }
}

// ── Stars – Night Screensaver ─────────────────────
#define SS_NUM_STARS 25
struct SSStar {
  uint8_t x, y;
  uint8_t brightness;  // 0-255
  int8_t  direction;   // +1 or -1 (fade in/out)
  uint8_t speed;       // fade speed
};
SSStar ssStars[SS_NUM_STARS];
bool ssStarsInitialized = false;

void initStars() {
  for (int i = 0; i < SS_NUM_STARS; i++) {
    // Place stars only on sky pixels (respects tree mask)
    int attempts = 0;
    do {
      ssStars[i].x = random(5, 235);
      ssStars[i].y = random(5, 220);
      attempts++;
    } while (!isSky(ssStars[i].x, ssStars[i].y) && attempts < 20);
    ssStars[i].brightness = random(0, 255);
    ssStars[i].direction  = random(2) ? 1 : -1;
    ssStars[i].speed      = random(3, 12);
  }
  ssStarsInitialized = true;
}

void drawStarFrame(TFT_eSPI& tft) {
  if (!ssStarsInitialized) initStars();
  for (int i = 0; i < SS_NUM_STARS; i++) {
    if (!isSky(ssStars[i].x, ssStars[i].y)) continue;

    // Erase old star with background pixel
    uint16_t nightBg = getBgPixel(ssStars[i].x, ssStars[i].y);
    tft.drawPixel(ssStars[i].x, ssStars[i].y, nightBg);
    // Also erase cross arms
    if (ssStars[i].x > 0)   tft.drawPixel(ssStars[i].x-1, ssStars[i].y,   nightBg);
    if (ssStars[i].x < 239) tft.drawPixel(ssStars[i].x+1, ssStars[i].y,   nightBg);
    if (ssStars[i].y > 0)   tft.drawPixel(ssStars[i].x,   ssStars[i].y-1, nightBg);
    if (ssStars[i].y < 319) tft.drawPixel(ssStars[i].x,   ssStars[i].y+1, nightBg);

    // Update brightness
    int bv = ssStars[i].brightness + ssStars[i].direction * ssStars[i].speed;
    if (bv >= 255) { bv = 255; ssStars[i].direction = -1; }
    if (bv <= 0)   { bv = 0;   ssStars[i].direction =  1; }
    ssStars[i].brightness = (uint8_t)bv;

    // Star color: white-blue tint
    uint8_t v5 = ssStars[i].brightness >> 3;
    uint8_t v6 = ssStars[i].brightness >> 2;
    uint16_t starColor = ((uint16_t)v5 << 11) | ((uint16_t)v6 << 5) | v5;
    tft.drawPixel(ssStars[i].x, ssStars[i].y, starColor);

    // Bright stars get a small cross
    if (ssStars[i].brightness > 180) {
      if (ssStars[i].x > 0)   tft.drawPixel(ssStars[i].x-1, ssStars[i].y,   starColor);
      if (ssStars[i].x < 239) tft.drawPixel(ssStars[i].x+1, ssStars[i].y,   starColor);
      if (ssStars[i].y > 0)   tft.drawPixel(ssStars[i].x,   ssStars[i].y-1, starColor);
      if (ssStars[i].y < 319) tft.drawPixel(ssStars[i].x,   ssStars[i].y+1, starColor);
    }
  }
}

void drawScreensaverFrame(TFT_eSPI& tft) {
  // Re-initialize if day/night mode changed
  static bool lastNight = false;
  bool night = isNightTime();
  if (ssInitialized && night != lastNight) {
    ssInitialized = false;
    ssStarsInitialized = false;
    lastNight = night;
  }

  if (!ssInitialized) {
    lastNight = night;
    initScreensaver(tft);
    return;
  }

  if (night) {
    // Night mode: twinkling stars, no clouds
    drawStarFrame(tft);
    delay(40);
  } else {
    // Day mode: moving clouds
    float oldX[SS_NUM_CLOUDS];
    for (int i = 0; i < SS_NUM_CLOUDS; i++) oldX[i] = ssClouds[i].x;
    for (int i = 0; i < SS_NUM_CLOUDS; i++) {
      ssClouds[i].x += ssClouds[i].speed;
      if (ssClouds[i].x > 240) ssClouds[i].x = -(float)(CLOUD_W * ssClouds[i].scale);
    }
    updateAllClouds(tft, oldX);
  }
}

void stopScreensaver() {
  extern void setBrightness(int);
  // Fade out – home screen is drawn while display is dark
  for (int b = 255; b >= 0; b -= 8) { setBrightness(b); delay(8); }
  ssInitialized = false;
  // Fade in happens AFTER drawHomeScreen in the touch handler
}

// ── Touch Handler ─────────────────────────────────

void handleTouch(TFT_eSPI& tft, AppState& state) {
  if (state.rfidAssignPending) return;

  uint16_t tx, ty;
  if (!tft.getTouch(&tx, &ty, 350)) return;

  state.lastInteraction = millis();

  if (state.screen == SCREEN_SCREENSAVER) {
    stopScreensaver();
    state.screen = SCREEN_HOME;
    drawHomeScreen(tft, state);
    extern void setBrightness(int);
    for (int b = 0; b <= 255; b += 8) { setBrightness(b); delay(8); }
    return;
  }

  if (state.screen == SCREEN_PLANNER && state.activeKid >= 0) {
    Kid& kid = state.kids[state.activeKid];

    // Week button
    if (tx > 148 && tx < 192 && ty > 10 && ty < 34) {
      state.screen = SCREEN_WEEKEND;
      drawWeekendScreen(tft, state);
      return;
    }

    // Back button
    if (tx > 197 && tx < 233 && ty > 10 && ty < 34) {
      state.activeKid = -1;
      state.screen = SCREEN_HOME;
      drawHomeScreen(tft, state);
      return;
    }

    // Day tabs
    if (ty > 52 && ty < 74) {
      int tabW = 240 / DAYS_COUNT;
      int newDay = tx / tabW;
      if (newDay != state.activeDay && newDay < DAYS_COUNT) {
        state.activeDay = newDay;
        drawPlannerScreen(tft, state);
        return;
      }
    }

    // Task checkboxes
    DayPlan& dp = kid.week[state.activeDay];
    int yStart = 102, rowH = 34;
    for (int t = 0; t < dp.taskCount; t++) {
      int ty0 = yStart + t * rowH;
      if (tx > 10 && tx < 230 && (int)ty > ty0 && (int)ty < ty0 + rowH - 4) {
        dp.tasks[t].done = !dp.tasks[t].done;
        extern bool savePending;
        extern unsigned long saveTimer;
        savePending = true;
        saveTimer = millis();

        int dayDone = 0;
        for (int i = 0; i < dp.taskCount; i++) if (dp.tasks[i].done) dayDone++;
        if (dayDone == dp.taskCount && dp.taskCount > 0) {
          drawPlannerScreen(tft, state);
          showCelebration(tft, kid.color, kid.name);
        }
        drawPlannerScreen(tft, state);
        return;
      }
    }
  }

  if (state.screen == SCREEN_WEEKEND) {
    if (tx > 197 && tx < 233 && ty > 8 && ty < 32) {
      state.screen = SCREEN_PLANNER;
      drawPlannerScreen(tft, state);
      return;
    }
  }

  if (state.screen == SCREEN_HOME) {
    // WiFi tap (left) – toggle IP display
    if ((int)tx < 80 && (int)ty > 295) {
      state.showIP = !state.showIP;
      drawStatusBar(tft, state);
      return;
    }

    // Battery tap (right) – toggle voltage display
    if ((int)tx > 160 && (int)ty > 295) {
      state.showVoltage = !state.showVoltage;
      drawStatusBar(tft, state);
      return;
    }

    int activeCount = countActiveKids(state);
    if (activeCount == 0) return;

    // Vertical button layout: btnW=220, btnH=40, start y=102, gap=4
    int btnW = 220, btnH = 40;
    int bx = 10;
    int by_start = 102, gap = 4;

    int btnIdx = 0;
    for (int i = 0; i < state.kidCount; i++) {
      if (!state.kids[i].active) continue;
      int by = by_start + btnIdx * (btnH + gap);

      if ((int)tx > bx && (int)tx < bx + btnW &&
          (int)ty > by && (int)ty < by + btnH) {
        state.activeKid = i;
        state.activeDay = getTodayIndex();
        state.screen = SCREEN_PLANNER;
        state.lastInteraction = millis();
        drawPlannerScreen(tft, state);
        return;
      }
      btnIdx++;
    }
  }
}