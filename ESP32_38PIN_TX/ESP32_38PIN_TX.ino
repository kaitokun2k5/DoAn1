#include <WiFi.h>
#include <esp_now.h>

// ===== DATA STRUCT =====
typedef struct {
  int x;
  int y;
  int speed;
} controlData;

controlData outgoing;

// ===== PIN =====
#define JS_SPEED 36
#define JS_Y     34
#define JS_X     35
bool Blink = false;
unsigned long lastBlink = 0;
// ===== MAC ESP32-CAM =====
uint8_t receiverMAC[6] = {0x2C, 0xBC, 0xBB, 0x84, 0x9D, 0x28};

// ===== CALLBACK SEND (chuẩn ESP32 core 3.3.3) =====
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Blink = false;
  }
  else {
    Blink = true;
  }
}

// ===== MAP =====
int mapAxis(int raw) {
  int v = map(raw, 0, 4095, -255, 255);
  if (abs(v) < 60) v = 0;  // deadzone lớn để không bị chạy khi thả tay joystick
  return v;
}

void setup() {
  Serial.begin(115200);

  pinMode(JS_SPEED, INPUT);
  pinMode(JS_X, INPUT);
  pinMode(JS_Y, INPUT);
  pinMode(13, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // ===== INIT ESP-NOW =====
  if (esp_now_init() != ESP_OK) {
    //Serial.println("ESP-NOW Init Failed!");
    return;
  }

  // đăng ký callback
  esp_now_register_send_cb(onDataSent);

  // ===== ADD PEER =====
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, receiverMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    //Serial.println("Add Peer FAIL!");
    return;
  }

  //Serial.println("Joystick TX Ready!");
}

void loop() {
  if (Blink) {
    if (millis()-lastBlink >= 100) {
      lastBlink = millis();
      digitalWrite(13,!digitalRead(13));
      }
  }
  int rawX = analogRead(JS_X);
  int rawY = analogRead(JS_Y);
  int rawSpeed = analogRead(JS_SPEED);

  // chỉ dùng X,Y để điều hướng – KHÔNG tính tốc độ
  outgoing.x = mapAxis(rawX);
  outgoing.y = -mapAxis(rawY);  // invert Y
  outgoing.speed = map(rawSpeed, 0, 4095, 0, 255);

  esp_now_send(receiverMAC, (uint8_t*)&outgoing, sizeof(outgoing));

  Serial.printf("X=%d  Y=%d  SPEED=%d\n",
                outgoing.x,
                outgoing.y,
                outgoing.speed);

  delay(40);  // 25Hz update rate
}
