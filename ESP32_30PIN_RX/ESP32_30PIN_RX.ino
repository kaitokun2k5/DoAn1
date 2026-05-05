#include <driver/ledc.h>

// MOTOR PINS
#define ENA 25
#define IN1 27
#define IN2 14

#define ENB 26
#define IN3 12
#define IN4 13

HardwareSerial CamSerial(1);   // UART1

int joyX = 0, joyY = 0, joySpeed = 0;

void setupPWM() {
  ledc_timer_config_t timer = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .duty_resolution  = LEDC_TIMER_8_BIT,
    .timer_num        = LEDC_TIMER_0,
    .freq_hz          = 1000,
    .clk_cfg          = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer);

  ledc_channel_config_t chA = {
    .gpio_num   = ENA,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 0
  };
  ledc_channel_config(&chA);

  ledc_channel_config_t chB = {
    .gpio_num   = ENB,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_1,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 0
  };
  ledc_channel_config(&chB);
}

void motorWrite(int ch, int duty) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)ch, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)ch);
}

void drive(int x, int y, int s) {
  
  int left  = y + x;
  int right = y - x;

  left  = map(left,  -255, 255, -s, s);
  right = map(right, -255, 255, -s, s);

  // LEFT MOTOR
  if (left >= 0) { digitalWrite(IN1,1); digitalWrite(IN2,0); motorWrite(0,left); }
  else           { digitalWrite(IN1,0); digitalWrite(IN2,1); motorWrite(0,-left); }

  // RIGHT MOTOR
  if (right >= 0){ digitalWrite(IN3,1); digitalWrite(IN4,0); motorWrite(1,right); }
  else           { digitalWrite(IN3,0); digitalWrite(IN4,1); motorWrite(1,-right); }
}

void setup() {
  Serial.begin(115200);
  CamSerial.begin(115200, SERIAL_8N1, 23, -1);  // RX pin = 23

  pinMode(IN1,OUTPUT);
  pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT);
  pinMode(IN4,OUTPUT);

  setupPWM();

  Serial.println("READY RX UART FROM ESP32-CAM");
}

void loop() {

  if (CamSerial.available()) {
    String line = CamSerial.readStringUntil('\n');
    sscanf(line.c_str(), "%d,%d,%d", &joyX, &joyY, &joySpeed);

    Serial.printf("UART = %d %d %d\n", joyX, joyY, joySpeed);

    drive(joyX, joyY, joySpeed);
  }

}
