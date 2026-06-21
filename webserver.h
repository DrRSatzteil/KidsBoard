// =====================================================
// webserver.h – Web-Interface + REST API
// =====================================================
#pragma once

#include "config.h"
#include "data.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>

// ── Settings stored in /settings.json ─────────────

String loadApiKey() {
  if (!SPIFFS.exists("/settings.json")) return "";
  File f = SPIFFS.open("/settings.json", "r");
  if (!f) return "";
  DynamicJsonDocument doc(512);
  deserializeJson(doc, f);
  f.close();
  return doc["anthropicKey"] | "";
}

String loadAnthropicModel() {
  if (!SPIFFS.exists("/settings.json")) return "claude-haiku-4-5-20251001";
  File f = SPIFFS.open("/settings.json", "r");
  if (!f) return "claude-haiku-4-5-20251001";
  DynamicJsonDocument doc(512);
  deserializeJson(doc, f);
  f.close();
  return doc["anthropicModel"] | "claude-haiku-4-5-20251001";
}

void saveSettings(const String &key, const String &model) {
  DynamicJsonDocument doc(512);
  if (SPIFFS.exists("/settings.json")) {
    File f = SPIFFS.open("/settings.json", "r");
    if (f) { deserializeJson(doc, f); f.close(); }
  }
  if (key.length() > 0)   doc["anthropicKey"]   = key;
  if (model.length() > 0) doc["anthropicModel"] = model;
  File f = SPIFFS.open("/settings.json", "w");
  serializeJson(doc, f);
  f.close();
}

// ── Anthropic API call ─────────────────────────────

int callAnthropicForQuiz(const String &apiKey, const String &topic,
                         QuizQuestion *out, int maxQuestions) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.anthropic.com", 443)) {
    Serial.println("Anthropic: connection failed");
    return -1;
  }

  String safeTopic = topic;
  safeTopic.replace("\"", "'");

  char prompt[512];
  snprintf(prompt, sizeof(prompt), STR_QUIZ_PROMPT_TEMPLATE, maxQuestions,
           safeTopic.c_str(), MAX_QUESTION_LEN - 8, MAX_ANSWER_LEN - 8);

  DynamicJsonDocument bodyDoc(2048);
  bodyDoc["model"] = loadAnthropicModel();
  bodyDoc["max_tokens"] = 4096;
  JsonArray messages = bodyDoc.createNestedArray("messages");
  JsonObject msg = messages.createNestedObject();
  msg["role"] = "user";
  msg["content"] = prompt;
  String body;
  serializeJson(bodyDoc, body);

  client.println("POST /v1/messages HTTP/1.1");
  client.println("Host: api.anthropic.com");
  client.println("Content-Type: application/json");
  client.println("x-api-key: " + apiKey);
  client.println("anthropic-version: 2023-06-01");
  client.println("Content-Length: " + String(body.length()));
  client.println("Connection: close");
  client.println();
  client.print(body);

  unsigned long timeout = millis() + 90000;
  while (client.available() == 0) {
    if (millis() > timeout) { Serial.println("Anthropic: timeout"); client.stop(); return -1; }
    delay(50);
  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = "";
  while (client.available()) {
    String chunkSizeLine = client.readStringUntil('\n');
    chunkSizeLine.trim();
    if (chunkSizeLine.length() == 0) continue;
    int chunkSize = strtol(chunkSizeLine.c_str(), nullptr, 16);
    if (chunkSize == 0) break;
    for (int i = 0; i < chunkSize; i++) {
      unsigned long t = millis() + 2000;
      while (!client.available() && millis() < t) delay(1);
      if (client.available()) response += (char)client.read();
    }
    client.readStringUntil('\n');
  }
  client.stop();

  DynamicJsonDocument respDoc(16384);
  if (deserializeJson(respDoc, response)) { Serial.println("Anthropic: envelope parse error"); return -1; }

  String jsonText = respDoc["content"][0]["text"] | "";
  if (jsonText.isEmpty()) { Serial.println("Anthropic: empty content"); return -1; }

  jsonText.trim();
  if (jsonText.startsWith("```")) {
    int start = jsonText.indexOf('\n') + 1;
    int end = jsonText.lastIndexOf("```");
    if (end > start) jsonText = jsonText.substring(start, end);
  }

  DynamicJsonDocument qDoc(8192);
  if (deserializeJson(qDoc, jsonText) || !qDoc.is<JsonArray>()) {
    Serial.println("Anthropic: question array parse error");
    return -1;
  }

  int count = 0;
  for (JsonObject q : qDoc.as<JsonArray>()) {
    if (count >= maxQuestions) break;
    strlcpy(out[count].question, q["q"] | "", MAX_QUESTION_LEN);
    out[count].correctIndex = q["c"] | 0;
    JsonArray answers = q["a"];
    for (int j = 0; j < 4 && j < (int)answers.size(); j++)
      strlcpy(out[count].answers[j].text, answers[j] | "", MAX_ANSWER_LEN);
    count++;
  }

  Serial.printf("Anthropic: got %d questions for topic '%s'\n", count, topic.c_str());
  return count;
}

// ── HTML ──────────────────────────────────────────

const char HTML_INDEX[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KidsBoard</title>
<!-- To localize: edit the S object below. Everything else is code. -->
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0f0f1a;color:#e8e8f0;font-family:'Courier New',monospace;font-size:14px;padding:14px}
.app{max-width:780px;margin:0 auto}
.topbar{display:flex;align-items:center;justify-content:space-between;padding:10px 14px;background:#1a1a2e;border:0.5px solid #2a2a4a;border-radius:10px;margin-bottom:12px}
.topbar-title{font-size:15px;font-weight:600;color:#4ecdc4;letter-spacing:1px}
.topbar-right{display:flex;gap:8px;align-items:center}
.toast{font-size:11px;color:#4ecdc4;background:#0f3d2e;border:0.5px solid #4ecdc4;border-radius:6px;padding:4px 10px;display:none}
.btn-save{background:#4ecdc4;border:none;color:#0f0f1a;border-radius:6px;padding:6px 14px;font-size:12px;font-weight:700;cursor:pointer;font-family:inherit}
.btn-settings{background:transparent;border:0.5px solid #3a3a5a;color:#888;border-radius:6px;padding:6px 10px;cursor:pointer;font-size:16px;line-height:1}
.tabs{display:flex;gap:4px;margin-bottom:12px}
.tab-btn{background:transparent;border:0.5px solid #2a2a4a;color:#666;border-radius:6px;padding:6px 12px;cursor:pointer;font-family:inherit;font-size:12px;white-space:nowrap}
.tab-btn.active{border-color:#4ecdc4;color:#4ecdc4;background:#1a1a2e}
.card{background:#1a1a2e;border:0.5px solid #2a2a4a;border-radius:10px;padding:14px;margin-bottom:10px}
.section-label{font-size:10px;color:#555;text-transform:uppercase;letter-spacing:1.5px;margin-bottom:6px}
.kid-tabs{display:flex;gap:4px;margin-bottom:12px;flex-wrap:wrap}
.kid-tab{background:transparent;border:0.5px solid #2a2a4a;color:#666;border-radius:6px;padding:5px 12px;cursor:pointer;font-family:inherit;font-size:12px;display:flex;align-items:center;gap:5px}
.kid-tab.active{background:#1a1a2e;color:#e8e8f0}
.kdot{width:8px;height:8px;border-radius:50%;display:inline-block;flex-shrink:0}
input[type=text],input[type=password],input[type=number]{background:#0f0f1a;border:0.5px solid #2a2a4a;border-radius:6px;padding:6px 10px;color:#e8e8f0;font-family:inherit;font-size:12px}
input[type=text]:focus,input[type=password]:focus,input[type=number]:focus{outline:none;border-color:#4ecdc4}
select{background:#0f0f1a;border:0.5px solid #2a2a4a;border-radius:6px;padding:6px 8px;color:#e8e8f0;font-family:inherit;font-size:12px;cursor:pointer}
.btn-sm{background:transparent;border:0.5px solid #2a2a4a;color:#888;border-radius:6px;padding:6px 10px;cursor:pointer;font-family:inherit;font-size:12px;white-space:nowrap}
.btn-sm:hover{border-color:#4ecdc4;color:#4ecdc4}
.btn-danger:hover{border-color:#f87171;color:#f87171}
.btn-add{width:100%;background:transparent;border:0.5px solid #2a2a4a;color:#888;border-radius:6px;padding:8px;cursor:pointer;font-family:inherit;font-size:12px;margin-top:4px}
.task-table{width:100%;border-collapse:collapse}
.task-table th{font-size:10px;color:#555;text-align:center;padding:4px 4px;font-weight:400;width:38px}
.task-table th:first-child{text-align:left;width:auto}
.task-table th:last-child{width:24px}
.task-table td{border-top:0.5px solid #1f1f38;padding:5px 4px;text-align:center}
.task-table td:first-child{text-align:left}
.task-name-input{background:transparent;border:none;color:#aaa;font-family:inherit;font-size:11px;width:90px;padding:0}
.task-name-input:focus{outline:none;color:#e8e8f0}
.quiz-pill{border-radius:4px;padding:1px 5px;font-size:9px;cursor:pointer;white-space:nowrap;border:0.5px solid #2a2a4a;background:transparent;color:#444;font-family:inherit}
.quiz-pill.has-quiz{background:#2a1a4a;border-color:#6a3aaa;color:#c084fc}
.cb{width:18px;height:18px;border:1px solid #3a3a5a;border-radius:3px;display:inline-flex;align-items:center;justify-content:center;cursor:pointer;font-size:10px;font-weight:700}
.cb.checked{background:#4ecdc4;border-color:#4ecdc4;color:#0f0f1a}
.cb.absent{border-style:dashed;border-color:#2a2a4a;opacity:0.4}
.cb.absent:hover{opacity:0.8;border-color:#4ecdc4}
.field-row{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.field-label{font-size:10px;color:#555;text-transform:uppercase;letter-spacing:1px;flex-shrink:0;width:50px}
.reward-row{display:flex;gap:6px;align-items:center;margin-bottom:6px;flex-wrap:wrap}
.rfid-box{background:rgba(255,230,100,0.06);border:0.5px solid rgba(255,230,100,0.2);border-radius:8px;padding:10px;margin-top:8px}
.rfid-uid{font-family:monospace;color:#ffe066;font-size:11px;margin-bottom:6px}
.status-msg{padding:8px 12px;border-radius:6px;font-size:11px;margin-top:8px;display:none}
.status-msg.ok{background:rgba(0,200,0,0.1);color:#4ade80;border:0.5px solid rgba(0,200,0,0.2)}
.status-msg.err{background:rgba(255,0,0,0.1);color:#f87171;border:0.5px solid rgba(255,0,0,0.2)}
.warn-msg{padding:8px 12px;border-radius:6px;font-size:11px;margin-bottom:10px;background:rgba(255,180,0,0.1);color:#fbbf24;border:0.5px solid rgba(255,180,0,0.25)}
.overlay{position:fixed;inset:0;background:rgba(0,0,0,0.75);display:flex;align-items:center;justify-content:center;z-index:200;padding:16px}
.modal{background:#1a1a2e;border:0.5px solid #3a3a5a;border-radius:12px;width:100%;max-width:520px;max-height:85vh;display:flex;flex-direction:column;overflow:hidden}
.modal-header{display:flex;align-items:center;justify-content:space-between;padding:12px 16px;border-bottom:0.5px solid #2a2a4a;flex-shrink:0}
.modal-title{font-size:14px;font-weight:600;color:#c084fc}
.modal-close{background:transparent;border:none;color:#666;font-size:20px;cursor:pointer;line-height:1}
.modal-body{overflow-y:auto;flex:1;padding:16px}
.modal-footer{padding:10px 16px;border-top:0.5px solid #2a2a4a;display:flex;gap:8px;justify-content:flex-end;flex-shrink:0}
.quiz-q{background:#0f0f1a;border:0.5px solid #2a2a4a;border-radius:8px;padding:10px;margin-bottom:8px}
.quiz-q-input{background:transparent;border:none;border-bottom:0.5px solid #2a2a4a;color:#e8e8f0;font-family:inherit;font-size:12px;padding:2px 0 4px;width:100%}
.quiz-q-input:focus{outline:none;border-color:#c084fc}
.answer-row{display:flex;gap:6px;align-items:center;margin-bottom:3px}
.answer-row input[type=radio]{accent-color:#4ecdc4;flex-shrink:0}
.answer-input{flex:1;background:transparent;border:none;border-bottom:0.5px solid #1f1f38;color:#aaa;font-family:inherit;font-size:11px;padding:2px 0 3px}
.answer-input:focus{outline:none;border-color:#4ecdc4;color:#e8e8f0}
.btn-gen{background:#2a1a4a;border:0.5px solid #6a3aaa;color:#c084fc;border-radius:6px;padding:7px 12px;cursor:pointer;font-family:inherit;font-size:11px;white-space:nowrap}
.btn-gen:disabled{opacity:0.5;cursor:not-allowed}
.btn-gen.done{background:#0f3d2e;border-color:#4ecdc4;color:#4ecdc4}
.settings-sheet{position:fixed;inset:0;background:rgba(0,0,0,0.7);display:flex;align-items:flex-end;justify-content:center;z-index:200}
.settings-inner{background:#1a1a2e;border:0.5px solid #3a3a5a;border-radius:12px 12px 0 0;width:100%;max-width:680px;padding:20px}
.settings-handle{width:36px;height:3px;background:#3a3a5a;border-radius:2px;margin:0 auto 16px}
</style>
</head>
<body>
<div class="app">
  <div class="topbar">
    <span class="topbar-title" id="appTitle"></span>
    <div class="topbar-right">
      <span class="toast" id="toast"></span>
      <button class="btn-settings" onclick="showSettings()">⚙</button>
      <button class="btn-save" id="btnSave" onclick="save()"></button>
    </div>
  </div>
  <div class="tabs" id="mainTabs"></div>
  <div id="tab-plan"></div>
  <div id="tab-kids" style="display:none"></div>
  <div id="tab-rewards" style="display:none"></div>
  <div id="tab-misc" style="display:none"></div>
  <div class="status-msg" id="globalStatus"></div>
</div>

<div class="overlay" id="quizModal" style="display:none" onclick="closeQuizOnBg(event)">
  <div class="modal">
    <div class="modal-header">
      <span class="modal-title" id="quizModalTitle">🧠 Quiz</span>
      <button class="modal-close" onclick="closeQuiz()">×</button>
    </div>
    <div class="modal-body" id="quizModalBody"></div>
    <div class="modal-footer">
      <button class="btn-sm" onclick="closeQuiz()" id="quizCancelBtn"></button>
      <button class="btn-save" onclick="saveQuizModal()" id="quizSaveBtn"></button>
    </div>
  </div>
</div>

<div class="settings-sheet" id="settingsSheet" style="display:none" onclick="closeSettingsOnBg(event)">
  <div class="settings-inner">
    <div class="settings-handle"></div>
    <div style="font-size:14px;font-weight:600;color:#ccc;margin-bottom:16px" id="settingsTitle"></div>
    <div style="margin-bottom:14px">
      <div class="section-label" id="settingsApiKeyLabel"></div>
      <input type="password" id="apiKeyInput" style="width:100%;margin-bottom:4px">
      <div style="font-size:10px;color:#444;margin-bottom:4px" id="settingsApiKeyHint"></div>
      <div id="keyStatus" style="font-size:11px;color:#4ecdc4;display:none"></div>
    </div>
    <div style="margin-bottom:16px">
      <div class="section-label" id="settingsModelLabel"></div>
      <input type="text" id="modelInput" style="width:100%;margin-bottom:4px">
      <div style="font-size:10px;color:#444" id="settingsModelHint"></div>
    </div>
    <div style="display:flex;gap:8px">
      <button class="btn-sm" onclick="closeSettings()" id="settingsCancelBtn"></button>
      <button class="btn-save" onclick="saveSettingsModal()" id="settingsSaveBtn"></button>
    </div>
  </div>
</div>

<script>
const DAYS = ['Mo','Di','Mi','Do','Fr'];
const MAX_TASKS_PER_DAY_UI = 5;
let data = null;
let activeKid = 0;

// ── Localization ──────────────────────────────────
// All user-visible strings in one place. Edit here to translate.
const S = {
  // App
  appTitle:          '🗓 KidsBoard',
  save:              '✓ Speichern',
  saved:             '✓ Gespeichert!',
  saveError:         'Fehler beim Speichern!',
  cancel:            'Abbrechen',
  remove:            '✕',

  // Tabs
  tabPlan:           '📅 Wochenplan',
  tabKids:           '👤 Kinder',
  tabRewards:        '🏆 Belohnungen',
  tabMisc:           '⚠️ Woche',

  // Plan tab
  tasksAndDays:      'Aufgaben & Tage',
  taskCol:           'Aufgabe',
  addTask:           '+ Aufgabe',
  newTaskName:       'Neue Aufgabe',
  quizPillEdit:      'Quiz ✎',
  quizPillAdd:       '+Quiz',
  resetKidBtn:       '↺ Zuruecksetzen',
  resetKidTitle:     'Setzt alle Haekchen dieses Kindes zurueck',
  resetKidConfirm:   'Alle Haekchen dieses Kindes zuruecksetzen?',
  dayPresentHint:    'Klicken zum Entfernen an diesem Tag',
  dayAbsentHint:     'Klicken zum Hinzufuegen an diesem Tag',
  tooManyTasksWarn:  (day,n) => `\u26A0\uFE0F ${day}: ${n} Aufgaben aktiv \u2013 nur ${MAX_TASKS_PER_DAY_UI} passen aufs Display!`,

  // Kids tab
  fieldName:         'Name',
  fieldColor:        'Farbe',
  fieldActive:       'Aktiv',
  rfidCard:          'RFID Karte',
  rfidNotAssigned:   'Noch nicht zugewiesen',
  rfidAssign:        'Naechste Karte zuweisen',
  rfidScanning:      'Karte jetzt auflegen...',
  rfidAssigned:      'Karte zugewiesen:',
  rfidNoCard:        'Keine Karte erkannt.',
  rfidTimeout:       'Timeout. Nochmal versuchen.',
  addKid:            '+ Kind hinzufügen',
  newKidName:        'Neues Kind',

  // Rewards tab
  addReward:         '+ Belohnung hinzufügen',
  rewardNameDefault: 'Belohnung',
  typeMin:           'Minuten',
  typePcs:           'Stueck',
  typeEur:           'EUR (Cent)',
  typeTxt:           'Freitext (5\u2605)',
  typeMys:           'Mystery (5\u2605)',

  // Misc tab
  resetWeekTitle:    '\u26A0\uFE0F Woche zuruecksetzen',
  resetWeekDesc:     'Setzt alle Haekchen der aktuellen Woche zurueck (alle Kinder).',
  resetWeekBtn:      'Alle Aufgaben zuruecksetzen',
  resetWeekConfirm:  'Alle Haekchen zuruecksetzen?',
  resetWeekDone:     'Woche zurueckgesetzt!',
  resetWeekError:    'Fehler',

  // Quiz modal
  quizModalPrefix:   '\uD83E\uDDE0 Quiz \u2013 ',
  quizThemeLabel:    'Thema',
  quizThemeTip:      'Tipp: Je genauer, desto besser. z.B. "Englisch 5. Klasse: going to \u2013 Zukunft"',
  quizThemePlaceholder: 'z.B. Kleines Einmaleins',
  quizGenerate:      '\uD83E\uDD16 KI generieren',
  quizGenerating:    '\u23F3 Generiere...',
  quizGenerated:     (n) => `\u2713 ${n} Fragen`,
  quizGenError:      '\u274C Fehler',
  quizNoApiKey:      'Kein API Key',
  quizQuestionsLabel:(n) => `Fragen (${n}) \u00B7 5 zufaellig pro Runde`,
  quizCorrectHint:   '\u25CF = richtige Antwort',
  quizQPlaceholder:  'Frage...',
  quizAPlaceholder:  (i) => `Antwort ${String.fromCharCode(65+i)}`,
  quizAddQuestion:   '+ Frage hinzufügen',

  // Settings sheet
  settingsTitle:     '\u2699\uFE0F Einstellungen',
  settingsApiKey:    'Anthropic API Key',
  settingsApiKeyPh:  'sk-ant-...',
  settingsApiKeyHint:'Nur fuer KI-Fragengeneration. Wird sicher auf dem Geraet gespeichert.',
  settingsApiKeyOk:  '\u2713 API Key konfiguriert',
  settingsModel:     'Modell',
  settingsModelPh:   'claude-haiku-4-5-20251001',
  settingsModelHint: 'Standard: claude-haiku-4-5-20251001',
};

let apiKeyConfigured = false;
let currentModel = '';
let quizModalKi = -1;
let quizModalTaskId = -1;
let quizModalQuestions = [];

function sanitize(s) {
  return s.replace(/ä/g,'ae').replace(/Ä/g,'Ae')
          .replace(/ö/g,'oe').replace(/Ö/g,'Oe')
          .replace(/ü/g,'ue').replace(/Ü/g,'Ue')
          .replace(/ß/g,'ss');
}

async function load() {
  // Set all static strings from S object
  document.getElementById('appTitle').textContent        = S.appTitle;
  document.getElementById('toast').textContent           = S.saved;
  document.getElementById('btnSave').textContent         = S.save;
  document.getElementById('quizCancelBtn').textContent   = S.cancel;
  document.getElementById('quizSaveBtn').textContent     = S.save;
  document.getElementById('settingsTitle').textContent   = S.settingsTitle;
  document.getElementById('settingsApiKeyLabel').textContent = S.settingsApiKey;
  document.getElementById('settingsApiKeyHint').textContent  = S.settingsApiKeyHint;
  document.getElementById('keyStatus').textContent       = S.settingsApiKeyOk;
  document.getElementById('settingsModelLabel').textContent  = S.settingsModel;
  document.getElementById('settingsModelHint').textContent   = S.settingsModelHint;
  document.getElementById('settingsCancelBtn').textContent   = S.cancel;
  document.getElementById('settingsSaveBtn').textContent     = S.save;
  document.getElementById('apiKeyInput').placeholder     = S.settingsApiKeyPh;
  document.getElementById('modelInput').placeholder      = S.settingsModelPh;

  // Tabs
  document.getElementById('mainTabs').innerHTML = [
    ['plan', S.tabPlan], ['kids', S.tabKids],
    ['rewards', S.tabRewards], ['misc', S.tabMisc]
  ].map(([id, label], i) =>
    `<button class="tab-btn ${i===0?'active':''}" onclick="switchTab(this,'${id}')">${label}</button>`
  ).join('');

  const [dr, sr] = await Promise.all([fetch('/api/data'), fetch('/api/settings')]);
  data = await dr.json();
  const s = await sr.json();
  apiKeyConfigured = s.hasKey || false;
  currentModel = s.model || 'claude-haiku-4-5-20251001';
  if (apiKeyConfigured) document.getElementById('keyStatus').style.display = 'block';
  document.getElementById('modelInput').value = currentModel;

  // Assign a stable client-side ID to each distinct task name.
  // This ID is the source of truth for matching the "same" task
  // across days in the UI - never the name itself, which can change
  // while typing. IDs are stripped again before saving to the device.
  assignTaskIds();

  // Sync quiz topics across days by task ID (NOT by name or array
  // index - tasks can legitimately differ per day; the topic for a
  // given task identity should stay consistent everywhere it appears).
  syncQuizTopicsById();

  renderAll();
}

let nextTaskId = 1;

// Walks all kids/days and assigns a stable _id to each task object.
// Tasks with the same name (first-seen order) across days get the
// same ID, since on first load that's the best signal we have for
// "this is conceptually the same task". After this point, identity
// is tracked by ID only - renaming never breaks the link.
function assignTaskIds() {
  nextTaskId = 1;
  data.kids.forEach(kid => {
    const idByName = {};
    kid.week.forEach(day => {
      day.tasks.forEach(t => {
        if (!(t.name in idByName)) idByName[t.name] = nextTaskId++;
        t._id = idByName[t.name];
      });
    });
  });
}

// For a given task ID, make sure every occurrence (on any day)
// shares the same quiz topic. Last non-empty value wins as source.
// Does NOT force tasks to exist on all days.
function syncQuizTopicsById() {
  data.kids.forEach(kid => {
    const topicById = {};
    kid.week.forEach(day => {
      day.tasks.forEach(t => {
        if (t.quizTopic && t.quizTopic.trim()) topicById[t._id] = t.quizTopic;
      });
    });
    kid.week.forEach(day => {
      day.tasks.forEach(t => {
        if (topicById[t._id] !== undefined) t.quizTopic = topicById[t._id];
      });
    });
  });
}

function renderAll() {
  renderPlan();
  renderKids();
  renderRewards();
  renderMisc();
}

// ── Plan Tab ──────────────────────────────────────

function renderPlan() {
  const el = document.getElementById('tab-plan');
  if (!data) return;

  const kidTabsHtml = data.kids.map((k,i) => {
    const hex = colorHex(k.color);
    return `<button class="kid-tab ${i===activeKid?'active':''}" onclick="setActiveKid(${i})">
      <span class="kdot" style="background:#${hex}"></span>${k.name}
    </button>`;
  }).join('');

  el.innerHTML = `
    <div class="kid-tabs">${kidTabsHtml}</div>
    <div id="planWarnings"></div>
    <div class="card" id="planCard"></div>
  `;
  renderPlanCard();
}

// Each "row" in the table is a task ID (union across all days).
// A cell is "present" if that day's task array contains an entry
// with that ID, "absent" otherwise. This lets tasks legitimately
// differ per day (e.g. "Trumpet lesson" only on Mondays). Matching
// is by ID, never by name - names can be edited freely without
// breaking the identity link.
function renderPlanCard() {
  const kid = data.kids[activeKid];

  const rowIds = [];
  const rowNames = {};
  const seen = new Set();
  kid.week.forEach(day => {
    day.tasks.forEach(t => {
      if (!seen.has(t._id)) { seen.add(t._id); rowIds.push(t._id); rowNames[t._id] = t.name; }
      // Keep the row label in sync with the latest-seen name for this ID
      rowNames[t._id] = t.name;
    });
  });

  function findTask(di, id) {
    return kid.week[di].tasks.find(t => t._id === id) || null;
  }

  function quizTopicForId(id) {
    for (const day of kid.week) {
      const t = day.tasks.find(x => x._id === id);
      if (t && t.quizTopic && t.quizTopic.trim()) return t.quizTopic;
    }
    return '';
  }

  let rows = '';
  rowIds.forEach((id) => {
    const name = rowNames[id];
    const quizTopic = quizTopicForId(id);
    const hasQuiz = !!quizTopic;

    const dayCells = DAYS.map((_,di) => {
      const task = findTask(di, id);
      const present = task !== null;
      const hint = present ? S.dayPresentHint : S.dayAbsentHint;
      return `<td>
        <div class="cb ${present?'present':'absent'}"
             onclick="toggleDayPresence(${di},${id})"
             title="${escAttr(hint)}">
          ${present?'✓':''}
        </div>
      </td>`;
    }).join('');

    rows += `<tr>
      <td>
        <div style="display:flex;align-items:center;gap:6px">
          <input class="task-name-input" value="${escHtml(name)}"
            oninput="renameTaskById(${id},this.value)">
          <button class="quiz-pill ${hasQuiz?'has-quiz':''}" onclick="openQuiz(${id})">${hasQuiz?S.quizPillEdit:S.quizPillAdd}</button>
        </div>
      </td>
      ${dayCells}
      <td><button class="btn-sm btn-danger" style="padding:2px 6px;font-size:11px" onclick="removeTaskById(${id})">${S.remove}</button></td>
    </tr>`;
  });

  document.getElementById('planCard').innerHTML = `
    <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:14px;flex-wrap:wrap;gap:8px">
      <span style="font-size:14px;font-weight:600;color:#ccc">${S.tasksAndDays}</span>
      <div style="display:flex;gap:8px">
        <button class="btn-sm" onclick="resetKidTasks()" title="${S.resetKidTitle}">${S.resetKidBtn}</button>
        <button class="btn-sm" onclick="addTask()">${S.addTask}</button>
      </div>
    </div>
    <div style="overflow-x:auto">
      <table class="task-table">
        <thead><tr>
          <th style="text-align:left">${S.taskCol}</th>
          ${DAYS.map(d=>`<th>${d}</th>`).join('')}
          <th></th>
        </tr></thead>
        <tbody>${rows}</tbody>
      </table>
    </div>
  `;

  renderPlanWarnings(kid);
}

// Warn if any day has more active tasks than fit on the device display.
function renderPlanWarnings(kid) {
  const el = document.getElementById('planWarnings');
  if (!el) return;
  const warnings = [];
  kid.week.forEach((day, di) => {
    if (day.tasks.length > MAX_TASKS_PER_DAY_UI) {
      warnings.push(S.tooManyTasksWarn(DAYS[di], day.tasks.length));
    }
  });
  el.innerHTML = warnings.map(w => `<div class="warn-msg">${w}</div>`).join('');
}

function setActiveKid(i) {
  activeKid = i;
  renderPlan();
}

// Toggle whether a task (by name) exists on a given day.
// Re-adding a previously-removed task starts as not-done and
// inherits the shared quiz topic and name (if any) for that ID.
function toggleDayPresence(di, id) {
  const day = data.kids[activeKid].week[di];
  const idx = day.tasks.findIndex(t => t._id === id);
  if (idx >= 0) {
    day.tasks.splice(idx, 1);
  } else {
    let topic = '', name = S.newTaskName;
    for (const d of data.kids[activeKid].week) {
      const t = d.tasks.find(x => x._id === id);
      if (t) { topic = t.quizTopic || ''; name = t.name; break; }
    }
    day.tasks.push({ name, done: false, quizTopic: topic, _id: id });
  }
  renderPlanCard();
}

// Renaming only touches the .name field; the ID link is untouched,
// so this never creates ghost rows or breaks the day-to-day match.
function renameTaskById(id, newVal) {
  const safe = sanitize(newVal);
  data.kids[activeKid].week.forEach(day => {
    day.tasks.forEach(t => { if (t._id === id) t.name = safe; });
  });
  // Don't re-render while typing (would lose input focus).
}

function removeTaskById(id) {
  data.kids[activeKid].week.forEach(day => {
    day.tasks = day.tasks.filter(t => t._id !== id);
  });
  renderPlanCard();
}

function addTask() {
  const id = nextTaskId++;
  let name = S.newTaskName;
  const existingNames = new Set();
  data.kids[activeKid].week.forEach(d => d.tasks.forEach(t => existingNames.add(t.name)));
  if (existingNames.has(name)) {
    let i = 2;
    while (existingNames.has(`${S.newTaskName} ${i}`)) i++;
    name = `${S.newTaskName} ${i}`;
  }
  data.kids[activeKid].week.forEach(day => day.tasks.push({ name, done: false, quizTopic: '', _id: id }));
  renderPlanCard();
}

function resetKidTasks() {
  if (!confirm(S.resetKidConfirm)) return;
  data.kids[activeKid].week.forEach(day => day.tasks.forEach(t => t.done = false));
  renderPlanCard();
}

// ── Quiz Modal ────────────────────────────────────
// Identified by task ID, not name or array index - the same task
// identity may live at different indices (or be absent) on different
// days, and the name itself can change while the modal is open.

async function openQuiz(id) {
  quizModalKi = activeKid;
  quizModalTaskId = id;
  // Look up the current display name for the title via any occurrence
  let displayName = S.newTaskName;
  for (const day of data.kids[activeKid].week) {
    const t = day.tasks.find(x => x._id === id);
    if (t) { displayName = t.name; break; }
  }
  document.getElementById('quizModalTitle').textContent = S.quizModalPrefix + displayName;

  const week = data.kids[activeKid].week;
  let loadDi = -1, loadTi = -1, topic = '';
  for (let di = 0; di < 5; di++) {
    const ti = week[di].tasks.findIndex(t => t._id === id);
    if (ti >= 0) {
      loadDi = di; loadTi = ti;
      topic = week[di].tasks[ti].quizTopic || '';
      break;
    }
  }

  quizModalQuestions = [];
  if (loadDi >= 0) {
    try {
      const r = await fetch(`/api/quiz/load?kid=${activeKid}&day=${loadDi}&task=${loadTi}`);
      if (r.ok) {
        const res = await r.json();
        if (res.questions) quizModalQuestions = res.questions.map(q => ({ q: q.q, answers: q.a, correct: q.c }));
      }
    } catch(e) {}
  }

  renderQuizModalBody(topic);
  document.getElementById('quizModal').style.display = 'flex';
}

function renderQuizModalBody(topic) {
  const body = document.getElementById('quizModalBody');
  const qHtml = quizModalQuestions.map((q, qi) => `
    <div class="quiz-q">
      <div style="display:flex;gap:6px;align-items:flex-start;margin-bottom:8px">
        <span style="font-size:10px;color:#555;padding-top:2px;min-width:18px">Q${qi+1}</span>
        <input class="quiz-q-input" value="${escHtml(q.q)}" onchange="quizModalQuestions[${qi}].q=sanitize(this.value)" placeholder="${S.quizQPlaceholder}">
        <button onclick="removeQuizQ(${qi})" style="background:transparent;border:none;color:#444;cursor:pointer;font-size:14px;padding:0 2px">&times;</button>
      </div>
      <div style="font-size:9px;color:#444;margin-bottom:4px">${S.quizCorrectHint}</div>
      ${q.answers.map((a,ai) => `
        <div class="answer-row">
          <input type="radio" name="qr${qi}" ${q.correct===ai?'checked':''} onchange="quizModalQuestions[${qi}].correct=${ai}">
          <input class="answer-input" value="${escHtml(a)}" onchange="quizModalQuestions[${qi}].answers[${ai}]=sanitize(this.value)" placeholder="${S.quizAPlaceholder(ai)}">
        </div>`).join('')}
    </div>`).join('');

  body.innerHTML = `
    <div style="margin-bottom:12px">
      <div class="section-label">${S.quizThemeLabel}</div>
      <div style="display:flex;gap:6px;margin-bottom:4px">
        <input type="text" id="quizTopicInput" value="${escHtml(topic)}" placeholder="${S.quizThemePlaceholder}" style="flex:1" oninput="this.value=sanitize(this.value)">
        <button class="btn-gen" id="genBtn" onclick="generateQuiz()" ${!apiKeyConfigured?'disabled title="'+S.quizNoApiKey+'"':''}>${S.quizGenerate}</button>
      </div>
      <div style="font-size:10px;color:#444">${S.quizThemeTip}</div>
    </div>
    <div class="section-label">${S.quizQuestionsLabel(quizModalQuestions.length)}</div>
    <div id="quizQList">${qHtml}</div>
    <button class="btn-add" onclick="addQuizQ()">${S.quizAddQuestion}</button>
  `;
}

function removeQuizQ(qi) {
  quizModalQuestions.splice(qi, 1);
  const topic = document.getElementById('quizTopicInput')?.value || '';
  renderQuizModalBody(topic);
}

function addQuizQ() {
  quizModalQuestions.push({ q: '', answers: ['','','',''], correct: 0 });
  const topic = document.getElementById('quizTopicInput')?.value || '';
  renderQuizModalBody(topic);
}

async function generateQuiz() {
  const topic = document.getElementById('quizTopicInput').value.trim();
  if (!topic) return;
  const btn = document.getElementById('genBtn');
  btn.disabled = true;
  btn.textContent = S.quizGenerating;
  btn.classList.remove('done');

  try {
    const r = await fetch('/api/quiz/generate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ kid: quizModalKi, day: 0, task: 0, topic })
    });
    const res = await r.json();
    if (res.ok && res.questions) {
      quizModalQuestions = res.questions.map(q => ({ q: q.q, answers: q.answers, correct: q.correct }));
      renderQuizModalBody(topic);
      const b = document.getElementById('genBtn');
      if (b) { b.classList.add('done'); b.textContent = S.quizGenerated(res.questions.length); b.disabled = false; }
    } else {
      btn.disabled = false;
      btn.textContent = S.quizGenError;
    }
  } catch(e) {
    btn.disabled = false;
    btn.textContent = S.quizGenError;
  }
}

async function saveQuizModal() {
  const topic = document.getElementById('quizTopicInput')?.value.trim() || '';
  const id = quizModalTaskId;

  // Apply the topic to every occurrence of this task ID
  data.kids[quizModalKi].week.forEach(day => {
    day.tasks.forEach(t => { if (t._id === id) t.quizTopic = topic; });
  });

  // Save the question bank under every day/index where this task exists
  if (quizModalQuestions.length > 0) {
    const payload = quizModalQuestions.map(q => ({ q: q.q, a: q.answers, c: q.correct }));
    const week = data.kids[quizModalKi].week;
    for (let di = 0; di < 5; di++) {
      const ti = week[di].tasks.findIndex(t => t._id === id);
      if (ti >= 0) {
        await fetch('/api/quiz/save', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ kid: quizModalKi, day: di, task: ti, questions: payload })
        });
      }
    }
  }

  closeQuiz();
  renderPlanCard();
}

function closeQuiz() { document.getElementById('quizModal').style.display = 'none'; }
function closeQuizOnBg(e) { if (e.target.id === 'quizModal') closeQuiz(); }

// ── Kids Tab ──────────────────────────────────────

function renderKids() {
  const el = document.getElementById('tab-kids');
  if (!data) return;
  el.innerHTML = data.kids.map((k,ki) => `
    <div class="card">
      <div style="display:flex;align-items:center;gap:10px;margin-bottom:14px">
        <span class="kdot" style="background:#${colorHex(k.color)};width:11px;height:11px"></span>
        <span style="font-size:14px;font-weight:600;color:#ccc">${k.name}</span>
      </div>
      <div class="field-row">
        <span class="field-label">${S.fieldName}</span>
        <input type="text" value="${escHtml(k.name)}" style="flex:1"
          oninput="data.kids[${ki}].name=sanitize(this.value);renderPlan()">
      </div>
      <div class="field-row">
        <span class="field-label">${S.fieldColor}</span>
        <input type="color" value="#${colorHex(k.color)}"
          style="height:32px;width:48px;border:0.5px solid #2a2a4a;border-radius:6px;background:transparent;cursor:pointer;padding:2px"
          oninput="data.kids[${ki}].color=hexToRgb565(this.value);renderKids();renderPlan()">
        <span style="font-size:12px;color:#888;font-family:monospace">#${colorHex(k.color)}</span>
      </div>
      <div class="field-row">
        <span class="field-label">${S.fieldActive}</span>
        <input type="checkbox" ${k.active?'checked':''} onchange="data.kids[${ki}].active=this.checked"
          style="width:17px;height:17px;cursor:pointer;accent-color:#4ecdc4">
      </div>
      <div class="section-label" style="margin-top:6px">${S.rfidCard}</div>
      <div class="rfid-box">
        <div class="rfid-uid">${k.rfid || S.rfidNotAssigned}</div>
        <button class="btn-sm" onclick="assignRFID(${ki})">${S.rfidAssign}</button>
      </div>
    </div>`).join('') +
    `<button class="btn-add" onclick="addKid()">${S.addKid}</button>`;
}

function addKid() {
  const colors = [63519, 2047, 65504, 63488];
  data.kids.push({
    name: S.newKidName,
    rfid: '',
    color: colors[data.kids.length % colors.length],
    active: true,
    rewards: [{ name: S.rewardNameDefault, type: 'min', maxValue: 60 }],
    week: Array(5).fill(null).map(() => ({ tasks: [] }))
  });
  renderKids();
  renderPlan();
}

async function assignRFID(ki) {
  showGlobalStatus(S.rfidScanning, true);
  await fetch('/api/assign-rfid?kid=' + ki);
  for (let i = 0; i < 20; i++) {
    await new Promise(r => setTimeout(r, 500));
    const r = await fetch('/api/assign-rfid-result');
    const res = await r.json();
    if (res.uid) {
      data.kids[ki].rfid = res.uid;
      showGlobalStatus(S.rfidAssigned + ' ' + res.uid, true);
      renderKids();
      return;
    }
    if (res.timeout) { showGlobalStatus(S.rfidNoCard, false); return; }
  }
  showGlobalStatus(S.rfidTimeout, false);
}

// ── Rewards Tab ───────────────────────────────────

function renderRewards() {
  const el = document.getElementById('tab-rewards');
  if (!data) return;
  const types = { min:S.typeMin, pcs:S.typePcs, eur:S.typeEur, txt:S.typeTxt, mys:S.typeMys };
  el.innerHTML = data.kids.map((k,ki) => `
    <div class="card">
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:12px">
        <span class="kdot" style="background:#${colorHex(k.color)}"></span>
        <span style="font-size:13px;font-weight:600;color:#ccc">${k.name}</span>
      </div>
      ${(k.rewards||[]).map((r,ri) => `
        <div class="reward-row">
          <input type="text" value="${escHtml(r.name)}" style="flex:2;min-width:100px" onchange="data.kids[${ki}].rewards[${ri}].name=sanitize(this.value)">
          <select onchange="data.kids[${ki}].rewards[${ri}].type=this.value;renderRewards()">
            ${Object.entries(types).map(([v,l]) => `<option value="${v}" ${r.type===v?'selected':''}>${l}</option>`).join('')}
          </select>
          ${r.type!=='txt'&&r.type!=='mys'?`<input type="number" value="${r.maxValue}" style="width:60px;text-align:right" onchange="data.kids[${ki}].rewards[${ri}].maxValue=parseInt(this.value)">`:'' }
          <button class="btn-sm btn-danger" onclick="data.kids[${ki}].rewards.splice(${ri},1);renderRewards()">${S.remove}</button>
        </div>`).join('')}
      <button class="btn-add" onclick="data.kids[${ki}].rewards.push({name:S.rewardNameDefault,type:'min',maxValue:60});renderRewards()">${S.addReward}</button>
    </div>`).join('');
}

// ── Misc Tab ──────────────────────────────────────

function renderMisc() {
  document.getElementById('tab-misc').innerHTML = `
    <div class="card">
      <div style="font-size:13px;font-weight:600;color:#ccc;margin-bottom:8px">${S.resetWeekTitle}</div>
      <div style="font-size:11px;color:#555;margin-bottom:10px">${S.resetWeekDesc}</div>
      <button class="btn-sm btn-danger" onclick="resetWeek()">${S.resetWeekBtn}</button>
    </div>`;
}

async function resetWeek() {
  if (!confirm(S.resetWeekConfirm)) return;
  const r = await fetch('/api/reset', { method: 'POST' });
  const res = await r.json();
  showGlobalStatus(res.ok ? S.resetWeekDone : S.resetWeekError, res.ok);
  if (res.ok) load();
}

// ── Settings Sheet ────────────────────────────────

function showSettings() { document.getElementById('settingsSheet').style.display = 'flex'; }
function closeSettings() { document.getElementById('settingsSheet').style.display = 'none'; }
function closeSettingsOnBg(e) { if (e.target.classList.contains('settings-sheet')) closeSettings(); }

async function saveSettingsModal() {
  const key = document.getElementById('apiKeyInput').value.trim();
  const model = document.getElementById('modelInput').value.trim();
  const r = await fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ anthropicKey: key, anthropicModel: model })
  });
  const res = await r.json();
  if (res.ok) {
    if (key) { apiKeyConfigured = true; document.getElementById('keyStatus').style.display = 'block'; document.getElementById('apiKeyInput').value = ''; }
    if (model) currentModel = model;
    closeSettings();
    showToast();
  }
}

// ── Save ──────────────────────────────────────────

async function save() {
  // The device's JSON schema doesn't know about _id - it's a
  // client-only field used to track task identity across days while
  // editing. Strip it before sending so the payload matches what the
  // ESP32 expects.
  const payload = JSON.parse(JSON.stringify(data));
  payload.kids.forEach(k => k.week.forEach(day => day.tasks.forEach(t => { delete t._id; })));

  const r = await fetch('/api/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const res = await r.json();
  if (res.ok) showToast();
  else showGlobalStatus(S.saveError, false);
}

// ── Tabs ──────────────────────────────────────────

function switchTab(btn, tab) {
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  ['plan','kids','rewards','misc'].forEach(t => {
    document.getElementById('tab-'+t).style.display = t===tab ? '' : 'none';
  });
}

// ── Helpers ───────────────────────────────────────

function showToast() {
  const t = document.getElementById('toast');
  t.style.display = 'block';
  setTimeout(() => t.style.display = 'none', 2500);
}

function showGlobalStatus(msg, ok) {
  const s = document.getElementById('globalStatus');
  s.className = 'status-msg ' + (ok ? 'ok' : 'err');
  s.textContent = msg;
  s.style.display = 'block';
  setTimeout(() => s.style.display = 'none', 3000);
}

function colorHex(rgb565) {
  const r = ((rgb565 >> 11) & 0x1F) << 3;
  const g = ((rgb565 >> 5)  & 0x3F) << 2;
  const b = (rgb565 & 0x1F) << 3;
  return ((r<<16)|(g<<8)|b).toString(16).padStart(6,'0');
}

function hexToRgb565(hex) {
  const n = parseInt(hex.slice(1), 16);
  const r = (n >> 16) & 0xFF, g = (n >> 8) & 0xFF, b = n & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function escAttr(s) {
  return String(s).replace(/&/g,'&amp;').replace(/'/g,'&#39;').replace(/"/g,'&quot;');
}

load();
</script>
</body>
</html>
)rawhtml";

const char HTML_WIFI[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KidsBoard WiFi Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0f0f1a;color:#fff;font-family:system-ui,sans-serif;padding:20px;display:flex;flex-direction:column;align-items:center;min-height:100vh;justify-content:center}
.card{background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:12px;padding:24px;width:100%;max-width:400px}
h1{color:#4ecdc4;margin-bottom:8px;font-size:1.4rem}
p{color:rgba(255,255,255,0.5);font-size:0.9rem;margin-bottom:20px}
label{display:block;color:rgba(255,255,255,0.6);font-size:0.85rem;margin-bottom:6px}
input{width:100%;background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.1);border-radius:8px;padding:10px 14px;color:#fff;font-size:1rem;margin-bottom:16px}
input:focus{outline:none;border-color:#4ecdc4}
button{width:100%;background:#4ecdc4;border:none;color:#0f0f1a;border-radius:10px;padding:12px;font-weight:700;cursor:pointer;font-size:1rem}
button:hover{background:#45b7b0}
.status{margin-top:16px;padding:12px;border-radius:8px;font-size:0.85rem;display:none}
.status.ok{background:rgba(0,200,0,0.1);color:#4ade80;border:1px solid rgba(0,200,0,0.2)}
.status.err{background:rgba(255,0,0,0.1);color:#f87171;border:1px solid rgba(255,0,0,0.2)}
</style>
</head>
<body>
<div class="card">
  <h1>📶 WiFi Setup</h1>
  <p>Connect KidsBoard to your home network.</p>
  <label>Network name (SSID)</label>
  <input type="text" id="ssid" placeholder="MyNetwork">
  <label>Password</label>
  <input type="password" id="pass" placeholder="••••••••">
  <button onclick="save()">Save &amp; Connect</button>
  <div class="status" id="status"></div>
</div>
<script>
async function save() {
  const ssid = document.getElementById('ssid').value.trim();
  const pass = document.getElementById('pass').value;
  if (!ssid) { showStatus('Please enter a network name', false); return; }
  const r = await fetch('/api/wifi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, pass })
  });
  const res = await r.json();
  if (res.ok) {
    showStatus('Saved! KidsBoard will restart and connect to ' + ssid, true);
    setTimeout(() => location.reload(), 5000);
  } else {
    showStatus('Error saving credentials', false);
  }
}
function showStatus(msg, ok) {
  const s = document.getElementById('status');
  s.className = 'status ' + (ok ? 'ok' : 'err');
  s.textContent = msg;
  s.style.display = 'block';
}
</script>
</body>
</html>
)rawhtml";

// ── Server setup ──────────────────────────────────

void setupWebserver(AsyncWebServer &server, AppState &state, TFT_eSPI &tft) {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", HTML_INDEX);
  });

  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", HTML_WIFI);
  });

  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, data, len);
      String ssid = doc["ssid"] | "";
      String pass = doc["pass"] | "";
      if (ssid.length() > 0) {
        saveWiFiCredentials(ssid, pass);
        req->send(200, "application/json", "{\"ok\":true}");
        delay(1000);
        ESP.restart();
      } else {
        req->send(400, "application/json", "{\"ok\":false}");
      }
    });

  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
    String key = loadApiKey();
    String model = loadAnthropicModel();
    DynamicJsonDocument doc(256);
    doc["hasKey"] = key.length() > 0;
    doc["model"]  = model;
    String resp;
    serializeJson(doc, resp);
    req->send(200, "application/json", resp);
  });

  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, data, len);
      String key   = doc["anthropicKey"]   | "";
      String model = doc["anthropicModel"] | "";
      if (key.length() > 0 || model.length() > 0) {
        saveSettings(key, model);
        req->send(200, "application/json", "{\"ok\":true}");
      } else {
        req->send(400, "application/json", "{\"ok\":false}");
      }
    });

  server.on("/api/data", HTTP_GET, [&state](AsyncWebServerRequest *req) {
    DynamicJsonDocument doc(16384);
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
        JsonObject day  = week.createNestedObject();
        JsonArray tasks = day.createNestedArray("tasks");
        for (int t = 0; t < state.kids[i].week[d].taskCount; t++) {
          JsonObject task = tasks.createNestedObject();
          task["name"]      = state.kids[i].week[d].tasks[t].name;
          task["done"]      = state.kids[i].week[d].tasks[t].done;
          task["quizTopic"] = state.kids[i].week[d].tasks[t].quizTopic;
        }
      }
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/save", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [&state](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      static uint8_t bodyBuf[16384];
      static size_t bodyLen = 0;
      if (index == 0) bodyLen = 0;
      if (bodyLen + len < sizeof(bodyBuf)) { memcpy(bodyBuf + bodyLen, data, len); bodyLen += len; }
      else { req->send(400, "application/json", "{\"ok\":false}"); return; }
      if (index + len < total) return;

      DynamicJsonDocument doc(16384);
      if (deserializeJson(doc, bodyBuf, bodyLen)) { req->send(400, "application/json", "{\"ok\":false}"); return; }

      JsonArray kids = doc["kids"];
      state.kidCount = 0;
      for (JsonObject k : kids) {
        if (state.kidCount >= MAX_KIDS) break;
        int i = state.kidCount++;
        strlcpy(state.kids[i].name,    k["name"] | "Kind",        24);
        strlcpy(state.kids[i].rfidUID, k["rfid"] | "00:00:00:00", 16);
        state.kids[i].color  = k["color"]  | COLOR_KID_0;
        state.kids[i].active = k["active"] | true;

        state.kids[i].rewardCount = 0;
        JsonArray rewards = k["rewards"];
        if (rewards) {
          for (JsonObject r : rewards) {
            if (state.kids[i].rewardCount >= 3) break;
            int ri = state.kids[i].rewardCount++;
            strlcpy(state.kids[i].rewards[ri].name, r["name"] | "Belohnung", 32);
            strlcpy(state.kids[i].rewards[ri].type, r["type"] | "min",       8);
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
            strlcpy(state.kids[i].week[d].tasks[ti].name,      t["name"] | "Aufgabe", 32);
            state.kids[i].week[d].tasks[ti].done = t["done"] | false;
            strlcpy(state.kids[i].week[d].tasks[ti].quizTopic, t["quizTopic"] | "", MAX_QUIZ_TOPIC_LEN);
          }
        }
      }

      saveData(state);
      cleanupOrphanedQuizFiles(state);
      req->send(200, "application/json", "{\"ok\":true}");
    });

  // Load quiz questions for the modal
  server.on("/api/quiz/load", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!req->hasParam("kid") || !req->hasParam("day") || !req->hasParam("task")) {
      req->send(400, "application/json", "{\"ok\":false}");
      return;
    }
    int ki = req->getParam("kid")->value().toInt();
    int di = req->getParam("day")->value().toInt();
    int ti = req->getParam("task")->value().toInt();

    QuizQuestion questions[MAX_QUIZ_QUESTIONS];
    int count = loadQuizQuestions(ki, di, ti, questions, MAX_QUIZ_QUESTIONS);

    DynamicJsonDocument doc(8192);
    doc["ok"] = true;
    JsonArray arr = doc.createNestedArray("questions");
    for (int i = 0; i < count; i++) {
      JsonObject q = arr.createNestedObject();
      q["q"] = questions[i].question;
      q["c"] = questions[i].correctIndex;
      JsonArray ans = q.createNestedArray("a");
      for (int j = 0; j < 4; j++) ans.add(questions[i].answers[j].text);
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/quiz/save", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      static uint8_t buf[8192];
      static size_t bufLen = 0;
      if (index == 0) bufLen = 0;
      if (bufLen + len < sizeof(buf)) { memcpy(buf + bufLen, data, len); bufLen += len; }
      if (index + len < total) return;

      DynamicJsonDocument doc(8192);
      if (deserializeJson(doc, buf, bufLen)) { req->send(400, "application/json", "{\"ok\":false}"); return; }

      int ki = doc["kid"] | 0;
      int di = doc["day"] | 0;
      int ti = doc["task"] | 0;

      QuizQuestion questions[MAX_QUIZ_QUESTIONS];
      int count = 0;
      for (JsonObject q : doc["questions"].as<JsonArray>()) {
        if (count >= MAX_QUIZ_QUESTIONS) break;
        strlcpy(questions[count].question, q["q"] | "", MAX_QUESTION_LEN);
        questions[count].correctIndex = q["c"] | 0;
        JsonArray answers = q["a"];
        for (int j = 0; j < 4 && j < (int)answers.size(); j++)
          strlcpy(questions[count].answers[j].text, answers[j] | "", MAX_ANSWER_LEN);
        count++;
      }

      bool ok = saveQuizQuestions(ki, di, ti, questions, count);
      req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    });

  server.on("/api/quiz/generate", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      static uint8_t buf[512];
      static size_t bufLen = 0;
      if (index == 0) bufLen = 0;
      if (bufLen + len < sizeof(buf)) { memcpy(buf + bufLen, data, len); bufLen += len; }
      if (index + len < total) return;

      DynamicJsonDocument doc(512);
      if (deserializeJson(doc, buf, bufLen)) { req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad request\"}"); return; }

      String apiKey = loadApiKey();
      if (apiKey.isEmpty()) { req->send(400, "application/json", "{\"ok\":false,\"error\":\"no api key\"}"); return; }

      String topic = doc["topic"] | "";
      if (topic.isEmpty()) { req->send(400, "application/json", "{\"ok\":false,\"error\":\"no topic\"}"); return; }

      int ki = doc["kid"] | 0;
      int di = doc["day"] | 0;
      int ti = doc["task"] | 0;

      QuizQuestion questions[MAX_QUIZ_QUESTIONS];
      int count = callAnthropicForQuiz(apiKey, topic, questions, MAX_QUIZ_QUESTIONS);

      if (count <= 0) { req->send(500, "application/json", "{\"ok\":false,\"error\":\"generation failed\"}"); return; }

      saveQuizQuestions(ki, di, ti, questions, count);

      DynamicJsonDocument respDoc(8192);
      respDoc["ok"] = true;
      JsonArray qArr = respDoc.createNestedArray("questions");
      for (int i = 0; i < count; i++) {
        JsonObject q = qArr.createNestedObject();
        q["q"]       = questions[i].question;
        q["correct"] = questions[i].correctIndex;
        JsonArray ans = q.createNestedArray("answers");
        for (int j = 0; j < 4; j++) ans.add(questions[i].answers[j].text);
      }
      String out;
      serializeJson(respDoc, out);
      req->send(200, "application/json", out);
    });

  server.on("/api/assign-rfid", HTTP_GET, [&state, &tft](AsyncWebServerRequest *req) {
    if (!req->hasParam("kid")) { req->send(400, "application/json", "{\"error\":\"missing kid\"}"); return; }
    int ki = req->getParam("kid")->value().toInt();
    state.rfidAssignPending   = true;
    state.rfidAssignKid       = ki;
    state.rfidAssignUID       = "";
    state.rfidAssignStartTime = millis();
    req->send(200, "application/json", "{\"ok\":true,\"waiting\":true}");
  });

  server.on("/api/assign-rfid-result", HTTP_GET, [&state](AsyncWebServerRequest *req) {
    if (state.rfidAssignUID.length() > 0) {
      String resp = "{\"uid\":\"" + state.rfidAssignUID + "\"}";
      state.rfidAssignUID       = "";
      state.rfidAssignStartTime = millis();
      req->send(200, "application/json", resp);
    } else if (!state.rfidAssignPending) {
      req->send(200, "application/json", "{\"uid\":null,\"timeout\":true}");
    } else {
      req->send(200, "application/json", "{\"uid\":null}");
    }
  });

  server.on("/api/reset", HTTP_POST, [&state](AsyncWebServerRequest *req) {
    for (int i = 0; i < state.kidCount; i++)
      for (int d = 0; d < DAYS_COUNT; d++)
        for (int t = 0; t < state.kids[i].week[d].taskCount; t++)
          state.kids[i].week[d].tasks[t].done = false;
    saveData(state);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not found");
  });
}