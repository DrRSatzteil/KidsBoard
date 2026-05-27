// =====================================================
//  strings.h – Localizable string constants
// =====================================================
#ifndef STRINGS_H
#define STRINGS_H

static const char* DAY_SHORT[] = { "Mo", "Di", "Mi", "Do", "Fr" };
static const char* DAY_LONG[]  = { "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag" };

// RFID reader
#define STR_RFID_UNKNOWN        "Unbekannte Karte:"
#define STR_RFID_ASSIGN         "Im Web-Interface zuordnen"
#define STR_RFID_SCAN_NOW       "Karte jetzt auflegen..."

// Boot screen
#define STR_APP_NAME         "KidsBoard"
#define STR_BOOT_LOADING     "wird gestartet..."
#define STR_BAT_EMPTY        "Akku leer!"
#define STR_BAT_CHARGE       "Bitte laden!"
#define STR_BAT_SHUTDOWN     "Schalte ab in 5s..."

// Planner screen
#define STR_PLANNER_WEEK   "Woche"
#define STR_PLANNER_TODAY  "Heute"

// Weekend / reward screen
#define STR_WEEKEND_TASKS        "%d von %d Aufgaben"
#define STR_WEEKEND_REWARD       "Deine Belohnung:"

// Celebration screen
#define STR_CELEBRATE_TITLE      "Super!"
#define STR_CELEBRATE_DONE       "Alles geschafft!"
#define STR_CELEBRATE_CONGRATS   "Gut gemacht, %s!"

// Home screen
#define STR_HOME_HEADER    "* FAMILIENPLANER *"
#define STR_HOME_SCAN_CARD "KARTE AUFLEGEN"
#define STR_HOME_OR_SELECT "- ODER WAEHLEN -"

#endif // STRINGS_H
