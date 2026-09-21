// =====================================================
// data.h – Data structures + SPIFFS storage
// =====================================================
#pragma once

#include "config.h"
#include "i18n.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <set>

using namespace fs;

// ── Quiz structures ───────────────────────────────

#define MAX_QUIZ_QUESTIONS 20      // stored on SPIFFS
#define QUIZ_QUESTIONS_PER_ROUND 5 // shown per session
#define MAX_ANSWER_LEN 64
#define MAX_QUESTION_LEN 96
#define MAX_QUIZ_TOPIC_LEN 96

struct QuizAnswer {
  char text[MAX_ANSWER_LEN];
};

struct QuizQuestion {
  char question[MAX_QUESTION_LEN];
  QuizAnswer answers[4];
  uint8_t correctIndex; // 0-3
};

// ── Data structures ───────────────────────────────

struct Task {
  char name[32];
  bool done;
  char quizTopic[MAX_QUIZ_TOPIC_LEN];
  uint32_t id;  // unique task ID, never reused
};

struct DayPlan {
  Task tasks[MAX_TASKS_PER_DAY];
  int taskCount;
};

struct Reward {
  char name[32]; // e.g. "iPad", "Sweets", "Cinema"
  char type[8];  // "min", "pcs", "eur", "txt"
  int maxValue; // Maximum value for 5 stars (minutes, pieces, cents, 0 for txt)
};

struct Kid {
  char name[24];
  char rfidUID[16];
  uint16_t color;
  DayPlan week[DAYS_COUNT];
  bool active;
  Reward rewards[3]; // up to 3 rewards
  int rewardCount;   // 0-3
};

struct AppState {
  Kid kids[MAX_KIDS];
  int kidCount = 0;
  int activeKid = -1;
  int activeDay = 0;
  Screen screen = SCREEN_HOME;
  bool wifiOk = false;
  bool apMode = false;
  unsigned long lastInteraction = 0;
  bool showIP = false;
  bool showVoltage = false;
  uint32_t nextTaskId = 0;

  // RFID Assignment (non-blocking)
  bool rfidAssignPending = false;
  int rfidAssignKid = -1;
  String rfidAssignUID = "";
  unsigned long rfidAssignStartTime = 0;

  // Quiz state
  uint32_t quizTaskId = 0;
  int quizKid = -1;
  int quizDay = -1;
  int quizTask = -1;
  QuizQuestion quizQuestions[QUIZ_QUESTIONS_PER_ROUND];
  int quizQuestionCount = 0;
  int quizCurrentQuestion = 0;
  int quizCorrectCount = 0;
  int quizSelectedAnswer = -1; // -1 = no selection yet
  bool quizShowResult = false;
  bool quizFirstTry = true;
};

// ── Helper functions ───────────────────────────────

extern int cachedDayIndex;

int getTodayIndex() {
  if (cachedDayIndex >= 0)
    return cachedDayIndex;
  return 0;
}

int getWeekScore(const Kid &kid, int &total) {
  int done = 0;
  total = 0;
  for (int d = 0; d < DAYS_COUNT; d++) {
    for (int t = 0; t < kid.week[d].taskCount; t++) {
      total++;
      if (kid.week[d].tasks[t].done)
        done++;
    }
  }
  return done;
}

int getStars(int done, int total) {
  if (total == 0)
    return 0;
  int pct = (done * 100) / total;
  if (pct >= 100)
    return 5;
  if (pct >= 80)
    return 4;
  if (pct >= 60)
    return 3;
  if (pct >= 40)
    return 2;
  if (pct >= 20)
    return 1;
  return 0;
}

// Calculates value based on stars (linear scaled)
int getRewardValue(int maxValue, int stars) {
  if (stars <= 1)
    return 0;
  float pct = (stars - 1) / 4.0f; // 2 stars = 25%, 5 stars = 100%
  return (int)(maxValue * pct);
}

// Formats reward value as string
void formatReward(const Reward &r, int stars, char *buf, int bufSize) {
  if (strcmp(r.type, "mys") == 0) {
    if (stars >= 5)
      strlcpy(buf, r.name, bufSize);
    else
      strlcpy(buf, "???", bufSize);
    return;
  }
  if (strcmp(r.type, "txt") == 0) {
    if (stars >= 5)
      strlcpy(buf, r.name, bufSize);
    else
      strlcpy(buf, "-", bufSize);
    return;
  }
  int val = getRewardValue(r.maxValue, stars);
  if (val <= 0) {
    strlcpy(buf, "-", bufSize);
    return;
  }
  if (strcmp(r.type, "min") == 0) {
    if (val >= 60 && val % 60 == 0)
      snprintf(buf, bufSize, "%dh", val / 60);
    else if (val >= 60)
      snprintf(buf, bufSize, "%dh%d", val / 60, val % 60);
    else
      snprintf(buf, bufSize, "%dmin", val);
  } else if (strcmp(r.type, "pcs") == 0) {
    snprintf(buf, bufSize, "%d Stk", val);
  } else if (strcmp(r.type, "eur") == 0) {
    if (val % 100 == 0)
      snprintf(buf, bufSize, "%d EUR", val / 100);
    else
      snprintf(buf, bufSize, "%.2f EUR", val / 100.0f);
  } else {
    snprintf(buf, bufSize, "%d", val);
  }
}

// ── Quiz helpers ──────────────────────────────────

// Returns true if a task has a quiz topic configured
bool taskHasQuiz(const Task &task) { return task.quizTopic[0] != '\0'; }

// Builds SPIFFS path for quiz questions: /q_<kidIdx>_<day>_<taskIdx>.json
void getQuizPath(uint32_t taskId, char *buf, int bufSize) {
  snprintf(buf, bufSize, "/q_%u.json", taskId);
}

// Saves quiz questions to SPIFFS (called from webserver after generation)
bool saveQuizQuestions(uint32_t taskId,
                       QuizQuestion *questions, int count) {
  char path[32];
  getQuizPath(taskId, path, sizeof(path));

  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("questions");

  for (int i = 0; i < count && i < MAX_QUIZ_QUESTIONS; i++) {
    JsonObject q = arr.createNestedObject();
    q["q"] = questions[i].question;
    q["c"] = questions[i].correctIndex;
    JsonArray ans = q.createNestedArray("a");
    for (int j = 0; j < 4; j++) {
      ans.add(questions[i].answers[j].text);
    }
  }

  File f = SPIFFS.open(path, "w");
  if (!f)
    return false;
  serializeJson(doc, f);
  f.close();
  Serial.printf("Quiz saved: %s (%d questions)\n", path, count);
  return true;
}

void cleanupOrphanedQuizFiles(AppState &state) {
  // Collect all valid task IDs
  std::set<uint32_t> validIds;
  for (int ki = 0; ki < state.kidCount; ki++)
    for (int di = 0; di < DAYS_COUNT; di++)
      for (int ti = 0; ti < state.kids[ki].week[di].taskCount; ti++)
        if (state.kids[ki].week[di].tasks[ti].id > 0)
          validIds.insert(state.kids[ki].week[di].tasks[ti].id);

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    if (!name.startsWith("/")) name = "/" + name;
    file.close();
    if (name.startsWith("/q_") && name.endsWith(".json")) {
      uint32_t id = 0;
      sscanf(name.c_str(), "/q_%u.json", &id);
      if (id == 0 || validIds.find(id) == validIds.end()) {
        SPIFFS.remove(name);
        Serial.printf("Cleanup: removed %s\n", name.c_str());
      }
    }
    file = root.openNextFile();
  }
}

void replaceUmlauts(char *str, int maxLen) {
  String s = String(str);
  s.replace("ä", "ae");
  s.replace("Ä", "Ae");
  s.replace("ö", "oe");
  s.replace("Ö", "Oe");
  s.replace("ü", "ue");
  s.replace("Ü", "Ue");
  s.replace("ß", "ss");
  strlcpy(str, s.c_str(), maxLen);
}

// Loads quiz questions from SPIFFS into provided array.
// Returns number of questions loaded, 0 if none exist.
int loadQuizQuestions(uint32_t taskId, QuizQuestion *questions, int maxCount) {
  char path[32];
  getQuizPath(taskId, path, sizeof(path));

  if (!SPIFFS.exists(path))
    return 0;

  File f = SPIFFS.open(path, "r");
  if (!f)
    return 0;

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err)
    return 0;

  JsonArray arr = doc["questions"];
  int count = 0;
  for (JsonObject q : arr) {
    if (count >= maxCount)
      break;
    strlcpy(questions[count].question, q["q"] | "", MAX_QUESTION_LEN);
    replaceUmlauts(questions[count].question, MAX_QUESTION_LEN);
    questions[count].correctIndex = q["c"] | 0;
    JsonArray ans = q["a"];
    for (int j = 0; j < 4 && j < (int)ans.size(); j++) {
      strlcpy(questions[count].answers[j].text, ans[j] | "", MAX_ANSWER_LEN);
      replaceUmlauts(questions[count].answers[j].text, MAX_ANSWER_LEN);
    }
    count++;
  }
  return count;
}

// Deletes quiz questions from SPIFFS (e.g. when topic changes)
void deleteQuizQuestions(uint32_t taskId) {
  char path[32];
  getQuizPath(taskId, path, sizeof(path));
  if (SPIFFS.exists(path))
    SPIFFS.remove(path);
}

int pickRandomQuizQuestions(uint32_t taskId, QuizQuestion *out) {
  QuizQuestion *bank = new QuizQuestion[MAX_QUIZ_QUESTIONS];
  if (!bank) return 0;
  int total = loadQuizQuestions(taskId, bank, MAX_QUIZ_QUESTIONS);
  if (total == 0) {
    delete[] bank;
    return 0;
  }

  int count = min(total, QUIZ_QUESTIONS_PER_ROUND);

  // Fisher-Yates shuffle auf Fragen-Indices
  int indices[MAX_QUIZ_QUESTIONS];
  for (int i = 0; i < total; i++)
    indices[i] = i;
  for (int i = total - 1; i > 0; i--) {
    int j = random(i + 1);
    int tmp = indices[i];
    indices[i] = indices[j];
    indices[j] = tmp;
  }

  for (int i = 0; i < count; i++) {
    QuizQuestion &src = bank[indices[i]];

    // Shuffle answers (Fisher-Yates)
    int ansIdx[4] = {0, 1, 2, 3};
    for (int a = 3; a > 0; a--) {
      int j = random(a + 1);
      int tmp = ansIdx[a];
      ansIdx[a] = ansIdx[j];
      ansIdx[j] = tmp;
    }

    // Reorder answers + track correctIndex
    for (int a = 0; a < 4; a++) {
      strlcpy(out[i].answers[a].text, src.answers[ansIdx[a]].text,
              MAX_ANSWER_LEN);
      if (ansIdx[a] == (int)src.correctIndex)
        out[i].correctIndex = a;
    }
    strlcpy(out[i].question, src.question, MAX_QUESTION_LEN);
  }
  delete[] bank;
  return count;
}

// ── Default data ─────────────────────────────────

void loadDefaultData(AppState &state) {
  const uint16_t colors[] = {COLOR_KID_0, COLOR_KID_1, COLOR_KID_2,
                             COLOR_KID_3};
  const char *tasks[] = {"Task 1", "Task 2", "Task 3"};
  const int taskCount = 3;

  state.kidCount = MAX_KIDS;
  for (int i = 0; i < MAX_KIDS; i++) {
    char name[24];
    snprintf(name, sizeof(name), "Kid %d", i + 1);
    strlcpy(state.kids[i].name, name, sizeof(state.kids[i].name));
    snprintf(name, sizeof(name), "00:00:00:0%d", i + 1);
    strlcpy(state.kids[i].rfidUID, name, sizeof(state.kids[i].rfidUID));
    state.kids[i].color = colors[i];
    state.kids[i].active = true;
    state.kids[i].rewardCount = 1;
    strlcpy(state.kids[i].rewards[0].name, "Reward", 32);
    strlcpy(state.kids[i].rewards[0].type, "min", 8);
    state.kids[i].rewards[0].maxValue = 60;

    for (int d = 0; d < DAYS_COUNT; d++) {
      state.kids[i].week[d].taskCount = taskCount;
      for (int t = 0; t < taskCount; t++) {
        strlcpy(state.kids[i].week[d].tasks[t].name, tasks[t], 32);
        state.kids[i].week[d].tasks[t].done = false;
        state.kids[i].week[d].tasks[t].quizTopic[0] = '\0'; // no quiz
      }
    }
  }
}

// ── Load JSON ────────────────────────────────────

void loadData(AppState &state) {
  if (!SPIFFS.exists("/data.json")) {
    Serial.println("No data.json – loading defaults");
    loadDefaultData(state);
    return;
  }

  File f = SPIFFS.open("/data.json", "r");
  if (!f) { loadDefaultData(state); return; }

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    loadDefaultData(state);
    return;
  }

  JsonArray kids = doc["kids"];
  state.kidCount = 0;

  if (kids.size() == 0) {
    Serial.println("Empty kids – loading defaults");
    loadDefaultData(state);
    return;
  }

  state.nextTaskId = doc["nextTaskId"] | 0;

  for (JsonObject k : kids) {
    if (state.kidCount >= MAX_KIDS) break;
    int i = state.kidCount++;

    strlcpy(state.kids[i].name,    k["name"] | "Kid",        24);
    strlcpy(state.kids[i].rfidUID, k["rfid"] | "00:00:00:00", 16);
    state.kids[i].color  = (uint16_t)(k["color"]  | COLOR_KID_0);
    state.kids[i].active = k["active"] | true;

    state.kids[i].rewardCount = 0;
    JsonArray rewards = k["rewards"];
    if (rewards) {
      for (JsonObject r : rewards) {
        if (state.kids[i].rewardCount >= 3) break;
        int ri = state.kids[i].rewardCount++;
        strlcpy(state.kids[i].rewards[ri].name, r["name"] | "Reward", 32);
        strlcpy(state.kids[i].rewards[ri].type, r["type"] | "min",    8);
        state.kids[i].rewards[ri].maxValue = r["maxValue"] | 60;
      }
    }

    JsonArray week = k["week"];
    for (int d = 0; d < DAYS_COUNT && d < (int)week.size(); d++) {
      JsonArray tasks = week[d]["tasks"];
      state.kids[i].week[d].taskCount = 0;
      for (JsonObject t : tasks) {
        if (state.kids[i].week[d].taskCount >= MAX_TASKS_PER_DAY) break;
        int ti = state.kids[i].week[d].taskCount++;
        strlcpy(state.kids[i].week[d].tasks[ti].name,      t["name"] | "Task",  32);
        state.kids[i].week[d].tasks[ti].done = t["done"] | false;
        strlcpy(state.kids[i].week[d].tasks[ti].quizTopic, t["quizTopic"] | "", MAX_QUIZ_TOPIC_LEN);
        state.kids[i].week[d].tasks[ti].id = t["id"] | 0;
      }
    }
  }

  Serial.printf("Loaded %d kids, nextTaskId=%u\n", state.kidCount, state.nextTaskId);
}

// ── Save JSON ────────────────────────────────────

void saveData(AppState &state) {
  DynamicJsonDocument doc(16384);
  doc["nextTaskId"] = state.nextTaskId;

  JsonArray kids = doc.createNestedArray("kids");
  for (int i = 0; i < state.kidCount; i++) {
    JsonObject k = kids.createNestedObject();
    k["name"]   = state.kids[i].name;
    k["rfid"]   = state.kids[i].rfidUID;
    k["color"]  = state.kids[i].color;
    k["active"] = state.kids[i].active;

    JsonArray rewards = k.createNestedArray("rewards");
    for (int r = 0; r < state.kids[i].rewardCount; r++) {
      JsonObject robj = rewards.createNestedObject();
      robj["name"]     = state.kids[i].rewards[r].name;
      robj["type"]     = state.kids[i].rewards[r].type;
      robj["maxValue"] = state.kids[i].rewards[r].maxValue;
    }

    JsonArray week = k.createNestedArray("week");
    for (int d = 0; d < DAYS_COUNT; d++) {
      JsonObject day   = week.createNestedObject();
      JsonArray  tasks = day.createNestedArray("tasks");
      for (int t = 0; t < state.kids[i].week[d].taskCount; t++) {
        JsonObject task = tasks.createNestedObject();
        task["name"] = state.kids[i].week[d].tasks[t].name;
        task["done"] = state.kids[i].week[d].tasks[t].done;
        task["id"]   = state.kids[i].week[d].tasks[t].id;
        if (state.kids[i].week[d].tasks[t].quizTopic[0] != '\0')
          task["quizTopic"] = state.kids[i].week[d].tasks[t].quizTopic;
      }
    }
  }

  File f = SPIFFS.open("/data.json", "w");
  if (!f) {
    Serial.println("saveData: failed to open data.json!");
    return;
  }
  size_t written = serializeJson(doc, f);
  f.close();
  if (written == 0)
    Serial.println("saveData: write failed - SPIFFS may be full!");
  else
    Serial.printf("Data saved (%d bytes)\n", written);
}