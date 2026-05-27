// =====================================================
//  retro_gfx.h – Retro GFX functions
//  All assets pre-swapped – no setSwapBytes needed!
// =====================================================
#pragma once
#include <TFT_eSPI.h>
#include "retro_assets.h"

// ── Is it night? (20:00 - 07:00) ────────────────────
bool isNightTime() {
  struct tm ti;
  if (!getLocalTime(&ti, 100)) return false;
  return (ti.tm_hour >= 20 || ti.tm_hour < 7);
}

// ── Draw background (2x scaled) ─────────────────────
void drawBackground(TFT_eSPI& tft, const uint16_t* bg) {
  uint16_t* lineBuf = (uint16_t*)malloc(240 * 2);
  if (!lineBuf) { Serial.println("drawBackground: malloc failed!"); return; }

  uint16_t rowBuf[BG_W];
  for (int y = 0; y < BG_H; y++) {
    memcpy_P(rowBuf, &bg[y * BG_W], BG_W * 2);
    for (int x = 0; x < BG_W; x++) {
      lineBuf[x*2]   = rowBuf[x];
      lineBuf[x*2+1] = rowBuf[x];
    }
    tft.pushImage(0, y*2,   240, 1, lineBuf);
    tft.pushImage(0, y*2+1, 240, 1, lineBuf);
  }
  free(lineBuf);
}

// ── Draw background depending on time of day ─────────
void drawBackgroundAuto(TFT_eSPI& tft) {
  if (isNightTime()) drawBackground(tft, BG_NIGHT);
  else               drawBackground(tft, BG_DAY);
}

// ── Get background pixel for mask operations ─────────
uint16_t getBgPixel(int x, int y) {
  int bx = x / 2;
  int by = y / 2;
  if (bx >= BG_W) bx = BG_W - 1;
  if (by >= BG_H) by = BG_H - 1;
  const uint16_t* bg = isNightTime() ? BG_NIGHT : BG_DAY;
  return pgm_read_word(&bg[by * BG_W + bx]);
}

// ── Draw avatar ──────────────────────────────────────
void drawAvatar(TFT_eSPI& tft, const uint16_t* avatar, int x, int y) {
  uint16_t buf[AVATAR_W * AVATAR_H];
  memcpy_P(buf, avatar, AVATAR_W * AVATAR_H * 2);
  tft.pushImage(x, y, AVATAR_W, AVATAR_H, buf);
}
