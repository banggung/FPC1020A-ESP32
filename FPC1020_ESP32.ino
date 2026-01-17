#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include "FPC1020_ESP32.h"

#define OLED_CLK   18
#define OLED_MOSI  23
#define OLED_RESET 5
#define OLED_DC    19
#define OLED_CS    15

#define LED_PIN    4
#define BUZZER_PIN 2
#define CLK_PIN    25
#define DT_PIN     26
#define SW_PIN     27

HardwareSerial fpSerial(2);
FPC1020_ESP32 finger(&fpSerial);
Adafruit_SSD1306 oled(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

int lastClk = HIGH;
const char* menuNames[] = {"SCAN", "ENROLL", "DELETE", "LIST", "CLEAR"};
int menuIndex = 0;  // Start with SCAN
bool inSubmenu = false;
int selectedId = 1;

void beep(int f, int d) { tone(BUZZER_PIN, f, d); delay(d + 10); noTone(BUZZER_PIN); }
void beepShort() { beep(800, 30); }
void melodyOK() { beep(523, 80); beep(659, 80); beep(784, 150); }
void melodyNG() { beep(200, 100); beep(150, 200); }

void showStatus(const char* l1, const char* l2, const char* l3 = "", const char* l4 = "") {
  oled.clearDisplay();
  oled.setTextSize(2); oled.setTextColor(WHITE);
  oled.setCursor(0, 0); oled.println(l1);
  oled.setTextSize(1);
  oled.setCursor(0, 30); oled.println(l2);
  if (strlen(l3)) { oled.setCursor(0, 42); oled.println(l3); }
  if (strlen(l4)) { oled.setCursor(0, 54); oled.println(l4); }
  oled.display();
}

void showMenu() {
  char buf[24];
  if (inSubmenu && (menuIndex == 1 || menuIndex == 2)) {
    sprintf(buf, "ID: %d", selectedId);
    showStatus(menuIndex == 1 ? "ENROLL" : "DELETE", buf, "Rotate: ID", "Press: OK");
  } else {
    showStatus(menuNames[menuIndex], "Rotate: MENU", "Press: OK");
  }
}

void doScan() {
  Serial.println("\n=== SCAN ===");
  showStatus("SCAN", "Place finger now!", "Waiting...");
  beepShort();
  digitalWrite(LED_PIN, HIGH);
  
  uint8_t r = finger.Search();
  digitalWrite(LED_PIN, LOW);
  
  char buf[20];
  if (r == ACK_SUCCESS) {
    sprintf(buf, "ID: %d", g_matchedId);
    Serial.printf("MATCH: ID=%d, Perm=%d\n", g_matchedId, g_matchedPerm);
    showStatus("Access OK", buf, "");
    melodyOK();
    digitalWrite(LED_PIN, HIGH);
    delay(2000);
    digitalWrite(LED_PIN, LOW);
  } else {
    const char* msg = "Error";
    if (r == ACK_NOUSER) msg = "No match";
    else if (r == ACK_TIMEOUT) msg = "Timeout";
    Serial.printf("DENIED: %s (0x%02X)\n", msg, r);
    showStatus("Denied", msg, "");
    melodyNG();
    delay(1500);
  }
}

const char* errMsg(uint8_t r) {
  switch(r) {
    case ACK_USER_OCCUPIED: return "ID in use";
    case ACK_USER_EXIST: return "Finger exists";
    case ACK_TIMEOUT: return "Timeout";
    case ACK_FULL: return "DB full";
    default: return "Error";
  }
}

void doEnroll(int id) {
  Serial.printf("\n=== ENROLL ID %d ===\n", id);
  char buf[20];
  uint8_t r;
  
  for (int step = 1; step <= 4; step++) {
    sprintf(buf, "Step %d/4", step);
    showStatus(buf, "Place finger...", "");
    beepShort();
    digitalWrite(LED_PIN, HIGH);
    
    Serial.printf("Step %d: ", step);
    if (step == 1) r = finger.Enroll1(id, 1);
    else if (step < 4) r = finger.Enroll2(id, 1);
    else r = finger.Enroll3(id, 1);
    
    digitalWrite(LED_PIN, LOW);
    
    if (r != ACK_SUCCESS) {
      sprintf(buf, "%d/4 Fail", step);
      Serial.printf("FAILED (0x%02X)\n", r);
      showStatus(buf, errMsg(r), "");
      melodyNG();
      delay(2000);
      return;
    }
    Serial.println("OK");
    
    if (step < 4) {
      sprintf(buf, "%d/4 OK", step);
      showStatus(buf, "Lift finger!", "");
      beepShort();
      delay(1500);
    }
  }
  
  sprintf(buf, "ID: %d", id);
  showStatus("Enrolled!", buf, "");
  Serial.printf("=== ENROLL SUCCESS ID=%d ===\n", id);
  melodyOK();
  delay(2000);
}

void doDelete(int id) {
  Serial.printf("\n=== DELETE ID %d ===\n", id);
  char buf[20];
  sprintf(buf, "ID: %d", id);
  showStatus("Deleting", buf, "");
  
  uint8_t r = finger.Delete(id);
  Serial.printf("Result: %s (0x%02X)\n", r == ACK_SUCCESS ? "OK" : "FAIL", r);
  showStatus(r == ACK_SUCCESS ? "Deleted" : "Failed", r == ACK_NOUSER ? "Not found" : buf, "");
  r == ACK_SUCCESS ? melodyOK() : melodyNG();
  delay(1500);
}

void doClear() {
  Serial.println("\n=== CLEAR ===");
  showStatus("CLEAR ALL", "Press OK to confirm", "Rotate to cancel");
  
  unsigned long start = millis();
  while (millis() - start < 5000) {
    if (readEncoder()) { Serial.println("CLEAR cancelled"); showMenu(); return; }
    if (digitalRead(SW_PIN) == LOW) {
      delay(30);
      while (digitalRead(SW_PIN) == LOW) delay(10);
      
      showStatus("Clearing", "Please wait...", "");
      uint8_t r = finger.Clear();
      Serial.printf("Clear result: %s (0x%02X)\n", r == ACK_SUCCESS ? "OK" : "FAIL", r);
      showStatus(r == ACK_SUCCESS ? "Cleared!" : "Failed", r == ACK_SUCCESS ? "All deleted" : "Error", "");
      r == ACK_SUCCESS ? melodyOK() : melodyNG();
      delay(2000);
      return;
    }
    delay(50);
    yield();
  }
  Serial.println("CLEAR timeout");
}

void doList() {
  Serial.println("\n=== LIST ===");
  showStatus("LIST", "Reading...", "");
  
  uint16_t ids[50];
  uint8_t perms[50];
  uint16_t count = 0;
  
  uint8_t r = finger.GetAllUsers(ids, perms, &count, 50);
  
  if (r != ACK_SUCCESS) {
    Serial.printf("GetAllUsers FAILED: 0x%02X\n", r);
    showStatus("LIST", "Read failed", "");
    melodyNG();
    delay(1500);
    return;
  }
  
  Serial.printf("Total users: %d\n", count);
  
  if (count == 0) {
    showStatus("LIST", "No users", "Total: 0");
    delay(2000);
    return;
  }
  
  Serial.print("IDs: ");
  for (int i = 0; i < count; i++) Serial.printf("%d ", ids[i]);
  Serial.println();
  
  // Sort IDs in ascending order
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (ids[j] < ids[i]) {
        uint16_t tmp = ids[i]; ids[i] = ids[j]; ids[j] = tmp;
      }
    }
  }
  
  Serial.print("Sorted: ");
  for (int i = 0; i < count; i++) Serial.printf("%d ", ids[i]);
  Serial.println();
  
  // Display pages (5 IDs per page)
  int page = 0;
  int totalPages = (count + 4) / 5;
  if (totalPages == 0) totalPages = 1;
  
  while (true) {
    char line1[20], line2[32], line3[32];
    sprintf(line1, "Total: %d", count);
    
    // Build ID list for this page
    line2[0] = 0;
    line3[0] = 0;
    int startIdx = page * 5;
    for (int i = 0; i < 5 && (startIdx + i) < count; i++) {
      char tmp[8];
      sprintf(tmp, "%d ", ids[startIdx + i]);
      if (i < 3) strcat(line2, tmp);
      else strcat(line3, tmp);
    }
    
    char pageInfo[16];
    sprintf(pageInfo, "Page %d/%d", page + 1, totalPages);
    
    oled.clearDisplay();
    oled.setTextSize(2); oled.setTextColor(WHITE);
    oled.setCursor(0, 0); oled.println("LIST");
    oled.setTextSize(1);
    oled.setCursor(0, 20); oled.println(line1);
    oled.setCursor(0, 32); oled.print("IDs: "); oled.println(line2);
    if (strlen(line3)) { oled.setCursor(29, 42); oled.println(line3); }
    oled.setCursor(0, 54); oled.println(pageInfo);
    oled.display();
    
    // Wait for input
    unsigned long waitStart = millis();
    while (millis() - waitStart < 10000) {
      int dir = readEncoder();
      if (dir) {
        beepShort();
        page = (page + dir + totalPages) % totalPages;
        break;
      }
      if (digitalRead(SW_PIN) == LOW) {
        delay(30);
        while (digitalRead(SW_PIN) == LOW) delay(10);
        Serial.println("LIST exit");
        return;
      }
      delay(20);
      yield();
    }
    if (millis() - waitStart >= 10000) {
      Serial.println("LIST timeout");
      return;
    }
  }
}

int readEncoder() {
  int clk = digitalRead(CLK_PIN);
  if (clk != lastClk && clk == LOW) {
    lastClk = clk;
    return (digitalRead(DT_PIN) != clk) ? 1 : -1;
  }
  lastClk = clk;
  return 0;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== FPC1020A Door Lock ===");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  
  oled.begin(SSD1306_SWITCHCAPVCC);
  showStatus("FPC1020A", "Initializing...", "");
  
  finger.begin(16, 17);
  Serial.println("Sensor initialized (19200 baud)");
  
  beep(262, 80); beep(330, 80); beep(392, 80); beep(523, 150);
  delay(3000);  // Show init screen for 3 seconds
  
  // Reset menu state
  menuIndex = 0;  // Force SCAN
  inSubmenu = false;
  lastClk = digitalRead(CLK_PIN);  // Sync encoder state
  
  Serial.println("Ready! Menu: SCAN");
  showMenu();
}

void loop() {
  int dir = readEncoder();
  if (dir) {
    beepShort();
    if (!inSubmenu) menuIndex = (menuIndex + dir + 5) % 5;
    else selectedId = constrain(selectedId + dir, 1, 100);
    showMenu();
  }
  
  if (digitalRead(SW_PIN) == LOW) {
    delay(30);
    if (digitalRead(SW_PIN) == LOW) {
      while (digitalRead(SW_PIN) == LOW) { delay(10); yield(); }
      beepShort();
      
      if (!inSubmenu) {
        if (menuIndex == 0) { doScan(); showMenu(); }
        else if (menuIndex == 3) { doList(); showMenu(); }
        else if (menuIndex == 4) { doClear(); showMenu(); }
        else { inSubmenu = true; showMenu(); }
      } else {
        if (menuIndex == 1) doEnroll(selectedId);
        else doDelete(selectedId);
        inSubmenu = false;
        showMenu();
      }
    }
  }
  
  delay(20);
  yield();
}
