#include <esp_now.h>
#include <WiFi.h>

// ===== DATA STRUCT (joystick) =====
typedef struct {
  int x;
  int y;
  int speed;
} controlData;

controlData dataRX;

// ===== WATCHDOG =====
unsigned long lastPacket = 0;
bool lost = true;

// ===== SAFE UART SEND =====
static void sendControlUART(int x, int y, int speed) {
  char buf[40];
  int n = snprintf(buf, sizeof(buf), "%d,%d,%d\n", x, y, speed);
  Serial.write((uint8_t*)buf, n);
}

static void sendModeUART(char m) {
  // gửi mode riêng 1 dòng để devkit tách dễ
  char buf[12];
  int n = snprintf(buf, sizeof(buf), "M,%c\n", m);
  Serial.write((uint8_t*)buf, n);
}

// ===== ESP-NOW RX CALLBACK =====
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {

  // 1) GÓI JOYSTICK (x,y,speed)
  if (len == (int)sizeof(controlData)) {
    memcpy(&dataRX, incomingData, sizeof(dataRX));
    lastPacket = millis();
    lost = false;

    sendControlUART(dataRX.x, dataRX.y, dataRX.speed);
    return;
  }

  // 2) GÓI MODE (1 ký tự)
  if (len == 1) {
    char modeChar = (char)incomingData[0];

    // tuỳ anh đặt: 'J' = joystick, 'L' = line, ...
    // lọc nhẹ để khỏi dính rác
    if (modeChar >= 32 && modeChar <= 126) {
      sendModeUART(modeChar);
    }
    return;
  }

  // Các loại gói khác thì bỏ qua
}

void setup() {
  Serial.begin(115200);   // ESP32-CAM U0TXD = GPIO1 -> nối sang RX devkit (GPIO23)

  WiFi.mode(WIFI_STA);
  //WiFi.disconnect(true);
 // WiFi.setSleep(false);

  WiFi.setChannel(1);

  if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  // Nếu mất control quá lâu -> gửi STOP cho xe đứng yên
  if (millis() - lastPacket > 300) {
    if (!lost) {
      lost = true;
      sendControlUART(0, 0, 0);   // STOP
    }
  }

  delay(10);
}
