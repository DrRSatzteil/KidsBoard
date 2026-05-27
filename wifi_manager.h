// =====================================================
//  wifi_manager.h – WiFi credentials via Preferences
//  On first boot or failed connect: opens AP for setup
// =====================================================
#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

// WiFi connection mode
enum KBWiFiMode {
  KB_WIFI_CONNECTED,  // Connected to home network
  KB_WIFI_AP,         // Running as Access Point
  KB_WIFI_NONE        // Not connected
};

KBWiFiMode wifiMode = KB_WIFI_NONE;

// Load credentials from NVS
bool loadWiFiCredentials(String& ssid, String& pass) {
  Preferences prefs;
  prefs.begin("wifi", true);  // read-only
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

// Save credentials to NVS
void saveWiFiCredentials(const String& ssid, const String& pass) {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.println("WiFi credentials saved");
}

// Clear stored credentials
void clearWiFiCredentials() {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  Serial.println("WiFi credentials cleared");
}

// Try to connect to stored WiFi, fall back to AP mode
// Returns KB_WIFI_CONNECTED or KB_WIFI_AP
KBWiFiMode startWiFi() {
  String ssid, pass;

  if (loadWiFiCredentials(ssid, pass)) {
    Serial.printf("Trying WiFi: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
      delay(500);
      Serial.print(".");
      tries++;
    }
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());
      wifiMode = KB_WIFI_CONNECTED;
      return KB_WIFI_CONNECTED;
    }
    Serial.println("\nWiFi connect failed, starting AP");
  } else {
    Serial.println("No WiFi credentials, starting AP");
  }

  // Start AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, strlen(WIFI_AP_PASS) > 0 ? WIFI_AP_PASS : nullptr);
  Serial.printf("AP started: %s, IP: %s\n", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
  wifiMode = KB_WIFI_AP;
  return KB_WIFI_AP;
}

String getWiFiStatusString() {
  if (wifiMode == KB_WIFI_CONNECTED) return WiFi.localIP().toString();
  if (wifiMode == KB_WIFI_AP) return String("AP: ") + WIFI_AP_SSID;
  return "Offline";
}

bool isWiFiConnected() { return wifiMode == KB_WIFI_CONNECTED; }
bool isAPMode()        { return wifiMode == KB_WIFI_AP; }