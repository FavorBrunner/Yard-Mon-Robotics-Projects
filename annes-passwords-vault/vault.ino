/*
  Anne's Passwords — ESP32 Password Vault
  =========================================
  BEST VERSION — Full-featured with:
    - Wi-Fi captive portal for add/edit/delete entries
    - RFID (MFRC522) key card lock/unlock
    - Joystick navigation + LCD display
    - NVS persistent storage (survives power cycles)
    - Alphabetical sorting
    - Flicker-free marquee scrolling on LCD
    - Robust double-click to exit viewing mode
    - Long-press (3s) to re-enable portal

  Hardware:
    LCD I2C: SDA=21, SCL=22 (address 0x27)
    Joystick: VRX=32, VRY=33, SW=4
    RFID RC522: MOSI=23, MISO=19, SCK=18, SDA(SS)=5, RST=0
    Wi-Fi AP: "Anne's Passwords" / password: "anne123"
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define JOY_VRX   32
#define JOY_VRY   33
#define JOY_SW    4
#define RFID_SDA  5
#define RFID_RST  0

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 mfrc522(RFID_SDA, RFID_RST);

static const char* AP_SSID = "Anne's Passwords";
static const char* AP_PASS = "anne123";
const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

Preferences prefs;
static const int MAX_ENTRIES = 30;

struct Entry { String app; String user; String pass; };
Entry entries[MAX_ENTRIES];
int entryCount = 0;

bool unlockedMenu = false;
bool viewing = false;
int menuIndex = 0;
int viewingIndex = -1;
bool portalEnabled = true;
String lastNotice = "";

const int CENTER_LOW = 1600, CENTER_HIGH = 2500;
const int MOVE_DEBOUNCE_MS = 160;
unsigned long lastMoveTime = 0;

bool lastBtnReading = HIGH, stableBtnState = HIGH;
unsigned long lastBtnChangeMs = 0;
const unsigned long DEBOUNCE_MS = 35;

int clickCount = 0;
unsigned long firstClickMs = 0;
const unsigned long DOUBLE_CLICK_WINDOW = 520;

bool longPressFired = false;
unsigned long pressStartMs = 0;
const unsigned long LONG_PRESS_MS = 3000;

unsigned long lastCardToggleMs = 0;
const unsigned long CARD_TOGGLE_COOLDOWN_MS = 1200;

enum ViewMode { SHOW_USERNAME, SHOW_PASSWORD };
ViewMode viewMode = SHOW_USERNAME;

unsigned long lastScrollMs = 0;
const unsigned long SCROLL_INTERVAL_MS = 280;
int scrollOffset = 0;
String lastScrollKey = "";

static bool btnWasDown = false;
static unsigned long btnDownAtMs = 0;

String keyApp(int i)  { return "a" + String(i); }
String keyUser(int i) { return "u" + String(i); }
String keyPass(int i) { return "p" + String(i); }

String lowerCopy(const String& s) { String t = s; t.toLowerCase(); return t; }

bool appLess(const Entry& a, const Entry& b) {
  String A = lowerCopy(a.app), B = lowerCopy(b.app);
  if (A == B) return lowerCopy(a.user) < lowerCopy(b.user);
  return A < B;
}

void sortEntriesInRAM() {
  for (int i = 1; i < entryCount; i++) {
    Entry key = entries[i]; int j = i - 1;
    while (j >= 0 && appLess(key, entries[j])) { entries[j + 1] = entries[j]; j--; }
    entries[j + 1] = key;
  }
  if (entryCount == 0) { menuIndex = 0; viewing = false; viewingIndex = -1; }
  else { if (menuIndex >= entryCount) menuIndex = entryCount - 1; if (menuIndex < 0) menuIndex = 0; if (viewing) { viewing = false; viewingIndex = -1; } }
}

void loadEntriesFromNVS() {
  prefs.begin("vault", false);
  entryCount = prefs.getInt("count", 0);
  if (entryCount < 0) entryCount = 0;
  if (entryCount > MAX_ENTRIES) entryCount = MAX_ENTRIES;
  for (int i = 0; i < entryCount; i++) {
    entries[i].app  = prefs.getString(keyApp(i).c_str(), "");
    entries[i].user = prefs.getString(keyUser(i).c_str(), "");
    entries[i].pass = prefs.getString(keyPass(i).c_str(), "");
  }
  prefs.end();
  sortEntriesInRAM();
}

void saveAllToNVS() {
  prefs.begin("vault", false);
  int oldCount = prefs.getInt("count", 0);
  if (oldCount < 0) oldCount = 0; if (oldCount > MAX_ENTRIES) oldCount = MAX_ENTRIES;
  for (int i = 0; i < oldCount; i++) { prefs.remove(keyApp(i).c_str()); prefs.remove(keyUser(i).c_str()); prefs.remove(keyPass(i).c_str()); }
  for (int i = 0; i < entryCount; i++) { prefs.putString(keyApp(i).c_str(), entries[i].app); prefs.putString(keyUser(i).c_str(), entries[i].user); prefs.putString(keyPass(i).c_str(), entries[i].pass); }
  prefs.putInt("count", entryCount);
  prefs.end();
}

bool addEntry(const String& app, const String& user, const String& pass) {
  if (entryCount >= MAX_ENTRIES) return false;
  entries[entryCount] = {app, user, pass}; entryCount++;
  sortEntriesInRAM(); saveAllToNVS(); return true;
}

bool deleteEntry(int idx) {
  if (idx < 0 || idx >= entryCount) return false;
  for (int i = idx; i < entryCount - 1; i++) entries[i] = entries[i + 1];
  entryCount--; sortEntriesInRAM(); saveAllToNVS();
  if (menuIndex >= entryCount) menuIndex = max(0, entryCount - 1);
  return true;
}

bool editEntry(int idx, const String& app, const String& user, const String& pass) {
  if (idx < 0 || idx >= entryCount) return false;
  entries[idx] = {app, user, pass}; sortEntriesInRAM(); saveAllToNVS(); return true;
}

void lcdBoot() { lcd.clear(); lcd.setCursor(0,0); lcd.print("Anne's Passwords"); lcd.setCursor(0,1); lcd.print("Starting..."); }
void lcdLocked() { lcd.clear(); lcd.setCursor(0,0); lcd.print("Tap key card"); lcd.setCursor(0,1); lcd.print("to unlock menu"); }
void lcdUnlockedHint() { lcd.clear(); lcd.setCursor(0,0); lcd.print("Unlocked"); lcd.setCursor(0,1); lcd.print("Use joystick"); delay(550); }
void lcdEmptyMenuHint() { lcd.clear(); lcd.setCursor(0,0); lcd.print("No passwords"); lcd.setCursor(0,1); lcd.print("Use WiFi portal"); }

void lcdMenuLine(int idx) {
  lcd.clear(); lcd.setCursor(0,0); lcd.print("> ");
  String name = entries[idx].app; if (name.length() > 14) name = name.substring(0, 14);
  lcd.print(name); lcd.setCursor(0,1);
  String hint = String(idx+1) + "/" + String(entryCount) + " press=view";
  if (hint.length() > 16) hint = hint.substring(0, 16); lcd.print(hint);
}

void lcdViewingHeader(const String& appName) {
  lcd.setCursor(0,0); String hdr = appName;
  if (hdr.length() > 16) hdr = hdr.substring(0, 16);
  while (hdr.length() < 16) hdr += " "; lcd.print(hdr);
}

void lcdWriteRow1Fixed(const String& s16) { lcd.setCursor(0,1); lcd.print(s16); }

void lcdViewingLineScrollNoFlicker(const String& label, const String& value) {
  const int lcdCols = 16; int maxLen = lcdCols - (int)label.length(); if (maxLen < 0) maxLen = 0;
  String key = label + "|" + value; unsigned long now = millis();
  if (key != lastScrollKey) { lastScrollKey = key; scrollOffset = 0; lastScrollMs = now; }
  String out = label, window;
  if ((int)value.length() <= maxLen) { window = value; while ((int)window.length() < maxLen) window += " "; }
  else { if (now - lastScrollMs >= SCROLL_INTERVAL_MS) { lastScrollMs = now; scrollOffset++; }
    String scroll = value + "   "; int L = scroll.length(); if (L <= 0) L = 1; scrollOffset %= L;
    window.reserve(maxLen); for (int i = 0; i < maxLen; i++) window += scroll[(scrollOffset + i) % L];
  }
  out += window;
  if ((int)out.length() > lcdCols) out = out.substring(0, lcdCols);
  while ((int)out.length() < lcdCols) out += " ";
  lcdWriteRow1Fixed(out);
}

void showMenuOrEmpty() {
  if (entryCount <= 0) lcdEmptyMenuHint();
  else { if (menuIndex < 0) menuIndex = 0; if (menuIndex >= entryCount) menuIndex = entryCount - 1; lcdMenuLine(menuIndex); }
}

// --- Web portal handlers (HTML generation + CRUD) ---
String htmlEscape(const String& s) { String o=s; o.replace("&","&amp;"); o.replace("<","&lt;"); o.replace(">","&gt;"); o.replace("\"","&quot;"); o.replace("'","&#39;"); return o; }

String portalPage(const String& notice) {
  String page; page.reserve(8000);
  page += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Anne's Passwords</title>";
  page += "<style>body{font-family:system-ui;margin:18px;max-width:900px}.card{border:1px solid #ddd;border-radius:14px;padding:14px;margin:12px 0}input,select{width:100%;padding:12px;border:1px solid #ccc;border-radius:10px;margin:8px 0;font-size:16px}button,a.btn{display:inline-block;text-align:center;padding:12px 14px;border:0;border-radius:10px;font-size:16px;cursor:pointer;text-decoration:none}.row{display:flex;gap:10px;flex-wrap:wrap}.row button{flex:1}.ok{background:#111;color:#fff}.warn{background:#b00020;color:#fff}.ghost{background:#f1f1f1;color:#111}small{color:#666}table{width:100%;border-collapse:collapse}td,th{border-bottom:1px solid #eee;padding:10px;text-align:left}</style></head><body>";
  page += "<h2>Anne's Passwords</h2><p><small>Add, edit, or delete entries. Unlock device with your <b>key card</b>.</small></p>";
  if (notice.length()) page += "<div class='card'><b>" + htmlEscape(notice) + "</b></div>";
  page += "<div class='card'><h3>Add new</h3><form method='POST' action='/add'><label>App</label><input name='app' maxlength='64' required><label>Username</label><input name='user' maxlength='64' required><label>Password</label><input name='pass' maxlength='128' required><div class='row'><button class='ok' type='submit'>Save</button><button class='ghost' type='button' onclick=\"location.href='/done'\">Done</button></div></form></div>";
  page += "<div class='card'><h3>Edit existing</h3>";
  if (entryCount == 0) page += "<p><small>No entries yet.</small></p>";
  else { page += "<form method='POST' action='/edit'><label>Select entry</label><select name='idx' required>";
    for (int i = 0; i < entryCount; i++) page += "<option value='" + String(i) + "'>" + htmlEscape(entries[i].app) + " (" + htmlEscape(entries[i].user) + ")</option>";
    page += "</select><label>New App</label><input name='app' maxlength='64' required><label>New Username</label><input name='user' maxlength='64' required><label>New Password</label><input name='pass' maxlength='128' required><button class='ok' type='submit'>Update</button></form>"; }
  page += "</div><div class='card'><b>Saved entries (" + String(entryCount) + "/" + String(MAX_ENTRIES) + ")</b>";
  if (entryCount == 0) page += "<p><small>No entries saved yet.</small></p>";
  else { page += "<table><tr><th>App</th><th>Username</th><th>Delete</th></tr>";
    for (int i = 0; i < entryCount; i++) page += "<tr><td>" + htmlEscape(entries[i].app) + "</td><td>" + htmlEscape(entries[i].user) + "</td><td><a class='btn warn' href='/delete?idx=" + String(i) + "'>Delete</a></td></tr>";
    page += "</table>"; }
  page += "</div></body></html>"; return page;
}

String donePage() { return "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:system-ui;margin:18px'><h2>Saved.</h2><p>You can close this page now.</p><p>Hold joystick button 3s to reopen portal.</p></body></html>"; }
void sendNoCache() { server.sendHeader("Cache-Control","no-store"); server.sendHeader("Pragma","no-cache"); }
void redirectHome() { sendNoCache(); server.sendHeader("Location","/",true); server.send(303,"text/plain",""); }

void handleRoot() { if (!portalEnabled) { sendNoCache(); server.send(200,"text/html",donePage()); return; } sendNoCache(); server.send(200,"text/html",portalPage(lastNotice)); lastNotice=""; }
void handleAdd() { if (!portalEnabled) { server.send(200,"text/html",donePage()); return; } String app=server.arg("app"), user=server.arg("user"), pass=server.arg("pass"); app.trim(); user.trim(); pass.trim(); if(!app.length()||!user.length()||!pass.length()){ lastNotice="Please fill out all fields."; redirectHome(); return; } if(!addEntry(app,user,pass)){ lastNotice="Storage full."; redirectHome(); return; } if(unlockedMenu&&!viewing) showMenuOrEmpty(); lastNotice="Saved: "+app; redirectHome(); }
void handleEdit() { if (!portalEnabled) { server.send(200,"text/html",donePage()); return; } if(entryCount==0){ lastNotice="No entries."; redirectHome(); return; } int idx=server.arg("idx").toInt(); String app=server.arg("app"),user=server.arg("user"),pass=server.arg("pass"); app.trim(); user.trim(); pass.trim(); if(idx<0||idx>=entryCount){ lastNotice="Invalid entry."; redirectHome(); return; } if(!app.length()||!user.length()||!pass.length()){ lastNotice="Fill all fields."; redirectHome(); return; } editEntry(idx,app,user,pass); if(unlockedMenu&&!viewing) showMenuOrEmpty(); lastNotice="Updated: "+app; redirectHome(); }
void doDeleteByIdx(int idx) { if(entryCount==0){ lastNotice="No entries."; redirectHome(); return; } if(idx<0||idx>=entryCount){ lastNotice="Invalid."; redirectHome(); return; } String name=entries[idx].app; deleteEntry(idx); if(unlockedMenu&&!viewing) showMenuOrEmpty(); if(entryCount==0&&unlockedMenu) lcdEmptyMenuHint(); lastNotice="Deleted: "+name; redirectHome(); }
void handleDeleteGET() { doDeleteByIdx(server.hasArg("idx")?server.arg("idx").toInt():-1); }
void handleDone() { portalEnabled=false; sendNoCache(); server.send(200,"text/html",donePage()); }
void handleCaptive() { redirectHome(); }
void handleNotFound() { if(server.method()==HTTP_GET){ server.sendHeader("Location",String("http://")+WiFi.softAPIP().toString()+"/",true); server.send(302,"text/plain",""); return; } sendNoCache(); server.send(200,"text/html",portalEnabled?portalPage(""): donePage()); }

void startAccessPointAndPortal() {
  WiFi.disconnect(true,true); WiFi.mode(WIFI_OFF); delay(250); WiFi.mode(WIFI_AP); delay(100);
  WiFi.softAP(AP_SSID, AP_PASS); delay(200);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.on("/",HTTP_GET,handleRoot); server.on("/add",HTTP_POST,handleAdd); server.on("/edit",HTTP_POST,handleEdit);
  server.on("/delete",HTTP_GET,handleDeleteGET); server.on("/done",HTTP_GET,handleDone);
  server.on("/generate_204",HTTP_GET,handleCaptive); server.on("/hotspot-detect.html",HTTP_GET,handleCaptive);
  server.on("/fwlink",HTTP_GET,handleCaptive); server.on("/connecttest.txt",HTTP_GET,handleCaptive);
  server.on("/redirect",HTTP_GET,handleCaptive); server.onNotFound(handleNotFound); server.begin();
}

bool buttonIsDownRaw() { return (digitalRead(JOY_SW) == LOW); }

bool buttonShortClickEvent() {
  unsigned long now = millis(); bool down = buttonIsDownRaw(); bool event = false;
  if (down && !btnWasDown) { btnWasDown = true; btnDownAtMs = now; }
  else if (!down && btnWasDown) { unsigned long held = now - btnDownAtMs; btnWasDown = false; if (held >= 25 && held < 700) event = true; }
  return event;
}

int readJoyAxisUpDown() { return analogRead(JOY_VRX); }

void setLockedState(bool locked) {
  unlockedMenu = !locked;
  if (locked) { viewing = false; viewingIndex = -1; menuIndex = 0; lcdLocked(); }
  else { lcdUnlockedHint(); showMenuOrEmpty(); }
}

void checkKeyCardToggle() {
  unsigned long now = millis();
  if ((now - lastCardToggleMs) < CARD_TOGGLE_COOLDOWN_MS) return;
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    lastCardToggleMs = now;
    if (unlockedMenu) setLockedState(true); else setLockedState(false);
    mfrc522.PICC_HaltA(); mfrc522.PCD_StopCrypto1(); delay(200);
  }
}

void handleLongPressPortalReopen() {
  unsigned long now = millis(); bool down = buttonIsDownRaw();
  if (down && pressStartMs == 0) { pressStartMs = now; longPressFired = false; }
  if (!down) { pressStartMs = 0; longPressFired = false; return; }
  if (pressStartMs != 0 && !longPressFired && (now - pressStartMs) >= LONG_PRESS_MS) {
    longPressFired = true; portalEnabled = true; lastNotice = "Portal re-opened.";
    lcd.clear(); lcd.setCursor(0,0); lcd.print("Portal enabled"); lcd.setCursor(0,1); lcd.print("Use WiFi"); delay(850);
    if (!unlockedMenu) lcdLocked(); else if (!viewing) showMenuOrEmpty();
  }
}

void resetDoubleClickState() { clickCount = 0; firstClickMs = 0; }

void menuLoop() {
  if (entryCount <= 0) return;
  unsigned long now = millis(); int v = readJoyAxisUpDown();
  if (v < CENTER_LOW && (now - lastMoveTime) > MOVE_DEBOUNCE_MS) { lastMoveTime = now; menuIndex--; if (menuIndex < 0) menuIndex = entryCount - 1; lcdMenuLine(menuIndex); }
  else if (v > CENTER_HIGH && (now - lastMoveTime) > MOVE_DEBOUNCE_MS) { lastMoveTime = now; menuIndex++; if (menuIndex >= entryCount) menuIndex = 0; lcdMenuLine(menuIndex); }
  if (buttonShortClickEvent()) { viewing = true; viewingIndex = menuIndex; viewMode = SHOW_USERNAME; scrollOffset = 0; lastScrollKey = ""; lastScrollMs = 0; lcdViewingHeader(entries[viewingIndex].app); lcdViewingLineScrollNoFlicker("User:", entries[viewingIndex].user); resetDoubleClickState(); }
}

void viewingLoop() {
  if (viewingIndex < 0 || viewingIndex >= entryCount) { viewing = false; viewingIndex = -1; showMenuOrEmpty(); return; }
  unsigned long now = millis(); int v = readJoyAxisUpDown();
  if (v < CENTER_LOW && (now - lastMoveTime) > MOVE_DEBOUNCE_MS) { lastMoveTime = now; viewMode = SHOW_USERNAME; lastScrollKey = ""; scrollOffset = 0; lcdViewingHeader(entries[viewingIndex].app); }
  else if (v > CENTER_HIGH && (now - lastMoveTime) > MOVE_DEBOUNCE_MS) { lastMoveTime = now; viewMode = SHOW_PASSWORD; lastScrollKey = ""; scrollOffset = 0; lcdViewingHeader(entries[viewingIndex].app); }
  if (viewMode == SHOW_USERNAME) lcdViewingLineScrollNoFlicker("User:", entries[viewingIndex].user);
  else lcdViewingLineScrollNoFlicker("Pass:", entries[viewingIndex].pass);
  if (buttonShortClickEvent()) {
    if (clickCount == 0) { clickCount = 1; firstClickMs = now; }
    else { if (now - firstClickMs <= DOUBLE_CLICK_WINDOW) { viewing = false; viewingIndex = -1; resetDoubleClickState(); showMenuOrEmpty(); delay(120); return; } else { clickCount = 1; firstClickMs = now; } }
  }
  if (clickCount == 1 && (now - firstClickMs) > DOUBLE_CLICK_WINDOW) resetDoubleClickState();
}

void setup() {
  Serial.begin(115200); Wire.begin(21, 22); lcd.init(); lcd.backlight();
  pinMode(JOY_SW, INPUT_PULLUP); SPI.begin(18, 19, 23, RFID_SDA); mfrc522.PCD_Init();
  randomSeed(analogRead(JOY_VRX) ^ analogRead(JOY_VRY) ^ millis());
  loadEntriesFromNVS(); lcdBoot(); startAccessPointAndPortal(); delay(900); setLockedState(true);
}

void loop() {
  dnsServer.processNextRequest(); server.handleClient();
  handleLongPressPortalReopen(); checkKeyCardToggle();
  if (!unlockedMenu) { delay(10); return; }
  if (!viewing) menuLoop(); else viewingLoop();
  delay(10);
}
