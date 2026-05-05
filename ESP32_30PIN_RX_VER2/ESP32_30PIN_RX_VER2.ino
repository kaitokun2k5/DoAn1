#include <driver/ledc.h>

// ===================== LINE SENSOR PINS =====================
#define S1 32
#define S2 33
#define S3 34
#define S4 35
#define S5 39

// ===================== L298N PINS =====================
#define ENA 25
#define IN1 27
#define IN2 14

#define ENB 26
#define IN3 12
#define IN4 13

// ===================== PWM CHANNELS =====================
#define PWM_CH_A LEDC_CHANNEL_0
#define PWM_CH_B LEDC_CHANNEL_1

// ===================== UART FROM ESP32-CAM =====================
HardwareSerial CamSerial(1);   // UART1
// CamSerial.begin(115200, SERIAL_8N1, 23, -1);  // RX=23

// ===================== MODES =====================
enum Mode : uint8_t { MODE_JOY = 0, MODE_LINE = 1 };
volatile Mode currentMode = MODE_JOY;

// ===================== JOYSTICK DATA =====================
int joyX = 0, joyY = 0, joySpeed = 0;
unsigned long lastJoyPacket = 0;

// ===================== PID =====================
int Kp = 16;
int Ki = 0;       // OFF
int Kd = 6;

int error = 0;
int previous_error = 0;
int PID_value = 0;

int base_speed = 70;   // line mode base speed

// ============================================================
// PWM INIT (ESP32 core 3.x.x)
// ============================================================
void setupPWM() {
  ledc_timer_config_t timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 1000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer);

  ledc_channel_config_t channelA = {
    .gpio_num = ENA,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = PWM_CH_A,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channelA);

  ledc_channel_config_t channelB = {
    .gpio_num = ENB,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = PWM_CH_B,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channelB);
}

void motorWrite(int channel, int duty) {
  duty = constrain(duty, 0, 255);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

// ============================================================
// MOTOR CONTROL
// ============================================================
void setMotor(int left, int right) {
  left  = constrain(left,  -255, 255);
  right = constrain(right, -255, 255);

  // LEFT
  if (left >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  motorWrite(PWM_CH_A, left); }
  else           { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); motorWrite(PWM_CH_A, -left); }

  // RIGHT
  if (right >= 0){ digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  motorWrite(PWM_CH_B, right); }
  else           { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); motorWrite(PWM_CH_B, -right); }
}

void stopMotors() {
  motorWrite(PWM_CH_A, 0);
  motorWrite(PWM_CH_B, 0);
  // giữ direction gì cũng được, nhưng set LOW cho chắc
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// ============================================================
// JOYSTICK MIXING (mode JOY)
// ============================================================
void drive(int x, int y, int s) {
  int left  = y + x;
  int right = y - x;

  left  = map(left,  -255, 255, -s, s);
  right = map(right, -255, 255, -s, s);

  // LEFT
  if (left >= 0) { digitalWrite(IN1,1); digitalWrite(IN2,0); motorWrite(PWM_CH_A, left); }
  else           { digitalWrite(IN1,0); digitalWrite(IN2,1); motorWrite(PWM_CH_A, -left); }

  // RIGHT
  if (right >= 0){ digitalWrite(IN3,1); digitalWrite(IN4,0); motorWrite(PWM_CH_B, right); }
  else           { digitalWrite(IN3,0); digitalWrite(IN4,1); motorWrite(PWM_CH_B, -right); }
}

// ============================================================
// LINE PID (mode LINE)
// ============================================================
void readError() {
  int l1 = digitalRead(S1);
  int l2 = digitalRead(S2);
  int l3 = digitalRead(S3);
  int l4 = digitalRead(S4);
  int l5 = digitalRead(S5);

  if (l1==0 && l2==1 && l3==1 && l4==1 && l5==1) error = -4;
  else if (l1==0 && l2==0 && l3==1 && l4==1 && l5==1) error = -3;
  else if (l1==1 && l2==0 && l3==1 && l4==1 && l5==1) error = -2;
  else if (l1==1 && l2==0 && l3==0 && l4==1 && l5==1) error = -1;
  else if (l1==1 && l2==1 && l3==0 && l4==1 && l5==1) error = 0;
  else if (l1==1 && l2==1 && l3==0 && l4==0 && l5==1) error = 1;
  else if (l1==1 && l2==1 && l3==1 && l4==0 && l5==1) error = 2;
  else if (l1==1 && l2==1 && l3==1 && l4==0 && l5==0) error = 3;
  else if (l1==1 && l2==1 && l3==1 && l4==1 && l5==0) error = 4;
  else error = 0;
}

void computePID() {
  int P = error;
  int D = error - previous_error;

  PID_value = Kp * P + Kd * D;
  previous_error = error;

  PID_value = constrain(PID_value, -200, 200);
}

void runLineFollower() {
  readError();
  computePID();

  int left_speed  = base_speed + PID_value;
  int right_speed = base_speed - PID_value;

  // giới hạn kiểu anh đang dùng
  left_speed  = constrain(left_speed,  0, 100);
  right_speed = constrain(right_speed, 0, 100);

  setMotor(left_speed, right_speed);
}

// ============================================================
// UART PARSER (nhận cả "x,y,s" và "M,<char>")
// ============================================================
void handleLine(const String &lineRaw) {
  String line = lineRaw;
  line.trim();
  if (line.length() == 0) return;

  // MODE LINE: "M,J" hoặc "M,L"
  if (line.startsWith("M,")) {
    if (line.length() >= 3) {
      char m = line.charAt(2);

      Mode newMode = currentMode;
      if (m == 'J' || m == 'j') newMode = MODE_JOY;
      if (m == 'L' || m == 'l') newMode = MODE_LINE;

      if (newMode != currentMode) {
        currentMode = newMode;
        previous_error = 0;
        PID_value = 0;

        stopMotors(); // đổi mode thì stop 1 nhịp
        Serial.printf("MODE => %s\n", currentMode == MODE_JOY ? "JOYSTICK" : "LINE");
      }
    }
    return;
  }

  // JOY DATA: "x,y,s"
  int x, y, s;
  if (sscanf(line.c_str(), "%d,%d,%d", &x, &y, &s) == 3) {
    joyX = x; joyY = y; joySpeed = s;
    lastJoyPacket = millis();

    // chỉ lái ngay khi đang ở mode JOY
    if (currentMode == MODE_JOY) {
      drive(joyX, joyY, joySpeed);
    }

    Serial.printf("UART JOY = %d %d %d\n", joyX, joyY, joySpeed);
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  CamSerial.begin(115200, SERIAL_8N1, 23, -1);  // RX = GPIO23

  // line sensors
  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);
  pinMode(S5, INPUT);

  // motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  setupPWM();
  stopMotors();

  Serial.println("READY: UART from ESP32-CAM | Modes: J=Joystick, L=Line");
}

void loop() {
  // đọc UART (có thể có nhiều dòng)
  while (CamSerial.available()) {
    String line = CamSerial.readStringUntil('\n');
    handleLine(line);
  }

  // chạy line follower liên tục khi ở mode LINE
  if (currentMode == MODE_LINE) {
    runLineFollower();
    delay(10);
  } else {
    // mode JOY: nếu quá lâu không có gói thì stop
    if (millis() - lastJoyPacket > 400) {
      stopMotors();
    }
    delay(2);
  }
}
