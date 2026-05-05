#include <driver/ledc.h>

// ===== Line sensor pins =====
#define S1 32   // Trái nhất
#define S2 33
#define S3 34   // Mắt giữa
#define S4 35
#define S5 39   // Phải nhất

// ===== L298N pins =====
#define ENA 25
#define IN1 27
#define IN2 14

#define ENB 26
#define IN3 12
#define IN4 13

// ===== PWM CHANNELS =====
#define PWM_CH_A LEDC_CHANNEL_0
#define PWM_CH_B LEDC_CHANNEL_1

// ===== PID =====
int Kp = 16;
int Ki = 0;       // OFF integral
int Kd = 6;

int error = 0;
int previous_error = 0;
int PID_value = 0;

int base_speed = 70; // tốc độ cơ bản chạy thẳng

// ============================================
// PWM INIT FOR ESP32 CORE 3.x.x
// ============================================
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

// ============================================
void motorWrite(int channel, int duty) {
  duty = constrain(duty, 0, 255);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

// ============================================
void setMotor(int left, int right) {

  left  = constrain(left,  -255, 255);
  right = constrain(right, -255, 255);

  // LEFT MOTOR
  if (left >= 0) { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); motorWrite(PWM_CH_A, left); }
  else { digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); motorWrite(PWM_CH_A, -left); }

  // RIGHT MOTOR
  if (right >= 0) { digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); motorWrite(PWM_CH_B, right); }
  else { digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); motorWrite(PWM_CH_B, -right); }
}

// ============================================
void readError() {
  int l1 = digitalRead(S1);
  int l2 = digitalRead(S2);
  int l3 = digitalRead(S3);
  int l4 = digitalRead(S4);
  int l5 = digitalRead(S5);

  // mapping lane (S1 trái → S5 phải)
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

  //Serial.printf("S=%d%d%d%d%d | Err=%d\n", l1,l2,l3,l4,l5,error);
}

// ============================================
void computePID() {
  int P = error;
  int D = error - previous_error;

  PID_value = Kp * P + Kd * D;

  previous_error = error;

  // giới hạn PID để tránh xoay điên
  PID_value = constrain(PID_value, -200, 200);

  Serial.printf("PID=%d\n", PID_value);
}

// ============================================
void setup() {
  Serial.begin(115200);

  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);
  pinMode(S5, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  setupPWM();

  Serial.println("ESP32 Line Follower PID READY!");
}

// ============================================
void loop() {
  readError();
  computePID();

  int left_speed  = base_speed + PID_value;
  int right_speed = base_speed - PID_value;

  left_speed  = constrain(left_speed,  0, 100);
  right_speed = constrain(right_speed, 0, 100);

  setMotor(left_speed, right_speed);

  delay(10);
}
