#include <esp_now.h>
#include <WiFi.h>

typedef struct {
  int x;
  int y;
  int speed;
} controlData;

controlData dataTX;

unsigned long lastPacket = 0;
bool lost = true;

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&dataTX, incomingData, sizeof(dataTX));
  lastPacket = millis();
  lost = false;

  // GỬI UART TỚI ESP32 DEVKIT
  Serial.printf("%d,%d,%d\n", dataTX.x, dataTX.y, dataTX.speed);
}

void setup() {
  Serial.begin(115200);      // UART TX → GPIO1 (U0TXD)

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(1);

  if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  // Nếu mất kết nối ESP-NOW → gửi STOP để xe đứng yên
  if (millis() - lastPacket > 300) {
    if (!lost) {
      lost = true;
      Serial.println("0,0,0");   // STOP
    }
  }
}
