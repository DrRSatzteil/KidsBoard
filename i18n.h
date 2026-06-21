// =====================================================
//  i18n.h – Localizable string constants
// =====================================================
#ifndef I18N_H
#define I18N_H

static const char* DAY_SHORT[] = { "Mo", "Di", "Mi", "Do", "Fr" };
static const char* DAY_LONG[]  = { "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag" };

// RFID reader
#define STR_RFID_UNKNOWN        "Unbekannte Karte:"
#define STR_RFID_ASSIGN         "Im Web-Interface zuordnen"
#define STR_RFID_SCAN_NOW       "Karte jetzt auflegen..."

// Boot screen
#define STR_APP_NAME            "KidsBoard"
#define STR_BOOT_LOADING        "wird gestartet..."
#define STR_BAT_EMPTY           "Akku leer!"
#define STR_BAT_CHARGE          "Bitte laden!"
#define STR_BAT_SHUTDOWN        "Schalte ab in 5s..."

// Planner screen
#define STR_PLANNER_WEEK        "Woche"
#define STR_PLANNER_TODAY       "Heute"
#define STR_PLANNER_QUIZ_BADGE  "Quiz"

// Weekend / reward screen
#define STR_WEEKEND_TASKS       "%d von %d Aufgaben"
#define STR_WEEKEND_REWARD      "Deine Belohnung:"

// Celebration screen
#define STR_CELEBRATE_TITLE     "Super!"
#define STR_CELEBRATE_DONE      "Alles geschafft!"
#define STR_CELEBRATE_CONGRATS  "Gut gemacht, %s!"

// Home screen
#define STR_HOME_HEADER         "* FAMILIENPLANER *"
#define STR_HOME_SCAN_CARD      "KARTE AUFLEGEN"
#define STR_HOME_OR_SELECT      "- ODER WAEHLEN -"

// Quiz screen – Dr. Pi
#define STR_QUIZ_HEADER         "Dr. Pi fragt..."
#define STR_QUIZ_COUNTER        "%d/%d"
#define STR_QUIZ_CORRECT_ALL    "Alle %d beim 1. Versuch!"
#define STR_QUIZ_CORRECT_SOME   "%d von %d beim 1. Versuch!"
#define STR_QUIZ_TASK_DONE      "Task erledigt!"
#define STR_QUIZ_TRY_AGAIN      "Nochmal versuchen!"
#define STR_QUIZ_NEXT           "Weiter"
#define STR_QUIZ_SUPER          "Super!"
#define STR_QUIZ_ALREADY_DONE   "Bereits geschafft!"
#define STR_QUIZ_WHAT_TODO      "Was moechtest du tun?"
#define STR_QUIZ_PLAY_AGAIN     "Nochmal spielen"
#define STR_QUIZ_RESET_TASK     "Task zuruecksetzen"
#define STR_QUIZ_BACK           "Zurueck"
#define STR_QUIZ_NO_QUESTIONS_1 "Noch keine Fragen!"
#define STR_QUIZ_NO_QUESTIONS_2 "Bitte im Webinterface"
#define STR_QUIZ_NO_QUESTIONS_3 "konfigurieren."

// Quiz API prompt template
// Parameters (in order): maxQuestions, topic, max question length, max answer length
// Length limits are computed in code as (MAX_X_LEN - 8) to leave buffer for encoding
#define STR_QUIZ_PROMPT_TEMPLATE \
  "Erstelle %d Multiple-Choice-Fragen zum Thema: \"%s\".\n" \
  "Antworte NUR mit einem JSON-Array, kein Text davor oder danach, keine Markdown-Backticks.\n" \
  "Format: [{\"q\":\"Frage?\",\"a\":[\"Antwort A\",\"Antwort B\",\"Antwort C\",\"Antwort D\"],\"c\":0}]\n" \
  "Dabei ist \"c\" der 0-basierte Index der richtigen Antwort.\n" \
  "Verwende nur ASCII-Zeichen, kein Malzeichen-Symbol, schreibe stattdessen 'mal' oder 'x'.\n" \
  "Wichtig: Teste nur Wissen das eindeutig richtig oder falsch ist.\n" \
  "Keine Fragen bei denen mehrere Antworten korrekt sein koennten.\n" \
  "Erstelle möglichst plausible Optionen und vermeide vollkommen absurde Antworten.\n" \
  "Wichtig: Fragen maximal %d Zeichen, Antworten maximal %d Zeichen.\n" \
  "Genau 4 Antworten pro Frage."

#endif // I18N_H