// =====================================================
//  webserver.h – Web-Interface + REST API
// =====================================================
#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "data.h"
#include "wifi_manager.h"

const char HTML_INDEX[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KidsBoard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #0f0f1a; color: #fff; font-family: system-ui, sans-serif; padding: 20px; }
  h1 { color: #4ecdc4; margin-bottom: 24px; }
  h2 { font-size: 1rem; color: rgba(255,255,255,0.5); margin: 20px 0 10px; text-transform: uppercase; letter-spacing: 2px; }
  .card { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; padding: 16px; margin-bottom: 16px; }
  .kid-header { display: flex; align-items: center; gap: 12px; margin-bottom: 16px; }
  .kid-dot { width: 12px; height: 12px; border-radius: 50%; }
  .kid-name { font-size: 1.1rem; font-weight: 700; }
  .day-tabs { display: flex; gap: 6px; margin-bottom: 12px; flex-wrap: wrap; }
  .tab { padding: 6px 12px; border-radius: 8px; border: 1px solid rgba(255,255,255,0.1); background: transparent; color: rgba(255,255,255,0.4); cursor: pointer; font-size: 0.85rem; }
  .tab.active { color: #fff; background: rgba(255,255,255,0.1); }
  .task-list { display: flex; flex-direction: column; gap: 6px; }
  .task-row { display: flex; gap: 8px; }
  .reward-row { display: flex; gap: 8px; align-items: center; margin-bottom: 6px; }
  .reward-row label { font-size: .8rem; color: rgba(255,255,255,.4); width: 60px; flex-shrink: 0; }
  .task-input { flex: 1; background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px 12px; color: #fff; font-size: 0.9rem; }
  .mins-input { width: 80px; background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px 12px; color: #fff; font-size: 0.9rem; text-align: right; }
  .task-input:focus, .mins-input:focus { outline: none; border-color: #4ecdc4; }
  .btn-del { background: rgba(255,60,60,0.2); border: 1px solid rgba(255,60,60,0.3); color: #ff6b6b; border-radius: 8px; padding: 8px 12px; cursor: pointer; }
  .btn-add { background: rgba(78,205,196,0.1); border: 1px dashed rgba(78,205,196,0.3); color: #4ecdc4; border-radius: 8px; padding: 8px; width: 100%; cursor: pointer; margin-top: 6px; }
  .btn-save { background: #4ecdc4; border: none; color: #0f0f1a; border-radius: 10px; padding: 12px 24px; font-weight: 700; cursor: pointer; font-size: 1rem; margin-top: 16px; }
  .btn-save:hover { background: #45b7b0; }
  .rfid-box { background: rgba(255,230,100,0.08); border: 1px solid rgba(255,230,100,0.2); border-radius: 10px; padding: 12px; margin-top: 12px; }
  .rfid-uid { font-family: monospace; color: #ffe066; font-size: 0.9rem; }
  .status { padding: 12px; border-radius: 8px; margin-top: 12px; font-size: 0.85rem; }
  .status.ok { background: rgba(0,200,0,0.1); color: #4ade80; border: 1px solid rgba(0,200,0,0.2); }
  .status.err { background: rgba(255,0,0,0.1); color: #f87171; border: 1px solid rgba(255,0,0,0.2); }
  .reset-btn { background: rgba(255,100,100,0.1); border: 1px solid rgba(255,100,100,0.3); color: #ff6b6b; border-radius: 10px; padding: 10px 20px; cursor: pointer; margin-top: 8px; }
  .checkbox-row { display: flex; align-items: center; gap: 8px; margin-bottom: 10px; }
  .checkbox-row input { width: 16px; height: 16px; cursor: pointer; }
  .checkbox-row label { color: rgba(255,255,255,.6); font-size: .9rem; cursor: pointer; }
  .mins-suffix { color: rgba(255,255,255,.3); font-size: .8rem; }
</style>
</head>
<body>
<h1>🗓 KidsBoard</h1>
<div id="app">Loading...</div>

<script>
const STRINGS = {
  showInPlanner:    'Im Planer anzeigen',
  rfidCard:         'RFID Karte',
  rfidNotAssigned:  'Noch nicht zugewiesen',
  rfidAssign:       'Nächste Karte zuweisen',
  rewards:          '🏆 Belohnungen',
  addReward:        '+ Belohnung hinzufügen',
  noRewards:        'Keine Belohnungen definiert',
  rewardPlaceholder:'z.B. iPad',
  rewardTypeMin:    'Minuten',
  rewardTypePcs:    'Stueck',
  rewardTypeEur:    'EUR (Cent)',
  rewardTypeTxt:    'Freitext (5★)',
  rewardTypeMys:    'Mystery (5★)',
  newRewardName:    'Neue Belohnung',
  tasks:            'Aufgaben',
  addTask:          '+ Aufgabe hinzufügen',
  newTaskName:      'Neue Aufgabe',
  save:             '💾 Speichern',
  saved:            'Gespeichert!',
  saveError:        'Fehler!',
  addKid:           '+ Kind hinzufügen',
  newKidName:       'Neues Kind',
  newRewardDefault: 'Belohnung',
  resetWeekTitle:   '⚠️ Woche zurücksetzen',
  resetWeekDesc:    'Setzt alle Häkchen der aktuellen Woche zurück.',
  resetWeekBtn:     'Alle Aufgaben zurücksetzen',
  resetWeekConfirm: 'Alle Häkchen zurücksetzen?',
  resetWeekDone:    'Woche zurückgesetzt!',
  resetWeekError:   'Fehler',
  rfidScanNow:      'Karte jetzt auflegen...',
  rfidAssigned:     'Karte zugewiesen:',
  rfidNoCard:       'Keine Karte erkannt. Nochmal versuchen.',
  rfidTimeout:      'Timeout. Nochmal versuchen.',
};

const DAYS = ['Mo','Di','Mi','Do','Fr'];

let data = null;
let activeDays = {};

async function load() {
  const r = await fetch('/api/data');
  data = await r.json();
  render();
}

function render() {
  const app = document.getElementById('app');
  app.innerHTML = '';

  data.kids.forEach((kid, ki) => {
    if (!activeDays[ki]) activeDays[ki] = 0;
    const ad = activeDays[ki];

    const card = document.createElement('div');
    card.className = 'card';
    card.innerHTML = `
      <div class="kid-header">
        <div class="kid-dot" style="background:#${colorHex(kid.color)}"></div>
        <div class="kid-name">${kid.name}</div>
      </div>

      <div class="checkbox-row">
        <input type="checkbox" id="active-${ki}" ${kid.active ? 'checked' : ''}
          onchange="data.kids[${ki}].active=this.checked">
        <label for="active-${ki}">${STRINGS.showInPlanner}</label>
      </div>

      <label style="font-size:.8rem;color:rgba(255,255,255,.4)">${STRINGS.rfidCard}</label>
      <div class="rfid-box">
        <div class="rfid-uid">${kid.rfid || STRINGS.rfidNotAssigned}</div>
        <button onclick="assignRFID(${ki})" style="margin-top:8px;background:rgba(255,230,100,0.1);border:1px solid rgba(255,230,100,0.3);color:#ffe066;border-radius:8px;padding:6px 12px;cursor:pointer;font-size:.8rem">
          ${STRINGS.rfidAssign}
        </button>
      </div>

      <h2>${STRINGS.rewards}</h2>
      <div id="rewards-${ki}">
        ${renderRewards(ki)}
      </div>
      ${(kid.rewards||[]).length < 3 ? `<button class="btn-add" onclick="addReward(${ki})">${STRINGS.addReward}</button>` : ''}

      <h2>${STRINGS.tasks}</h2>
      <div class="day-tabs" id="tabs-${ki}">
        ${DAYS.map((d,di)=>`<button class="tab ${di===ad?'active':''}" onclick="setDay(${ki},${di})">${d}</button>`).join('')}
      </div>

      <div class="task-list" id="tasks-${ki}">
        ${renderTasks(ki, ad)}
      </div>
      <button class="btn-add" onclick="addTask(${ki})">${STRINGS.addTask}</button>

      <div style="margin-top:16px">
        <button class="btn-save" onclick="save()">${STRINGS.save}</button>
      </div>
    `;
    app.appendChild(card);
  });

  // Neues Kind
  const addKidCard = document.createElement('div');
  addKidCard.className = 'card';
  addKidCard.innerHTML = `<button class="btn-add" style="margin-top:0" onclick="addKid()">${STRINGS.addKid}</button>`;
  app.appendChild(addKidCard);

  // Woche zurücksetzen
  const resetCard = document.createElement('div');
  resetCard.className = 'card';
  resetCard.innerHTML = `
    <h2>${STRINGS.resetWeekTitle}</h2>
    <p style="color:rgba(255,255,255,.4);font-size:.85rem;margin-bottom:8px">${STRINGS.resetWeekDesc}</p>
    <button class="reset-btn" onclick="resetWeek()">${STRINGS.resetWeekBtn}</button>
  `;
  app.appendChild(resetCard);

  const statusDiv = document.createElement('div');
  statusDiv.id = 'status';
  app.appendChild(statusDiv);
}

function renderRewards(ki) {
  const rewards = data.kids[ki].rewards || [];
  if (rewards.length === 0) return `<p style="color:rgba(255,255,255,.3);font-size:.85rem">${STRINGS.noRewards}</p>`;
  return rewards.map((r, ri) => {
    const isTxt = r.type === 'txt';
    const isMys = r.type === 'mys';
    return `
    <div class="reward-row" style="flex-wrap:wrap;gap:6px">
      <input class="task-input" value="${r.name}"
        onchange="data.kids[${ki}].rewards[${ri}].name=this.value"
        placeholder="${STRINGS.rewardPlaceholder}" style="min-width:100px">
      <select class="mins-input" style="width:120px;background:#1a1a2e;color:#fff;border:1px solid rgba(255,255,255,0.2)"
        onchange="data.kids[${ki}].rewards[${ri}].type=this.value;render()">
        <option value="min" ${r.type==='min'?'selected':''}>${STRINGS.rewardTypeMin}</option>
        <option value="pcs" ${r.type==='pcs'?'selected':''}>${STRINGS.rewardTypePcs}</option>
        <option value="eur" ${r.type==='eur'?'selected':''}>${STRINGS.rewardTypeEur}</option>
        <option value="txt" ${r.type==='txt'?'selected':''}>${STRINGS.rewardTypeTxt}</option>
        <option value="mys" ${r.type==='mys'?'selected':''}>${STRINGS.rewardTypeMys}</option>
      </select>
      ${(!isTxt && !isMys) ? `<input class="mins-input" type="number" value="${r.maxValue||0}"
        onchange="data.kids[${ki}].rewards[${ri}].maxValue=parseInt(this.value)"
        min="0" max="9999" placeholder="Max">
      <span class="mins-suffix">${r.type==='min'?'min':r.type==='pcs'?'Stk':r.type==='eur'?'ct':''}</span>` : ''}
      <button class="btn-del" onclick="removeReward(${ki},${ri})">✕</button>
    </div>
  `}).join('');
}

function addReward(ki) {
  if (!data.kids[ki].rewards) data.kids[ki].rewards = [];
  data.kids[ki].rewards.push({ name: STRINGS.newRewardName, type: 'min', maxValue: 60 });
  document.getElementById(`rewards-${ki}`).innerHTML = renderRewards(ki);
}

function removeReward(ki, ri) {
  data.kids[ki].rewards.splice(ri, 1);
  document.getElementById(`rewards-${ki}`).innerHTML = renderRewards(ki);
}

function renderTasks(ki, di) {
  const tasks = data.kids[ki].week[di]?.tasks || [];
  return tasks.map((t, ti) => `
    <div class="task-row">
      <input class="task-input" value="${t.name}"
        onchange="data.kids[${ki}].week[${di}].tasks[${ti}].name=this.value">
      <button class="btn-del" onclick="removeTask(${ki},${di},${ti})">✕</button>
    </div>
  `).join('');
}

function setDay(ki, di) {
  activeDays[ki] = di;
  document.querySelectorAll(`#tabs-${ki} .tab`).forEach((t,i) => {
    t.className = 'tab' + (i===di?' active':'');
  });
  document.getElementById(`tasks-${ki}`).innerHTML = renderTasks(ki, di);
}

function addTask(ki) {
  const di = activeDays[ki];
  if (!data.kids[ki].week[di]) data.kids[ki].week[di] = { tasks: [] };
  data.kids[ki].week[di].tasks.push({ name: STRINGS.newTaskName, done: false });
  document.getElementById(`tasks-${ki}`).innerHTML = renderTasks(ki, di);
}

function removeTask(ki, di, ti) {
  data.kids[ki].week[di].tasks.splice(ti, 1);
  document.getElementById(`tasks-${ki}`).innerHTML = renderTasks(ki, di);
}

function addKid() {
  const colors = [63519, 2047, 65504, 63488];
  data.kids.push({
    name: STRINGS.newKidName,
    rfid: '', color: colors[data.kids.length % colors.length],
    active: true, rewards: [{ name: STRINGS.newRewardDefault, type: 'min', maxValue: 60 }],
    week: Array(5).fill(null).map(() => ({ tasks: [] }))
  });
  render();
}

async function save() {
  const r = await fetch('/api/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  });
  const res = await r.json();
  showStatus(res.ok ? STRINGS.saved : STRINGS.saveError, res.ok);
}

async function assignRFID(ki) {
  showStatus(STRINGS.rfidScanNow, true);
  await fetch(`/api/assign-rfid?kid=${ki}`);
  for (let i = 0; i < 20; i++) {
    await new Promise(r => setTimeout(r, 500));
    const r = await fetch('/api/assign-rfid-result');
    const res = await r.json();
    if (res.uid) {
      data.kids[ki].rfid = res.uid;
      showStatus(`${STRINGS.rfidAssigned} ${res.uid}`, true);
      render();
      return;
    }
    if (res.timeout) {
      showStatus(STRINGS.rfidNoCard, false);
      return;
    }
  }
  showStatus(STRINGS.rfidTimeout, false);
}

async function resetWeek() {
  if (!confirm(STRINGS.resetWeekConfirm)) return;
  const r = await fetch('/api/reset', { method: 'POST' });
  const res = await r.json();
  showStatus(res.ok ? STRINGS.resetWeekDone : STRINGS.resetWeekError, res.ok);
  load();
}

function showStatus(msg, ok) {
  const s = document.getElementById('status');
  if (s) { s.className = 'status ' + (ok ? 'ok' : 'err'); s.textContent = msg; }
}

function colorHex(rgb565) {
  const r = ((rgb565 >> 11) & 0x1F) << 3;
  const g = ((rgb565 >> 5)  & 0x3F) << 2;
  const b = (rgb565 & 0x1F) << 3;
  return ((r<<16)|(g<<8)|b).toString(16).padStart(6,'0');
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
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #0f0f1a; color: #fff; font-family: system-ui, sans-serif;
         padding: 20px; display: flex; flex-direction: column; align-items: center; min-height: 100vh; justify-content: center; }
  .card { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1);
          border-radius: 12px; padding: 24px; width: 100%; max-width: 400px; }
  h1 { color: #4ecdc4; margin-bottom: 8px; font-size: 1.4rem; }
  p { color: rgba(255,255,255,0.5); font-size: 0.9rem; margin-bottom: 20px; }
  label { display: block; color: rgba(255,255,255,0.6); font-size: 0.85rem; margin-bottom: 6px; }
  input { width: 100%; background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1);
          border-radius: 8px; padding: 10px 14px; color: #fff; font-size: 1rem; margin-bottom: 16px; }
  input:focus { outline: none; border-color: #4ecdc4; }
  button { width: 100%; background: #4ecdc4; border: none; color: #0f0f1a;
           border-radius: 10px; padding: 12px; font-weight: 700; cursor: pointer; font-size: 1rem; }
  button:hover { background: #45b7b0; }
  .status { margin-top: 16px; padding: 12px; border-radius: 8px; font-size: 0.85rem; display: none; }
  .status.ok  { background: rgba(0,200,0,0.1); color: #4ade80; border: 1px solid rgba(0,200,0,0.2); }
  .status.err { background: rgba(255,0,0,0.1); color: #f87171; border: 1px solid rgba(255,0,0,0.2); }
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
  <button onclick="save()">Save & Connect</button>
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

void setupWebserver(AsyncWebServer& server, AppState& state, TFT_eSPI& tft) {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML_INDEX);
  });

  // WiFi setup page (accessible in AP mode)
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML_WIFI);
  });

  // Save WiFi credentials and restart
  server.on("/api/wifi", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
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
    }
  );

  server.on("/api/data", HTTP_GET, [&state](AsyncWebServerRequest* req) {
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
        robj["name"]    = state.kids[i].rewards[r].name;
        robj["type"]     = state.kids[i].rewards[r].type;
        robj["maxValue"] = state.kids[i].rewards[r].maxValue;
      }

      JsonArray week = k.createNestedArray("week");
      for (int d = 0; d < DAYS_COUNT; d++) {
        JsonObject day  = week.createNestedObject();
        JsonArray tasks = day.createNestedArray("tasks");
        for (int t = 0; t < state.kids[i].week[d].taskCount; t++) {
          JsonObject task = tasks.createNestedObject();
          task["name"] = state.kids[i].week[d].tasks[t].name;
          task["done"] = state.kids[i].week[d].tasks[t].done;
        }
      }
    }

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/save", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [&state](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      static uint8_t bodyBuf[16384];
      static size_t  bodyLen = 0;

      if (index == 0) bodyLen = 0;
      if (bodyLen + len < sizeof(bodyBuf)) {
        memcpy(bodyBuf + bodyLen, data, len);
        bodyLen += len;
      } else {
        req->send(400, "application/json", "{\"ok\":false}");
        return;
      }
      if (index + len < total) return;

      DynamicJsonDocument doc(16384);
      DeserializationError err = deserializeJson(doc, bodyBuf, bodyLen);
      if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        req->send(400, "application/json", "{\"ok\":false}");
        return;
      }

      JsonArray kids = doc["kids"];
      state.kidCount = 0;
      for (JsonObject k : kids) {
        if (state.kidCount >= MAX_KIDS) break;
        int i = state.kidCount++;
        strlcpy(state.kids[i].name,    k["name"]  | "Kind",        24);
        strlcpy(state.kids[i].rfidUID, k["rfid"]  | "00:00:00:00", 16);
        state.kids[i].color  = k["color"]  | COLOR_KID_0;
        state.kids[i].active = k["active"] | true;

        // Belohnungen
        state.kids[i].rewardCount = 0;
        JsonArray rewards = k["rewards"];
        if (rewards) {
          for (JsonObject r : rewards) {
            if (state.kids[i].rewardCount >= 3) break;
            int ri = state.kids[i].rewardCount++;
            strlcpy(state.kids[i].rewards[ri].name, r["name"] | "Belohnung", 32);
            strlcpy(state.kids[i].rewards[ri].type, r["type"] | "min", 8);
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
            strlcpy(state.kids[i].week[d].tasks[ti].name, t["name"] | "Aufgabe", 32);
            state.kids[i].week[d].tasks[ti].done = t["done"] | false;
          }
        }
      }
      saveData(state);
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  server.on("/api/assign-rfid", HTTP_GET, [&state, &tft](AsyncWebServerRequest* req) {
    if (!req->hasParam("kid")) {
      req->send(400, "application/json", "{\"error\":\"missing kid\"}");
      return;
    }
    int ki = req->getParam("kid")->value().toInt();
    state.rfidAssignPending = true;
    state.rfidAssignKid = ki;
    state.rfidAssignUID = "";
    state.rfidAssignStartTime = millis();
    Serial.printf("RFID assign pending fuer Kid %d\n", ki);

    req->send(200, "application/json", "{\"ok\":true,\"waiting\":true}");
  });

  server.on("/api/assign-rfid-result", HTTP_GET, [&state](AsyncWebServerRequest* req) {
    if (state.rfidAssignUID.length() > 0) {
      String resp = "{\"uid\":\"" + state.rfidAssignUID + "\"}";
      state.rfidAssignUID = "";
    state.rfidAssignStartTime = millis();
      req->send(200, "application/json", resp);
    } else if (!state.rfidAssignPending) {
      req->send(200, "application/json", "{\"uid\":null,\"timeout\":true}");
    } else {
      req->send(200, "application/json", "{\"uid\":null}");
    }
  });

  server.on("/api/reset", HTTP_POST, [&state](AsyncWebServerRequest* req) {
    for (int i = 0; i < state.kidCount; i++)
      for (int d = 0; d < DAYS_COUNT; d++)
        for (int t = 0; t < state.kids[i].week[d].taskCount; t++)
          state.kids[i].week[d].tasks[t].done = false;
    saveData(state);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Not found");
  });
}