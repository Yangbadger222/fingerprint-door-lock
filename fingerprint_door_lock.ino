/*
 * 声纹指纹门锁系统 - Arduino Nano ESP32 专用版
 * MCU: Arduino Nano ESP32 (NORA-W106-10B / ESP32-S3)
 *
 * 核心改动 (相对于 Nano ATmega328P):
 *   - 使用 HardwareSerial Serial1 (D0=RX, D1=TX) 代替 SoftwareSerial
 *   - 3.3V I/O, 指纹模块直连无需分压
 *   - 所有数字引脚均支持 PWM (analogWrite via LEDC), RGB 真彩呼吸
 *   - Serial = USB CDC, 不占用任何引脚
 *
 * 功能:
 *   - 声音唤醒 (KY-038 + MAX9812)
 *   - 指纹验证 (TM1026M)
 *   - OLED 多场景动画 (SSD1306 0.96")
 *   - 无源蜂鸣器多段旋律
 *   - 继电器控制电磁锁
 *   - RGB LED 状态指示 (真 PWM 渐变)
 *   - ISD1820 语音播报
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Fingerprint.h>

// ==================== 引脚定义 (Nano ESP32) ====================
//   D0 = Serial1 RX  → 指纹 TX
//   D1 = Serial1 TX  → 指纹 RX
#define ISD1820_PIN  2    // ISD1820 PLAYE
#define BUZZER_PIN   3    // 无源蜂鸣器 (PWM/LEDC)
#define RELAY_PIN    4    // 继电器 IN
#define RGB_R        5    // RGB-R (PWM)
#define RGB_G        6    // RGB-G (PWM)
#define RGB_B        7    // RGB-B (PWM)
#define MIC_DO       8    // KY-038 数字输出
#define SOUND_AO     A0   // MAX9812 模拟输出

// 共阴极 RGB: 设为 false; 共阳极: 设为 true
#define RGB_COMMON_ANODE  false

// ==================== OLED ====================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C    // 备用 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== 指纹 (硬件串口) ====================
HardwareSerial &fpSerial = Serial1;
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fpSerial);

// ==================== 系统状态 ====================
enum SystemState {
  STATE_IDLE, STATE_WAKE, STATE_WAIT_FINGER,
  STATE_MATCH_OK, STATE_MATCH_FAIL, STATE_UNLOCKED
};
SystemState currentState = STATE_IDLE;
unsigned long stateTimer = 0;
unsigned long lastAnimFrame = 0;
uint32_t animFrame = 0;
int      enrollId = 1;
bool     enrollMode = false;
uint16_t unlockCount = 0;     // 累计开锁次数 (>=5 触发彩蛋旋律)

// ==================== 音符 ====================
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_A5 880
#define NOTE_B5 988
#define NOTE_C6 1047
#define REST     0

// ==================== 函数前向声明 ====================
void showBootScreen();
void showIdleScreen();
void showWakeAnimation();
void showFingerWaitScreen();
void showUnlockAnimation();
void showFailAnimation();
void showUnlockedScreen();
void drawLockIcon(int x, int y, int size, bool locked);
void drawFingerprintIcon(int x, int y, int frame);
void drawArc(int cx, int cy, int r, int s, int e);
void drawHeart(int cx, int cy, int size);
void drawSparkle(int x, int y, int size);
void setRGB(uint8_t r, uint8_t g, uint8_t b);
void breathRGB(uint8_t r, uint8_t g, uint8_t b);
void rainbowRGB();
bool detectSound();
int  getFingerprintMatch();
void playStartupMelody();
void playWakeTone();
void playSuccessMelody();
void playFailTone();
void playLockTone();
void playTimeoutTone();
void playEnrollTone();
void playEasterEgg();
void playISD1820();
void checkSerialCommand();
void handleEnroll();
void showEnrollError();

// ==================== setup ====================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== 声纹指纹门锁 (Nano ESP32) ==="));

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(ISD1820_PIN, OUTPUT);
  pinMode(MIC_DO, INPUT);
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(ISD1820_PIN, LOW);
  setRGB(0, 0, 0);

  // ESP32 ADC 用 10bit 与下文阈值一致
  analogReadResolution(10);

  // OLED 初始化
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("OLED 初始化失败!"));
      while (1) { tone(BUZZER_PIN, 1000, 150); delay(400); }
    }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  showBootScreen();
  playStartupMelody();

  // 指纹模块初始化
  fpSerial.begin(57600, SERIAL_8N1, D0, D1);  // ESP32 显式指定 RX/TX
  delay(100);
  if (finger.verifyPassword()) {
    Serial.println(F("指纹模块: OK"));
    finger.getTemplateCount();
    Serial.print(F("已存指纹: ")); Serial.println(finger.templateCount);
    enrollId = finger.templateCount + 1;
  } else {
    Serial.println(F("指纹模块: 未找到, 请检查接线/波特率"));
    display.clearDisplay();
    display.setCursor(8, 28);
    display.print(F("FP Sensor Error!"));
    display.display();
    delay(2000);
  }

  Serial.println(F("系统就绪. 串口命令: e=录入, d=删除全部"));
  currentState = STATE_IDLE;
  stateTimer = millis();
}

// ==================== loop ====================
void loop() {
  checkSerialCommand();
  if (enrollMode) { handleEnroll(); return; }

  switch (currentState) {
    case STATE_IDLE:        handleIdle();        break;
    case STATE_WAKE:        handleWake();        break;
    case STATE_WAIT_FINGER: handleWaitFinger();  break;
    case STATE_MATCH_OK:    handleMatchOK();     break;
    case STATE_MATCH_FAIL:  handleMatchFail();   break;
    case STATE_UNLOCKED:    handleUnlocked();    break;
  }
}

// ==================== 状态处理 ====================
void handleIdle() {
  showIdleScreen();
  breathRGB(60, 0, 0);          // 红色呼吸
  if (detectSound()) {
    Serial.println(F("声音唤醒!"));
    currentState = STATE_WAKE;
    stateTimer = millis();
    setRGB(0, 0, 200);
    playWakeTone();
  }
}

void handleWake() {
  showWakeAnimation();
  if (millis() - stateTimer > 1500) {
    currentState = STATE_WAIT_FINGER;
    stateTimer = millis();
  }
}

void handleWaitFinger() {
  showFingerWaitScreen();
  // 蓝色脉冲
  uint8_t v = (sin(millis() / 150.0) + 1.0) * 127;
  setRGB(0, 0, v);

  int r = getFingerprintMatch();
  if (r >= 0) {
    Serial.print(F("匹配! ID=")); Serial.print(r);
    Serial.print(F(" 置信度=")); Serial.println(finger.confidence);
    currentState = STATE_MATCH_OK; stateTimer = millis();
  } else if (r == -2) {
    Serial.println(F("指纹不匹配"));
    currentState = STATE_MATCH_FAIL; stateTimer = millis();
  }
  if (millis() - stateTimer > 15000) {
    Serial.println(F("等待超时"));
    playTimeoutTone();
    currentState = STATE_IDLE; stateTimer = millis();
  }
}

void handleMatchOK() {
  setRGB(0, 220, 0);
  showUnlockAnimation();
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println(F(">>> 门锁打开 <<<"));
  unlockCount++;
  if (unlockCount % 5 == 0) playEasterEgg();
  else                       playSuccessMelody();
  playISD1820();
  currentState = STATE_UNLOCKED; stateTimer = millis();
}

void handleMatchFail() {
  setRGB(220, 0, 0);
  playFailTone();
  showFailAnimation();
  delay(1500);
  currentState = STATE_IDLE; stateTimer = millis();
}

void handleUnlocked() {
  showUnlockedScreen();
  // 绿色随时间渐暗
  int remain = 5000 - (millis() - stateTimer);
  if (remain < 0) remain = 0;
  uint8_t g = map(remain, 0, 5000, 30, 220);
  setRGB(0, g, 0);

  if (millis() - stateTimer > 5000) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println(F(">>> 门锁关闭 <<<"));
    playLockTone();
    currentState = STATE_IDLE; stateTimer = millis();
  }
}

// ==================== 声音检测 ====================
bool detectSound() {
  if (digitalRead(MIC_DO) == LOW) {
    delay(30);
    if (digitalRead(MIC_DO) == LOW) return true;
  }
  // MAX9812 模拟检测
  int peak = 0;
  for (int i = 0; i < 32; i++) {
    int v = analogRead(SOUND_AO);
    int dev = abs(v - 512);
    if (dev > peak) peak = dev;
    delayMicroseconds(200);
  }
  return peak > 100;
}

// ==================== 指纹识别 ====================
int getFingerprintMatch() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) return finger.fingerID;
  return -2;
}

// ==================== OLED 动画 ====================
void showBootScreen() {
  // 扫描线开机动画
  for (int y = 0; y < SCREEN_HEIGHT; y += 2) {
    display.drawLine(0, y, SCREEN_WIDTH - 1, y, SSD1306_WHITE);
    display.display();
    delay(8);
  }
  display.clearDisplay();
  drawLockIcon(48, 5, 32, true);
  // 闪烁的星点
  for (int i = 0; i < 6; i++) {
    drawSparkle(random(0, 128), random(0, 64), 3);
  }
  display.setTextSize(1);
  display.setCursor(8, 48);
  display.print(F("Smart Door Lock"));
  display.setCursor(28, 56);
  display.print(F("Nano ESP32"));
  display.display();
  delay(1500);
}

void showIdleScreen() {
  if (millis() - lastAnimFrame < 80) return;
  lastAnimFrame = millis();
  animFrame++;

  display.clearDisplay();

  // 锁图标 + 周围呼吸圆环
  drawLockIcon(48, 2, 28, true);
  int rr = (animFrame / 4) % 20 + 18;
  display.drawCircle(64, 18, rr, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(35, 40);
  display.print(F("- LOCKED -"));

  // 滚动横幅
  const char *banner = "Speak to wake up * ";
  int sx = -((animFrame * 3) % 130);
  display.setCursor(sx, 54);
  display.print(banner);
  display.setCursor(sx + 130, 54);
  display.print(banner);

  display.display();
}

void showWakeAnimation() {
  // 同心声波扩散 + 呼吸光点
  for (int r = 6; r < 50; r += 4) {
    display.clearDisplay();
    // 中心麦克风
    display.fillCircle(64, 28, 6, SSD1306_WHITE);
    display.fillRect(62, 34, 4, 8, SSD1306_WHITE);
    display.drawLine(56, 44, 72, 44, SSD1306_WHITE);
    // 三圈声波
    for (int i = 0; i < 3; i++) {
      int rr = r - i * 8;
      if (rr > 0) {
        drawArc(64, 28, rr, 200, 340);
        drawArc(64, 28, rr,  20, 160);
      }
    }
    display.setTextSize(1);
    display.setCursor(20, 54);
    display.print(F("Voice Detected!"));
    display.display();
    delay(60);
  }
}

void showFingerWaitScreen() {
  if (millis() - lastAnimFrame < 80) return;
  lastAnimFrame = millis();
  animFrame++;

  display.clearDisplay();
  drawFingerprintIcon(50, 2, animFrame);

  display.setTextSize(1);
  display.setCursor(15, 40);
  display.print(F("Place Finger..."));

  // 双向流动进度条
  int bw = (animFrame * 4) % 100;
  display.drawRect(14, 54, 100, 8, SSD1306_WHITE);
  display.fillRect(14, 54, bw, 8, SSD1306_WHITE);
  display.fillRect(14 + 100 - bw, 54, bw / 3, 8, SSD1306_WHITE);

  display.display();
}

void showUnlockAnimation() {
  // 1) 锁开门动画
  for (int f = 0; f < 8; f++) {
    display.clearDisplay();
    drawLockIcon(48, 5, 28, false);
    // 放射庆祝线
    for (int i = 0; i < 12; i++) {
      float a = (i * 30 + f * 10) * PI / 180.0;
      int x1 = 64 + cos(a) * (22 + f);
      int y1 = 22 + sin(a) * (22 + f);
      int x2 = 64 + cos(a) * (32 + f * 2);
      int y2 = 22 + sin(a) * (32 + f * 2);
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
    display.setTextSize(2);
    display.setCursor(10, 46);
    display.print(F("UNLOCKED"));
    display.display();
    delay(80);
  }
  // 2) 烟花式星点
  for (int f = 0; f < 6; f++) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 22);
    display.print(F("WELCOME!"));
    for (int i = 0; i < 10; i++) {
      drawSparkle(random(0, 128), random(0, 64), random(2, 5));
    }
    display.display();
    delay(120);
  }
}

void showFailAnimation() {
  // 抖动 + X
  for (int i = 0; i < 4; i++) {
    int sh = (i % 2) ? 3 : -3;
    display.clearDisplay();
    display.drawLine(44 + sh, 5,  84 + sh, 45, SSD1306_WHITE);
    display.drawLine(84 + sh, 5,  44 + sh, 45, SSD1306_WHITE);
    display.drawLine(45 + sh, 5,  85 + sh, 45, SSD1306_WHITE);
    display.drawLine(85 + sh, 5,  45 + sh, 45, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 52);
    display.print(F("ACCESS DENIED"));
    display.display();
    delay(120);
  }
  delay(400);
}

void showUnlockedScreen() {
  if (millis() - lastAnimFrame < 100) return;
  lastAnimFrame = millis();

  display.clearDisplay();
  drawLockIcon(48, 2, 28, false);
  drawHeart(20, 30, 6);
  drawHeart(100, 30, 6);

  int remain = 5 - (millis() - stateTimer) / 1000;
  if (remain < 0) remain = 0;
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.print(F("Door Open: "));
  display.print(remain); display.print(F("s"));

  int prog = map(millis() - stateTimer, 0, 5000, 100, 0);
  if (prog < 0) prog = 0;
  display.drawRect(14, 54, 100, 8, SSD1306_WHITE);
  display.fillRect(14, 54, prog, 8, SSD1306_WHITE);
  display.display();
}

// ==================== 绘图辅助 ====================
void drawLockIcon(int x, int y, int size, bool locked) {
  int bodyW = size, bodyH = size * 0.6, bodyY = y + size * 0.5;
  int shW = size * 0.5, shH = size * 0.4;
  display.fillRoundRect(x, bodyY, bodyW, bodyH, 3, SSD1306_WHITE);
  int hx = x + bodyW / 2, hy = bodyY + bodyH / 3;
  display.fillCircle(hx, hy, 3, SSD1306_BLACK);
  display.drawLine(hx, hy + 2, hx, hy + bodyH / 3, SSD1306_BLACK);
  int sx = x + (bodyW - shW) / 2;
  if (locked) {
    display.drawRoundRect(sx, y, shW, shH + 5, shW / 2, SSD1306_WHITE);
    display.drawRoundRect(sx + 1, y, shW - 2, shH + 5, shW / 2, SSD1306_WHITE);
  } else {
    display.drawRoundRect(sx + 8, y - 3, shW, shH + 5, shW / 2, SSD1306_WHITE);
    display.drawRoundRect(sx + 9, y - 3, shW - 2, shH + 5, shW / 2, SSD1306_WHITE);
  }
}

void drawFingerprintIcon(int x, int y, int frame) {
  int cx = x + 14, cy = y + 16;
  for (int r = 4; r < 16; r += 3) {
    int sa = (frame * 10 + r * 5) % 360;
    for (int a = 0; a < 180; a += 8) {
      float rad = (sa + a) * PI / 180.0;
      int px = cx + cos(rad) * r;
      int py = cy + sin(rad) * (r * 0.7);
      if (px >= 0 && px < 128 && py >= 0 && py < 64)
        display.drawPixel(px, py, SSD1306_WHITE);
    }
  }
  display.drawRoundRect(x, y, 28, 32, 5, SSD1306_WHITE);
}

void drawArc(int cx, int cy, int r, int s, int e) {
  for (int a = s; a <= e; a += 4) {
    float rad = a * PI / 180.0;
    int px = cx + cos(rad) * r;
    int py = cy + sin(rad) * r;
    if (px >= 0 && px < 128 && py >= 0 && py < 64)
      display.drawPixel(px, py, SSD1306_WHITE);
  }
}

void drawHeart(int cx, int cy, int s) {
  display.fillCircle(cx - s / 2, cy, s / 2, SSD1306_WHITE);
  display.fillCircle(cx + s / 2, cy, s / 2, SSD1306_WHITE);
  display.fillTriangle(cx - s, cy, cx + s, cy, cx, cy + s, SSD1306_WHITE);
}

void drawSparkle(int x, int y, int s) {
  display.drawLine(x - s, y, x + s, y, SSD1306_WHITE);
  display.drawLine(x, y - s, x, y + s, SSD1306_WHITE);
  display.drawPixel(x - s + 1, y - s + 1, SSD1306_WHITE);
  display.drawPixel(x + s - 1, y - s + 1, SSD1306_WHITE);
  display.drawPixel(x - s + 1, y + s - 1, SSD1306_WHITE);
  display.drawPixel(x + s - 1, y + s - 1, SSD1306_WHITE);
}

// ==================== RGB LED (真 PWM) ====================
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  if (RGB_COMMON_ANODE) { r = 255 - r; g = 255 - g; b = 255 - b; }
  analogWrite(RGB_R, r);
  analogWrite(RGB_G, g);
  analogWrite(RGB_B, b);
}

void breathRGB(uint8_t r, uint8_t g, uint8_t b) {
  float k = (sin(millis() / 800.0) + 1.0) * 0.5;  // 0..1
  setRGB(r * k, g * k, b * k);
}

// ==================== 蜂鸣器旋律 ====================
void playTone(int f, int d) {
  if (f == 0) { noTone(BUZZER_PIN); delay(d); }
  else { tone(BUZZER_PIN, f, d); delay(d + 20); }
  noTone(BUZZER_PIN);
}

void playStartupMelody() {           // 上升大三和弦 + 顶音
  int n[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_E5};
  int d[] = {120, 120, 120, 120, 260};
  for (int i = 0; i < 5; i++) playTone(n[i], d[i]);
}

void playWakeTone() {                // 叮咚
  playTone(NOTE_E5, 100);
  playTone(NOTE_C5, 140);
}

void playSuccessMelody() {           // 欢快上行 + 装饰音
  int n[] = {NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6, NOTE_G5, NOTE_C6};
  int d[] = {110, 110, 110, 180, 90, 260};
  for (int i = 0; i < 6; i++) playTone(n[i], d[i]);
}

void playFailTone() {                // 错误警告: 三连下行 + 嗡音
  int n[] = {500, 400, 300, 200, 200};
  int d[] = {180, 180, 180, 220, 220};
  for (int i = 0; i < 5; i++) playTone(n[i], d[i]);
}

void playLockTone() {                // 上锁: 平稳下行
  playTone(NOTE_G4, 110);
  playTone(NOTE_E4, 110);
  playTone(NOTE_C4, 220);
}

void playTimeoutTone() {             // 两声短促
  playTone(NOTE_A4, 80);
  playTone(NOTE_A4, 80);
}

void playEnrollTone() {              // 录入提示
  playTone(NOTE_D5, 160);
}

// 彩蛋: 简短的"超级玛丽"风格升级音
void playEasterEgg() {
  int n[] = {NOTE_E5, NOTE_E5, REST,    NOTE_E5,
             REST,    NOTE_C5, NOTE_E5, NOTE_G5,
             REST,    NOTE_G4};
  int d[] = {120, 120, 80, 120, 80, 120, 120, 240, 80, 240};
  for (int i = 0; i < 10; i++) playTone(n[i], d[i]);
}

// ==================== ISD1820 ====================
void playISD1820() {
  digitalWrite(ISD1820_PIN, HIGH);
  delay(100);
  digitalWrite(ISD1820_PIN, LOW);
}

// ==================== 录入指纹 ====================
void checkSerialCommand() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == 'e' || c == 'E') {
    enrollMode = true;
    Serial.print(F("=== 录入模式, ID=")); Serial.println(enrollId);
    Serial.println(F("请放上手指..."));
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(15, 10);
    display.print(F("ENROLL MODE"));
    display.setCursor(5, 30);
    display.print(F("Place finger"));
    display.setCursor(5, 42);
    display.print(F("(step 1)"));
    display.setCursor(5, 55);
    display.print(F("ID: ")); display.print(enrollId);
    display.display();
    setRGB(0, 0, 200);
  } else if (c == 'd' || c == 'D') {
    Serial.println(F("清空所有指纹"));
    finger.emptyDatabase();
    enrollId = 1;
  }
}

void handleEnroll() {
  static uint8_t step = 0;
  if (step == 0) {
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz(1) == FINGERPRINT_OK) {
        Serial.println(F("移开手指..."));
        display.clearDisplay();
        display.setCursor(5, 25);
        display.print(F("Remove finger..."));
        display.display();
        playEnrollTone();
        delay(1500);
        while (finger.getImage() != FINGERPRINT_NOFINGER) delay(80);
        Serial.println(F("再次按同一手指"));
        display.clearDisplay();
        display.setCursor(5, 20); display.print(F("Place same finger"));
        display.setCursor(5, 32); display.print(F("(step 2)"));
        display.display();
        step = 1;
      }
    }
  } else if (step == 1) {
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz(2) == FINGERPRINT_OK &&
          finger.createModel() == FINGERPRINT_OK &&
          finger.storeModel(enrollId) == FINGERPRINT_OK) {
        Serial.print(F("录入成功 ID=")); Serial.println(enrollId);
        display.clearDisplay();
        display.setTextSize(2); display.setCursor(15, 10); display.print(F("SUCCESS"));
        display.setTextSize(1); display.setCursor(5, 40); display.print(F("Saved ID:"));
        display.print(enrollId);
        display.display();
        playSuccessMelody();
        setRGB(0, 220, 0);
        delay(1800);
        enrollId++;
      } else {
        showEnrollError();
      }
      step = 0;
      enrollMode = false;
      currentState = STATE_IDLE;
      stateTimer = millis();
      Serial.println(F("=== 退出录入 ==="));
    }
  }
}

void showEnrollError() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 25); display.print(F("Enroll Failed"));
  display.setCursor(15, 40); display.print(F("Please try again"));
  display.display();
  playFailTone();
  setRGB(220, 0, 0);
  delay(1500);
}
