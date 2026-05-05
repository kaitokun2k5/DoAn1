#include <WiFi.h>
#include <esp_now.h>

// ===== DATA STRUCT (GIỮ NGUYÊN như code cũ) =====
typedef struct {
  int x;
  int y;
  int speed;
} controlData;

controlData outgoing;

// ===== JOYSTICK PINS =====
#define JS_SPEED 36
#define JS_Y     34
#define JS_X     35

// ===== MODE BUTTON + LED =====
#define BTN_MODE 23      
#define LED_CONNECT  13
#define LED_MODE  22
// ===== MAC ESP32-CAM / RX =====
uint8_t receiverMAC[6] = {0x2C, 0xBC, 0xBB, 0x84, 0x9D, 0x28};

// ===== SEND STATUS BLINK =====
bool Blink = false;
unsigned long lastBlink = 0;

// ===== MODE =====
enum Mode : uint8_t { MODE_JOYSTICK = 0, MODE_LINE = 1 };
Mode modeNow = MODE_JOYSTICK;

// ===== BUTTON DEBOUNCE =====
bool btnReading = HIGH;
bool btnStable  = HIGH;
unsigned long lastDebounce = 0;
const unsigned long debounceMs = 25;

// ===== CALLBACK SEND (ESP32 core 3.3.3) =====
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Blink = false;
  }
  else {
    Blink = true;
  }
}

// ===== MAP joystick axis =====
int mapAxis(int raw) {
  int v = map(raw, 0, 4095, -255, 255);
  if (abs(v) < 60) v = 0;  // deadzone
  return v;
}

// ===== gửi ký tự mode sang RX =====
void sendModeChar() {
  uint8_t c = (modeNow == MODE_JOYSTICK) ? 'J' : 'L';

  // gửi lặp 2 lần cho chắc (không block lâu)
  esp_now_send(receiverMAC, &c, 1);
  delay(5);
  esp_now_send(receiverMAC, &c, 1);

  Serial.printf(">>> MODE = %c\n", c);
}

// ===== toggle mode khi THẢ nút =====
void toggleMode() {
  modeNow = (modeNow == MODE_JOYSTICK) ? MODE_LINE : MODE_JOYSTICK;
  sendModeChar();
}

// ===== update button (nhấn + thả => đổi mode) =====
void updateButton() {
  bool r = digitalRead(BTN_MODE);

  if (r != btnReading) {
    btnReading = r;
    lastDebounce = millis();
  }

  if (millis() - lastDebounce >= debounceMs) {
    if (btnStable != btnReading) {
      bool prev = btnStable;
      btnStable = btnReading;

      // release edge: LOW -> HIGH (vì pullup)
      if (prev == LOW && btnStable == HIGH) {
        toggleMode();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(JS_SPEED, INPUT);
  pinMode(JS_X, INPUT);
  pinMode(JS_Y, INPUT);

  pinMode(LED_CONNECT, OUTPUT);
  pinMode(LED_MODE, OUTPUT);
  digitalWrite(LED_CONNECT, LOW);

  pinMode(BTN_MODE, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // ===== INIT ESP-NOW =====
  if (esp_now_init() != ESP_OK) {
    return;
  }
  esp_now_register_send_cb(onDataSent);

  // ===== ADD PEER =====
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, receiverMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) return;

  // gửi mode mặc định lúc khởi động
  sendModeChar();
}

void loop() {
  // 1) đọc nút liên tục
  updateButton();

  // 2) blink nếu gửi fail
  if (Blink) {
    if (millis() - lastBlink >= 100) {
      lastBlink = millis();
      digitalWrite(LED_CONNECT, !digitalRead(LED_CONNECT));
    }
  } else {
    digitalWrite(LED_CONNECT, LOW);
  }

  // 3) đọc joystick
  int rawX = analogRead(JS_X);
  int rawY = analogRead(JS_Y);
  int rawSpeed = analogRead(JS_SPEED);

  int x = mapAxis(rawX);
  int y = -mapAxis(rawY);                 // invert Y
  int sp = map(rawSpeed, 0, 4095, 0, 255);

  // 4) đóng gói data gửi đi
  if (modeNow == MODE_JOYSTICK) {
    outgoing.x = x;
    outgoing.y = y;
    outgoing.speed = sp;
    digitalWrite(LED_MODE, LOW);
  } else {
    // LINE mode:
    outgoing.x = 0;
    outgoing.y = 0;
    outgoing.speed = 50;
    digitalWrite(LED_MODE, HIGH);  
  }

  // 5) gửi struct như cũ
  esp_now_send(receiverMAC, (uint8_t*)&outgoing, sizeof(outgoing));

  // debug
  Serial.printf("MODE=%s | X=%d Y=%d SPEED=%d\n",
                (modeNow == MODE_JOYSTICK) ? "JOY" : "LINE",
                outgoing.x, outgoing.y, outgoing.speed);

  delay(40); // ~25Hz
}
