// =====================================================
//  rfid_reader.h – Read RFID card + assignment
// =====================================================
#pragma once
#include <MFRC522.h>
#include "config.h"
#include "data.h"
#include "display_ui.h"

// ── Format UID as string ────────────────────

String getCardUID(MFRC522& rfid) {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (i > 0) uid += ":";
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ── Assign card to child ─────────────────────

int findKidByUID(const String& uid, AppState& state) {
  for (int i = 0; i < state.kidCount; i++) {
    if (uid.equals(state.kids[i].rfidUID)) return i;
  }
  return -1;
}

// ── Handle card tap ────────────────────

void handleCardTap(const String& uid, AppState& state, TFT_eSPI& tft) {
  int kidIdx = findKidByUID(uid, state);

  if (kidIdx < 0) {
    // Unknown card
    Serial.printf("Unknown card: %s\n", uid.c_str());

    // Helpful: Show UID on display for setup
    tft.fillRect(0, 280, 240, 40, COLOR_CARD);
    tft.setTextFont(1);
    tft.setTextColor(TFT_YELLOW, COLOR_CARD);
    tft.setCursor(5, 285);
    tft.print(STR_RFID_UNKNOWN);
    tft.setCursor(5, 298);
    tft.print(uid.c_str());
    tft.setCursor(5, 311);
    tft.setTextColor(COLOR_MUTED, COLOR_CARD);
    tft.print(STR_RFID_ASSIGN);
    return;
  }

  // Same card again → logout
  if (state.activeKid == kidIdx && state.screen != SCREEN_HOME) {
    state.activeKid = -1;
    state.screen = SCREEN_HOME;
    drawHomeScreen(tft, state);
    Serial.printf("%s logged out\n", state.kids[kidIdx].name);
    return;
  }

  // Log-in child
  state.activeKid  = kidIdx;
  state.activeDay  = getTodayIndex();
  state.screen     = SCREEN_PLANNER;
  state.lastInteraction = millis();

  Serial.printf("%s logged in (Day: %s)\n",
    state.kids[kidIdx].name,
    DAY_LONG[state.activeDay]);

  drawPlannerScreen(tft, state);
}

