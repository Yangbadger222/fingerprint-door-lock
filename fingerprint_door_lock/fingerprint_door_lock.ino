/*
 * 智能指纹门锁 v2.0 - Arduino Nano (ATmega328P)
 * 屏幕: 2.2" ILI9341 SPI TFT (240x320, 横屏使用 320x240)
 *
 * 功能:
 *   - 声音唤醒 (KY-038 + MAX9812)
 *   - 指纹验证 (TM1026M, AS608 协议)
 *   - 彩色 TFT UI (锁屏 / 唤醒 / 等待 / 解锁动画)
 *   - 桌面宠物 (走动 + 4 种情绪)
 *   - 个性化欢迎 (每个 ID 独立姓名/颜色/旋律)
 *   - EEPROM 留言板 (定向 / TTL / 已读)
 *   - 暴力破解锁定 (3 次失败 -> 30s 锁定)
 *   - 解锁日志 (最近 10 条)
 *   - RGB LED 状态灯 + 无源蜂鸣器旋律
 *   - ISD1820 语音播报
 *
 * 串口命令 (9600):
 *   e          录入指纹
 *   d          清空所有指纹
 *   u <id> <name> <R> <G> <B> <melodyId>   设置用户个性化
 *   ul         列出用户
 *   m <text>           群发留言
 *   m@<id> <text>      定向留言给指定 ID
 *   m! <text>          置顶留言
 *   ml         列出留言
 *   md <idx>   删除留言
 *   mc         清空留言
 *   log        查看解锁日志
 *   silent     切换静音
 *   stats      查看统计
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// ==================== 引脚 ====================
#define FP_RX_PIN    2
#define FP_TX_PIN    3
#define ISD1820_PIN  4
#define RGB_B_PIN    5    // PWM (Timer0)
#define RELAY_PIN    6    // PWM (Timer0)
#define MIC_DO_PIN   7
#define BUZZER_PIN   8    // tone() 使用 Timer2, 不冲突
#define RGB_R_PIN    9    // PWM (Timer1)
#define RGB_G_PIN   10    // PWM (Timer1)
// D11 MOSI / D12 MISO / D13 SCK 由 SPI 占用
#define TFT_CS_PIN  A1
#define TFT_DC_PIN  A2
#define TFT_RST_PIN A3
#define SOUND_AO    A0

#define RGB_COMMON_ANODE false

// ==================== TFT ====================
Adafruit_ILI9341 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
#define TFT_W 320
#define TFT_H 240

// 颜色 (RGB565)
#define C_BG     0x0000
#define C_FG     0xFFFF
#define C_DIM    0x528A
#define C_OK     0x07E0
#define C_ERR    0xF800
#define C_WAKE   0x07FF
#define C_PET    0xFFE0
#define C_ACCENT 0x051F
#define C_PINK   0xFB76
#define C_PURPLE 0x801F

// ==================== 指纹 ====================
SoftwareSerial fpSerial(FP_RX_PIN, FP_TX_PIN);
Adafruit_Fingerprint finger(&fpSerial);

// ==================== EEPROM 布局 ====================
#define EE_MAGIC0    0
#define EE_MAGIC_VAL 0xA5C30001UL
#define EE_UNLOCK_LO 4
#define EE_UNLOCK_HI 5
#define EE_THEME     6
#define EE_SILENT    7
#define EE_USER_BASE  32
#define EE_USER_SZ    16
#define EE_USER_MAX   8
#define EE_MSG_BASE   160
#define EE_MSG_SZ     96
#define EE_MSG_MAX    6
#define EE_PET_XP_LO  736
#define EE_PET_XP_HI  737
#define EE_PET_LAST   738   // 2 bytes uptime hour
#define EE_LOG_BASE   760
#define EE_LOG_SZ     6
#define EE_LOG_MAX    10
#define EE_LOG_HEAD   820
#define USER_ACTIVE_FLAG 0xAA
#define MSG_ACTIVE_FLAG  0xAA

// 用户结构 (16B):
//  [0]=active(0xAA)  [1..9]=name(8 char + null)
//  [10]=R [11]=G [12]=B  [13]=melodyId (0..3)
//  [14..15]=reserved

// 留言结构 (96B):
//  [0]=active  [1]=targetId(0=all)  [2]=readFlag  [3]=flags(bit0=pinned)
//  [4..5]=createdHour  [6..7]=ttlHours(0xFFFF=forever)
//  [8..87]=text(80B null-terminated)  [88..95]=reserved

// ==================== 系统状态 ====================
enum SysState {
  S_IDLE, S_WAKE, S_WAIT_FP,
  S_OK, S_FAIL, S_UNLOCKED, S_LOCKOUT, S_DOORBELL
};
SysState state = S_IDLE;
unsigned long stateT = 0;
unsigned long lastFrameMs = 0;
uint16_t animFrame = 0;

uint16_t unlockCount = 0;
uint8_t  enrollId = 1;
bool     enrollMode = false;
uint8_t  failStreak = 0;
unsigned long lockoutStartMs = 0;
uint8_t  lastMatchedId = 0;
bool     hasUnreadMsg = false;
uint8_t  themeId = 0;
bool     silentMode = false;

// 粗略小时计数 (无 RTC)
uint16_t uptimeHours = 0;
unsigned long lastHourMs = 0;

// 桌面宠物
int16_t  petX = 220, petY = 130;
int8_t   petDX = 1;
uint8_t  petEmotion = 0;   // 0=happy 1=sleepy 2=excited 3=sad
uint8_t  petFrame = 0;
uint16_t petXP = 0;
uint16_t petLastHour = 0;
unsigned long lastPetMs = 0;
unsigned long lastPetThinkMs = 0;

// 留言板滚动
int16_t  bannerX = TFT_W;
unsigned long lastBannerMs = 0;
uint8_t  bannerMsgIdx = 0;

// 音符
#define N_C4 262
#define N_E4 330
#define N_G4 392
#define N_A4 440
#define N_C5 523
#define N_D5 587
#define N_E5 659
#define N_F5 698
#define N_G5 784
#define N_A5 880
#define N_C6 1047

// ==================== 前向声明 ====================
void drawBoot();
void drawIdle();
void drawWake();
void drawWaitFp();
void drawOk(uint8_t id);
void drawFail();
void drawUnlocked();
void drawLockout();
void drawDoorbell();
void drawHeaderBar(const __FlashStringHelper* title, uint16_t color);
void drawLockIcon(int x, int y, int w, int h, bool open, uint16_t col);
void drawFpIcon(int x, int y, int r, uint16_t col, uint8_t f);
void drawPet(int x, int y, uint8_t emotion, uint8_t frame);
void drawMsgBanner();
void drawXMark(int x, int y, int s);
void drawSparkle(int x, int y, int s, uint16_t col);

void setRGB(uint8_t r, uint8_t g, uint8_t b);
void breathRGB(uint8_t r, uint8_t g, uint8_t b);
bool detectSound();
int  fpMatch();

void playTone(int f, int d);
void playStartup();
void playWake();
void playSuccess(uint8_t melodyId);
void playFail();
void playLockTone();
void playTimeout();
void playEnrollTone();
void playEaster();
void playDoorbell();
void playISD1820();

void eepromInit();
void loadConfig();
void saveUnlockCount();

void getUser(uint8_t id, char* nameOut, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* mel);
bool setUser(uint8_t id, const char* name, uint8_t r, uint8_t g, uint8_t b, uint8_t mel);
void listUsers();

bool addMessage(uint8_t target, uint8_t flags, uint16_t ttl, const char* text);
void deleteMessage(uint8_t idx);
void clearMessages();
void listMessages();
uint8_t findMessageForUser(uint8_t userId, uint8_t startFrom);
void markRead(uint8_t idx);
void refreshUnreadFlag();

void logUnlock(uint8_t id);
void printLog();

void updateUptime();
void updatePet();
void petInteract(bool positive);
void recomputePetEmotion();

void handleSerial();
void handleEnroll();
void handleIdle();
void handleWake();
void handleWaitFp();
void handleOk();
void handleFail();
void handleUnlocked();
void handleLockout();
void handleDoorbell();

// ==================== Setup ====================
void setup() {
  Serial.begin(9600);
  Serial.println(F("== Door Lock v2.0 (TFT) =="));

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(ISD1820_PIN, OUTPUT);
  pinMode(MIC_DO_PIN, INPUT);
  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(ISD1820_PIN, LOW);
  setRGB(0, 0, 0);

  tft.begin();
  tft.setRotation(1);     // 横屏 320x240
  tft.fillScreen(C_BG);

  eepromInit();
  loadConfig();

  drawBoot();
  playStartup();

  finger.begin(57600);
  delay(120);
  if (finger.verifyPassword()) {
    finger.getTemplateCount();
    enrollId = finger.templateCount + 1;
    if (enrollId < 1) enrollId = 1;
    Serial.print(F("FP OK, stored="));
    Serial.println(finger.templateCount);
  } else {
    Serial.println(F("FP not found!"));
    tft.fillRect(0, 200, TFT_W, 40, C_ERR);
    tft.setTextColor(C_FG);
    tft.setTextSize(2);
    tft.setCursor(40, 210);
    tft.print(F("FP Sensor Error"));
    delay(1500);
  }

  refreshUnreadFlag();
  recomputePetEmotion();
  state = S_IDLE;
  stateT = millis();
  lastHourMs = millis();
  Serial.println(F("Ready. Type 'help' for cmds."));
}

// ==================== Loop ====================
void loop() {
  handleSerial();
  updateUptime();
  updatePet();

  if (enrollMode) { handleEnroll(); return; }

  switch (state) {
    case S_IDLE:     handleIdle();     break;
    case S_WAKE:     handleWake();     break;
    case S_WAIT_FP:  handleWaitFp();   break;
    case S_OK:       handleOk();       break;
    case S_FAIL:     handleFail();     break;
    case S_UNLOCKED: handleUnlocked(); break;
    case S_LOCKOUT:  handleLockout();  break;
    case S_DOORBELL: handleDoorbell(); break;
  }
}

// ==================== State Handlers ====================
void handleIdle() {
  if (millis() - lastFrameMs > 60) {
    lastFrameMs = millis();
    animFrame++;
    drawIdle();
  }
  // 有未读留言用紫色慢闪, 否则红色呼吸
  if (hasUnreadMsg) {
    float k = (sin(millis() / 600.0) + 1.0) * 0.5;
    setRGB(120 * k, 0, 180 * k);
  } else {
    breathRGB(80, 0, 0);
  }

  if (detectSound()) {
    state = S_WAKE;
    stateT = millis();
    setRGB(0, 0, 200);
    playWake();
    tft.fillScreen(C_BG);
  }
}

void handleWake() {
  if (millis() - lastFrameMs > 70) {
    lastFrameMs = millis();
    animFrame++;
    drawWake();
  }
  if (millis() - stateT > 1500) {
    state = S_WAIT_FP;
    stateT = millis();
    tft.fillScreen(C_BG);
    animFrame = 0;
  }
}

void handleWaitFp() {
  if (millis() - lastFrameMs > 70) {
    lastFrameMs = millis();
    animFrame++;
    drawWaitFp();
  }
  uint8_t v = (sin(millis() / 150.0) + 1.0) * 127;
  setRGB(0, 0, v);

  int r = fpMatch();
  if (r >= 0) {
    lastMatchedId = (uint8_t)r;
    failStreak = 0;
    state = S_OK;
    stateT = millis();
    tft.fillScreen(C_BG);
  } else if (r == -2) {
    failStreak++;
    state = S_FAIL;
    stateT = millis();
    tft.fillScreen(C_BG);
  }
  if (millis() - stateT > 15000) {
    Serial.println(F("Timeout"));
    if (!silentMode) playTimeout();
    // 没匹配且没指纹尝试 -> 门铃
    state = S_DOORBELL;
    stateT = millis();
    tft.fillScreen(C_BG);
  }
}

void handleOk() {
  setRGB(0, 220, 0);
  drawOk(lastMatchedId);
  digitalWrite(RELAY_PIN, HIGH);
  unlockCount++;
  saveUnlockCount();
  logUnlock(lastMatchedId);
  petInteract(true);

  // 选择旋律
  uint8_t mel = 0;
  char nm[10]; uint8_t r, g, b;
  getUser(lastMatchedId, nm, &r, &g, &b, &mel);

  if (!silentMode) {
    if (unlockCount % 5 == 0) playEaster();
    else                      playSuccess(mel);
    playISD1820();
  }
  state = S_UNLOCKED;
  stateT = millis();
  tft.fillScreen(C_BG);
}

void handleFail() {
  setRGB(220, 0, 0);
  drawFail();
  if (!silentMode) playFail();
  petInteract(false);
  delay(1300);

  if (failStreak >= 3) {
    state = S_LOCKOUT;
    stateT = millis();
    lockoutStartMs = millis();
    tft.fillScreen(C_BG);
  } else {
    state = S_IDLE;
    stateT = millis();
    tft.fillScreen(C_BG);
  }
}

void handleUnlocked() {
  if (millis() - lastFrameMs > 100) {
    lastFrameMs = millis();
    drawUnlocked();
  }
  long remain = 5000L - (long)(millis() - stateT);
  if (remain < 0) remain = 0;
  uint8_t g = map(remain, 0, 5000, 30, 220);
  setRGB(0, g, 0);

  if (millis() - stateT > 5000) {
    digitalWrite(RELAY_PIN, LOW);
    if (!silentMode) playLockTone();
    // 显示该用户专属留言
    uint8_t mi = findMessageForUser(lastMatchedId, 0);
    if (mi != 0xFF) {
      markRead(mi);
      refreshUnreadFlag();
    }
    state = S_IDLE;
    stateT = millis();
    tft.fillScreen(C_BG);
  }
}

void handleLockout() {
  if (millis() - lastFrameMs > 100) {
    lastFrameMs = millis();
    drawLockout();
  }
  // 红灯爆闪
  uint8_t v = ((millis() / 120) % 2) ? 255 : 0;
  setRGB(v, 0, 0);

  if (millis() - lockoutStartMs > 30000UL) {
    failStreak = 0;
    state = S_IDLE;
    stateT = millis();
    tft.fillScreen(C_BG);
  }
}

void handleDoorbell() {
  drawDoorbell();
  if (!silentMode) playDoorbell();
  delay(1500);
  state = S_IDLE;
  stateT = millis();
  tft.fillScreen(C_BG);
}

// ==================== Sound Detect ====================
bool detectSound() {
  if (digitalRead(MIC_DO_PIN) == LOW) {
    delay(25);
    if (digitalRead(MIC_DO_PIN) == LOW) return true;
  }
  int peak = 0;
  for (uint8_t i = 0; i < 24; i++) {
    int v = analogRead(SOUND_AO);
    int d = v - 512; if (d < 0) d = -d;
    if (d > peak) peak = d;
    delayMicroseconds(180);
  }
  return peak > 100;
}

// ==================== Fingerprint ====================
int fpMatch() {
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  if (finger.fingerFastSearch() == FINGERPRINT_OK) return finger.fingerID;
  return -2;
}

// ==================== Drawing ====================
void drawBoot() {
  tft.fillScreen(C_BG);
  // 扫描线
  for (int y = 0; y < TFT_H; y += 4) {
    tft.drawFastHLine(0, y, TFT_W, C_DIM);
  }
  drawLockIcon(120, 50, 80, 110, false, C_ACCENT);
  tft.setTextColor(C_FG);
  tft.setTextSize(3);
  tft.setCursor(40, 175);
  tft.print(F("Smart Door Lock"));
  tft.setTextSize(1);
  tft.setTextColor(C_DIM);
  tft.setCursor(100, 215);
  tft.print(F("Nano ATmega328  v2.0"));
}

void drawIdle() {
  // 用局部刷新避免闪烁: 只清动态区
  drawHeaderBar(F("- LOCKED -"), C_ERR);

  // 锁图标 (左侧)
  static uint8_t lastDot = 99;
  uint8_t dot = (animFrame / 8) % 4;
  if (dot != lastDot) {
    lastDot = dot;
    tft.fillRect(40, 50, 100, 130, C_BG);
    drawLockIcon(50, 60, 70, 100, false, C_ERR);
  }

  // 中间状态文字
  tft.fillRect(140, 90, 120, 30, C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_DIM);
  tft.setCursor(150, 100);
  tft.print(F("Speak..."));
  for (uint8_t i = 0; i < dot; i++) {
    tft.fillCircle(255 + i * 8, 108, 2, C_FG);
  }

  // 桌面宠物 (右侧区域)
  drawPet(petX, petY, petEmotion, petFrame);

  // 未读消息提示
  if (hasUnreadMsg) {
    tft.fillRect(280, 5, 35, 18, C_PURPLE);
    tft.setTextColor(C_FG);
    tft.setTextSize(1);
    tft.setCursor(284, 10);
    tft.print(F("MSG!"));
  }

  // 留言板滚动条
  drawMsgBanner();
}

void drawWake() {
  drawHeaderBar(F("Voice Wake"), C_WAKE);
  // 麦克风扩散动画 - 中心圆 + 三圈
  int cx = TFT_W / 2, cy = TFT_H / 2;
  static int lastR = 0;
  int r = 20 + (animFrame * 6) % 80;
  // 抹掉旧圈
  if (lastR != r) {
    tft.drawCircle(cx, cy, lastR, C_BG);
    tft.drawCircle(cx, cy, lastR + 1, C_BG);
    lastR = r;
  }
  tft.fillCircle(cx, cy, 18, C_WAKE);
  tft.fillRect(cx - 4, cy + 16, 8, 14, C_WAKE);
  tft.drawFastHLine(cx - 14, cy + 32, 28, C_WAKE);
  tft.drawCircle(cx, cy, r, C_WAKE);
  tft.drawCircle(cx, cy, r + 1, C_WAKE);
}

void drawWaitFp() {
  drawHeaderBar(F("Place Finger"), C_ACCENT);
  int cx = TFT_W / 2, cy = 110;
  drawFpIcon(cx - 35, cy - 45, 35, C_ACCENT, animFrame);

  // 双向流动进度条
  int barX = 40, barY = 195, barW = 240, barH = 14;
  tft.drawRect(barX, barY, barW, barH, C_FG);
  tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, C_BG);
  int p = (animFrame * 5) % barW;
  tft.fillRect(barX + 1, barY + 1, p, barH - 2, C_ACCENT);
  int back = barW - p;
  if (back > 0) tft.fillRect(barX + 1 + back, barY + 1, p / 3, barH - 2, C_ACCENT);
}

void drawOk(uint8_t id) {
  drawHeaderBar(F("ACCESS GRANTED"), C_OK);

  char name[10]; uint8_t r, g, b, mel;
  getUser(id, name, &r, &g, &b, &mel);
  uint16_t userColor = tft.color565(r, g, b);
  if (r == 0 && g == 0 && b == 0) userColor = C_OK;

  // 锁打开图标
  drawLockIcon(40, 50, 70, 100, true, C_OK);

  // 大字欢迎
  tft.setTextSize(3);
  tft.setTextColor(userColor);
  tft.setCursor(140, 65);
  tft.print(F("HI!"));

  tft.setTextSize(2);
  tft.setTextColor(C_FG);
  tft.setCursor(140, 105);
  if (name[0] != 0 && name[0] != 0xFF) {
    tft.print(name);
  } else {
    tft.print(F("User "));
    tft.print(id);
  }

  // 头像方块 (色块 + 首字母)
  tft.fillRoundRect(140, 140, 50, 50, 6, userColor);
  tft.setTextColor(C_FG);
  tft.setTextSize(4);
  char init = (name[0] != 0 && name[0] != 0xFF) ? name[0] : ('0' + id);
  if (init >= 'a' && init <= 'z') init -= 32;
  tft.setCursor(155, 150);
  tft.print(init);

  // 烟花
  for (uint8_t i = 0; i < 12; i++) {
    drawSparkle(random(140, 310), random(40, 220), random(3, 7),
                (i & 1) ? C_PET : C_PINK);
  }

  // 显示该用户的专属留言
  uint8_t mi = findMessageForUser(id, 0);
  if (mi != 0xFF) {
    char buf[81];
    uint16_t base = EE_MSG_BASE + (uint16_t)mi * EE_MSG_SZ;
    for (uint8_t i = 0; i < 80; i++) buf[i] = EEPROM.read(base + 8 + i);
    buf[80] = 0;
    tft.fillRect(20, 200, 280, 36, C_PURPLE);
    tft.drawRect(20, 200, 280, 36, C_FG);
    tft.setTextColor(C_FG);
    tft.setTextSize(1);
    tft.setCursor(28, 208);
    tft.print(F("[MSG] "));
    tft.print(buf);
  }
}

void drawFail() {
  drawHeaderBar(F("ACCESS DENIED"), C_ERR);
  for (uint8_t shake = 0; shake < 4; shake++) {
    int dx = (shake & 1) ? 6 : -6;
    tft.fillRect(60, 50, 200, 130, C_BG);
    drawXMark(140 + dx, 90, 50);
    delay(120);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_ERR);
  tft.setCursor(70, 200);
  tft.print(F("Try Again ("));
  tft.print(3 - failStreak);
  tft.print(F(" left)"));
}

void drawUnlocked() {
  drawHeaderBar(F("DOOR OPEN"), C_OK);
  drawLockIcon(40, 50, 70, 100, true, C_OK);

  long remain = 5L - (long)((millis() - stateT) / 1000);
  if (remain < 0) remain = 0;

  tft.fillRect(140, 80, 160, 40, C_BG);
  tft.setTextSize(3);
  tft.setTextColor(C_OK);
  tft.setCursor(160, 80);
  tft.print(F("OPEN "));
  tft.print((int)remain);
  tft.print('s');

  // 进度条
  int barX = 30, barY = 200, barW = 260, barH = 14;
  tft.drawRect(barX, barY, barW, barH, C_FG);
  long prog = map((long)(millis() - stateT), 0, 5000, barW - 2, 0);
  if (prog < 0) prog = 0;
  tft.fillRect(barX + 1, barY + 1, prog, barH - 2, C_OK);
  tft.fillRect(barX + 1 + prog, barY + 1, barW - 2 - prog, barH - 2, C_BG);

  // 心心
  tft.fillCircle(150 - 4, 145, 4, C_PINK);
  tft.fillCircle(150 + 4, 145, 4, C_PINK);
  tft.fillTriangle(150 - 8, 145, 150 + 8, 145, 150, 156, C_PINK);
}

void drawLockout() {
  drawHeaderBar(F("!! LOCKED OUT !!"), C_ERR);
  long remain = 30L - (long)((millis() - lockoutStartMs) / 1000);
  if (remain < 0) remain = 0;

  if (((millis() / 200) % 2) == 0) {
    tft.fillRect(40, 60, 240, 110, C_ERR);
    tft.setTextColor(C_FG);
  } else {
    tft.fillRect(40, 60, 240, 110, C_BG);
    tft.setTextColor(C_ERR);
  }
  tft.setTextSize(4);
  tft.setCursor(80, 80);
  tft.print(F("BRUTE"));
  tft.setCursor(70, 125);
  tft.print(F("FORCE"));

  tft.fillRect(60, 185, 200, 30, C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_ERR);
  tft.setCursor(70, 195);
  tft.print(F("Wait "));
  tft.print((int)remain);
  tft.print(F("s"));
}

void drawDoorbell() {
  drawHeaderBar(F("DING DONG"), C_PET);
  tft.setTextSize(3);
  tft.setTextColor(C_PET);
  tft.setCursor(60, 90);
  tft.print(F("Visitor at"));
  tft.setCursor(120, 130);
  tft.print(F("Door"));
}

void drawHeaderBar(const __FlashStringHelper* title, uint16_t color) {
  tft.fillRect(0, 0, TFT_W, 28, color);
  tft.setTextSize(2);
  tft.setTextColor(C_FG);
  tft.setCursor(8, 6);
  tft.print(title);
  // 右上角时间/统计
  tft.setTextSize(1);
  tft.setCursor(220, 4);
  tft.print(F("Up:"));
  tft.print(uptimeHours);
  tft.print('h');
  tft.setCursor(220, 16);
  tft.print(F("Cnt:"));
  tft.print(unlockCount);
}

void drawLockIcon(int x, int y, int w, int h, bool open, uint16_t col) {
  int bodyH = h * 6 / 10;
  int bodyY = y + h - bodyH;
  tft.fillRoundRect(x, bodyY, w, bodyH, 6, col);
  // 钥匙孔
  tft.fillCircle(x + w / 2, bodyY + bodyH / 3, 5, C_BG);
  tft.fillRect(x + w / 2 - 2, bodyY + bodyH / 3, 4, bodyH / 3, C_BG);
  // 锁环
  int shW = w * 6 / 10;
  int shH = h * 5 / 10;
  int sx = x + (w - shW) / 2;
  if (!open) {
    tft.drawRoundRect(sx, y, shW, shH + 4, shW / 2, col);
    tft.drawRoundRect(sx + 1, y, shW - 2, shH + 4, shW / 2, col);
    tft.drawRoundRect(sx + 2, y, shW - 4, shH + 4, shW / 2, col);
  } else {
    int ox = sx + shW - 6;
    tft.drawRoundRect(ox, y - 4, shW, shH + 4, shW / 2, col);
    tft.drawRoundRect(ox + 1, y - 4, shW - 2, shH + 4, shW / 2, col);
    tft.drawRoundRect(ox + 2, y - 4, shW - 4, shH + 4, shW / 2, col);
  }
}

void drawFpIcon(int x, int y, int r, uint16_t col, uint8_t f) {
  int cx = x + r, cy = y + r;
  tft.drawRoundRect(x - 4, y - 4, r * 2 + 8, r * 2 + 8, 8, col);
  // 旋转的指纹纹路 (椭圆)
  for (int rr = 8; rr < r; rr += 5) {
    int sa = (f * 12 + rr * 6) % 360;
    for (int a = 0; a < 240; a += 10) {
      float rad = (sa + a) * 0.01745;
      int px = cx + cos(rad) * rr;
      int py = cy + sin(rad) * (rr * 0.75);
      if (px >= 0 && px < TFT_W && py >= 0 && py < TFT_H)
        tft.drawPixel(px, py, col);
    }
  }
}

void drawXMark(int x, int y, int s) {
  for (int t = 0; t < 4; t++) {
    tft.drawLine(x - s + t, y, x + s, y + s * 2 - t, C_ERR);
    tft.drawLine(x + s - t, y, x - s, y + s * 2 - t, C_ERR);
  }
}

void drawSparkle(int x, int y, int s, uint16_t col) {
  tft.drawFastHLine(x - s, y, s * 2, col);
  tft.drawFastVLine(x, y - s, s * 2, col);
  tft.drawPixel(x - s + 1, y - s + 1, col);
  tft.drawPixel(x + s - 1, y - s + 1, col);
  tft.drawPixel(x - s + 1, y + s - 1, col);
  tft.drawPixel(x + s - 1, y + s - 1, col);
}

// ==================== 桌面宠物 ====================
// 32x32 位置, 用基本图形画一只猫
void drawPet(int x, int y, uint8_t emotion, uint8_t frame) {
  // 清掉宠物移动尾迹
  static int16_t lastX = -100, lastY = -100;
  if (lastX != x || lastY != y) {
    if (lastX > -100) {
      tft.fillRect(lastX - 18, lastY - 18, 40, 40, C_BG);
    }
    lastX = x; lastY = y;
  }

  uint16_t bodyCol = C_PET;
  if (emotion == 1) bodyCol = C_DIM;       // sleepy
  else if (emotion == 2) bodyCol = C_PINK; // excited
  else if (emotion == 3) bodyCol = C_PURPLE;// sad

  // 身体
  tft.fillRoundRect(x - 12, y - 6, 24, 18, 6, bodyCol);
  // 头
  tft.fillCircle(x, y - 12, 10, bodyCol);
  // 耳朵
  if (emotion == 3) {
    // 下垂
    tft.fillTriangle(x - 9, y - 16, x - 4, y - 12, x - 8, y - 8, bodyCol);
    tft.fillTriangle(x + 9, y - 16, x + 4, y - 12, x + 8, y - 8, bodyCol);
  } else {
    tft.fillTriangle(x - 9, y - 22, x - 4, y - 16, x - 11, y - 14, bodyCol);
    tft.fillTriangle(x + 9, y - 22, x + 4, y - 16, x + 11, y - 14, bodyCol);
  }
  // 眼睛
  if (emotion == 1) {
    // sleepy: 短横线
    tft.drawFastHLine(x - 5, y - 12, 3, C_BG);
    tft.drawFastHLine(x + 2, y - 12, 3, C_BG);
    // Z (睡觉符号)
    tft.setTextColor(C_FG);
    tft.setTextSize(1);
    tft.setCursor(x + 14, y - 24);
    tft.print('Z');
  } else if (emotion == 2) {
    // excited: 星眼
    tft.fillCircle(x - 4, y - 12, 2, C_FG);
    tft.fillCircle(x + 4, y - 12, 2, C_FG);
    tft.drawPixel(x - 6, y - 12, C_FG);
    tft.drawPixel(x - 2, y - 12, C_FG);
    tft.drawPixel(x + 2, y - 12, C_FG);
    tft.drawPixel(x + 6, y - 12, C_FG);
  } else if (emotion == 3) {
    // sad: T_T
    tft.drawLine(x - 6, y - 14, x - 2, y - 10, C_BG);
    tft.drawLine(x - 2, y - 14, x - 6, y - 10, C_BG);
    tft.drawLine(x + 2, y - 14, x + 6, y - 10, C_BG);
    tft.drawLine(x + 6, y - 14, x + 2, y - 10, C_BG);
    // 泪
    tft.fillCircle(x - 7, y - 8, 1, C_WAKE);
  } else {
    // happy: 圆眼
    tft.fillCircle(x - 4, y - 12, 2, C_BG);
    tft.fillCircle(x + 4, y - 12, 2, C_BG);
  }
  // 嘴
  if (emotion == 2 || emotion == 0) {
    tft.drawPixel(x - 1, y - 6, C_BG);
    tft.drawPixel(x + 1, y - 6, C_BG);
    tft.drawPixel(x, y - 5, C_BG);
  }
  // 尾巴 (摇摆)
  int tailDX = (frame & 1) ? 4 : -4;
  tft.drawLine(x + 12, y - 2, x + 18 + tailDX, y - 8, bodyCol);
  tft.drawLine(x + 12, y - 1, x + 18 + tailDX, y - 7, bodyCol);

  // 心情泡泡 (excited 时)
  if (emotion == 2 && (frame & 2)) {
    tft.fillCircle(x - 14, y - 22, 3, C_PINK);
    tft.fillCircle(x - 11, y - 22, 3, C_PINK);
    tft.fillTriangle(x - 17, y - 22, x - 8, y - 22, x - 12, y - 16, C_PINK);
  }
}

// ==================== 留言滚动 ====================
void drawMsgBanner() {
  // 在屏幕底部画一行滚动的留言
  // 找第一条 active 消息
  if (millis() - lastBannerMs < 60) return;
  lastBannerMs = millis();
  bannerX -= 2;

  char buf[81] = {0};
  bool found = false;
  for (uint8_t i = 0; i < EE_MSG_MAX; i++) {
    uint16_t base = EE_MSG_BASE + (uint16_t)i * EE_MSG_SZ;
    if (EEPROM.read(base) == MSG_ACTIVE_FLAG) {
      for (uint8_t k = 0; k < 80; k++) buf[k] = EEPROM.read(base + 8 + k);
      buf[80] = 0;
      found = true;
      break;
    }
  }
  if (!found) {
    strcpy_P(buf, PSTR("Speak to wake * No messages * "));
  }

  tft.fillRect(0, TFT_H - 18, TFT_W, 18, C_DIM);
  tft.setTextColor(C_FG);
  tft.setTextSize(1);
  tft.setCursor(bannerX, TFT_H - 14);
  tft.print(buf);
  // 末尾再画一份做循环
  int textW = strlen(buf) * 6;
  tft.setCursor(bannerX + textW + 30, TFT_H - 14);
  tft.print(buf);
  if (bannerX < -textW - 30) bannerX = TFT_W;
}

// ==================== RGB ====================
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  if (RGB_COMMON_ANODE) { r = 255 - r; g = 255 - g; b = 255 - b; }
  analogWrite(RGB_R_PIN, r);
  analogWrite(RGB_G_PIN, g);
  analogWrite(RGB_B_PIN, b);
}

void breathRGB(uint8_t r, uint8_t g, uint8_t b) {
  float k = (sin(millis() / 800.0) + 1.0) * 0.5;
  setRGB((uint8_t)(r * k), (uint8_t)(g * k), (uint8_t)(b * k));
}

// ==================== 蜂鸣器 ====================
void playTone(int f, int d) {
  if (silentMode) return;     // 静音直接跳过, 不阻塞
  if (f == 0) { noTone(BUZZER_PIN); delay(d); }
  else { tone(BUZZER_PIN, f, d); delay(d + 15); }
  noTone(BUZZER_PIN);
}

void playStartup() {
  int n[] = {N_C4, N_E4, N_G4, N_C5, N_E5};
  int d[] = {110, 110, 110, 110, 240};
  for (uint8_t i = 0; i < 5; i++) playTone(n[i], d[i]);
}
void playWake()    { playTone(N_E5, 90);  playTone(N_C5, 130); }
void playFail()    { int n[]={500,400,300,200,200}, d[]={150,150,150,200,200}; for(uint8_t i=0;i<5;i++) playTone(n[i],d[i]); }
void playLockTone(){ playTone(N_G4, 100); playTone(N_E4, 100); playTone(N_C4, 200); }
void playTimeout() { playTone(N_A4, 70);  playTone(N_A4, 70); }
void playEnrollTone(){ playTone(N_D5, 150); }

void playSuccess(uint8_t mel) {
  switch (mel & 3) {
    case 1: { // 海豚音 / 上滑
      int n[]={N_C5,N_D5,N_E5,N_G5,N_C6}; int d[]={80,80,80,120,200};
      for(uint8_t i=0;i<5;i++) playTone(n[i],d[i]); break;
    }
    case 2: { // 三连音
      int n[]={N_E5,N_E5,N_E5,N_G5}; int d[]={100,100,100,250};
      for(uint8_t i=0;i<4;i++) playTone(n[i],d[i]); break;
    }
    case 3: { // 反向 (低沉感)
      int n[]={N_G4,N_C5,N_E5,N_C5}; int d[]={120,120,160,200};
      for(uint8_t i=0;i<4;i++) playTone(n[i],d[i]); break;
    }
    default: { // 经典上行
      int n[]={N_C5,N_E5,N_G5,N_C6,N_G5,N_C6}; int d[]={100,100,100,160,80,240};
      for(uint8_t i=0;i<6;i++) playTone(n[i],d[i]);
    }
  }
}

void playEaster() {
  int n[] = {N_E5, N_E5, 0, N_E5, 0, N_C5, N_E5, N_G5, 0, N_G4};
  int d[] = {110, 110, 70, 110, 70, 110, 110, 220, 70, 220};
  for (uint8_t i = 0; i < 10; i++) playTone(n[i], d[i]);
}

void playDoorbell() {
  playTone(N_E5, 220);
  playTone(N_C5, 360);
}

void playISD1820() {
  digitalWrite(ISD1820_PIN, HIGH);
  delay(100);
  digitalWrite(ISD1820_PIN, LOW);
}

// ==================== EEPROM ====================
uint32_t readU32(uint16_t addr) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 4; i++) v |= ((uint32_t)EEPROM.read(addr + i)) << (i * 8);
  return v;
}
void writeU32(uint16_t addr, uint32_t v) {
  for (uint8_t i = 0; i < 4; i++) EEPROM.update(addr + i, (v >> (i * 8)) & 0xFF);
}

void eepromInit() {
  uint32_t magic = readU32(EE_MAGIC0);
  if (magic != EE_MAGIC_VAL) {
    Serial.println(F("EEPROM init"));
    // 全部填 0
    for (uint16_t a = 0; a < 1024; a++) EEPROM.update(a, 0);
    writeU32(EE_MAGIC0, EE_MAGIC_VAL);
    EEPROM.update(EE_THEME, 0);
    EEPROM.update(EE_SILENT, 0);
    EEPROM.update(EE_LOG_HEAD, 0);
    // 默认用户 1 = "Master" 绿色
    char def[] = "Master";
    setUser(1, def, 0, 200, 80, 0);
  }
}

void loadConfig() {
  uint8_t lo = EEPROM.read(EE_UNLOCK_LO);
  uint8_t hi = EEPROM.read(EE_UNLOCK_HI);
  unlockCount = (uint16_t)lo | ((uint16_t)hi << 8);
  themeId = EEPROM.read(EE_THEME);
  silentMode = EEPROM.read(EE_SILENT) != 0;
  petXP = ((uint16_t)EEPROM.read(EE_PET_XP_LO)) | ((uint16_t)EEPROM.read(EE_PET_XP_HI) << 8);
  petLastHour = ((uint16_t)EEPROM.read(EE_PET_LAST)) | ((uint16_t)EEPROM.read(EE_PET_LAST + 1) << 8);
}

void saveUnlockCount() {
  EEPROM.update(EE_UNLOCK_LO, unlockCount & 0xFF);
  EEPROM.update(EE_UNLOCK_HI, (unlockCount >> 8) & 0xFF);
}

// ==================== Users ====================
void getUser(uint8_t id, char* nameOut, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* mel) {
  nameOut[0] = 0;
  *r = 0; *g = 0; *b = 0; *mel = 0;
  if (id == 0 || id > EE_USER_MAX) return;
  uint16_t base = EE_USER_BASE + (uint16_t)(id - 1) * EE_USER_SZ;
  if (EEPROM.read(base) != USER_ACTIVE_FLAG) return;
  for (uint8_t i = 0; i < 9; i++) nameOut[i] = EEPROM.read(base + 1 + i);
  nameOut[9] = 0;
  *r = EEPROM.read(base + 10);
  *g = EEPROM.read(base + 11);
  *b = EEPROM.read(base + 12);
  *mel = EEPROM.read(base + 13);
}

bool setUser(uint8_t id, const char* name, uint8_t r, uint8_t g, uint8_t b, uint8_t mel) {
  if (id == 0 || id > EE_USER_MAX) return false;
  uint16_t base = EE_USER_BASE + (uint16_t)(id - 1) * EE_USER_SZ;
  EEPROM.update(base, USER_ACTIVE_FLAG);
  // 全部写完 8 字节, 短名字补 0, 防止旧名字残留
  bool done = false;
  for (uint8_t i = 0; i < 8; i++) {
    char c = 0;
    if (!done) {
      c = name[i];
      if (c == 0) done = true;
    }
    EEPROM.update(base + 1 + i, (uint8_t)c);
  }
  EEPROM.update(base + 9, 0);
  EEPROM.update(base + 10, r);
  EEPROM.update(base + 11, g);
  EEPROM.update(base + 12, b);
  EEPROM.update(base + 13, mel & 3);
  return true;
}

void listUsers() {
  Serial.println(F("ID | Name     | Color       | Mel"));
  for (uint8_t i = 1; i <= EE_USER_MAX; i++) {
    char nm[10]; uint8_t r, g, b, m;
    getUser(i, nm, &r, &g, &b, &m);
    if (nm[0] == 0) continue;
    Serial.print(i); Serial.print(F(" | "));
    Serial.print(nm); Serial.print(F(" | "));
    Serial.print(r); Serial.print(','); Serial.print(g); Serial.print(',');
    Serial.print(b); Serial.print(F("  | "));
    Serial.println(m);
  }
}

// ==================== Messages ====================
bool addMessage(uint8_t target, uint8_t flags, uint16_t ttl, const char* text) {
  // 找第一个空槽
  for (uint8_t i = 0; i < EE_MSG_MAX; i++) {
    uint16_t base = EE_MSG_BASE + (uint16_t)i * EE_MSG_SZ;
    if (EEPROM.read(base) != MSG_ACTIVE_FLAG) {
      EEPROM.update(base, MSG_ACTIVE_FLAG);
      EEPROM.update(base + 1, target);
      EEPROM.update(base + 2, 0);
      EEPROM.update(base + 3, flags);
      EEPROM.update(base + 4, uptimeHours & 0xFF);
      EEPROM.update(base + 5, (uptimeHours >> 8) & 0xFF);
      EEPROM.update(base + 6, ttl & 0xFF);
      EEPROM.update(base + 7, (ttl >> 8) & 0xFF);
      uint8_t k = 0;
      while (k < 79 && text[k]) {
        EEPROM.update(base + 8 + k, text[k]);
        k++;
      }
      EEPROM.update(base + 8 + k, 0);
      refreshUnreadFlag();
      return true;
    }
  }
  return false;
}

void deleteMessage(uint8_t idx) {
  if (idx >= EE_MSG_MAX) return;
  EEPROM.update(EE_MSG_BASE + (uint16_t)idx * EE_MSG_SZ, 0);
  refreshUnreadFlag();
}

void clearMessages() {
  for (uint8_t i = 0; i < EE_MSG_MAX; i++) deleteMessage(i);
}

void listMessages() {
  Serial.println(F("idx tgt rd  text"));
  for (uint8_t i = 0; i < EE_MSG_MAX; i++) {
    uint16_t base = EE_MSG_BASE + (uint16_t)i * EE_MSG_SZ;
    if (EEPROM.read(base) != MSG_ACTIVE_FLAG) continue;
    Serial.print(i); Serial.print(F("   "));
    Serial.print(EEPROM.read(base + 1)); Serial.print(F("   "));
    Serial.print(EEPROM.read(base + 2) ? F("Y ") : F("N "));
    for (uint8_t k = 0; k < 80; k++) {
      char c = EEPROM.read(base + 8 + k);
      if (!c) break;
      Serial.write(c);
    }
    Serial.println();
  }
}

uint8_t findMessageForUser(uint8_t userId, uint8_t startFrom) {
  for (uint8_t i = startFrom; i < EE_MSG_MAX; i++) {
    uint16_t base = EE_MSG_BASE + (uint16_t)i * EE_MSG_SZ;
    if (EEPROM.read(base) != MSG_ACTIVE_FLAG) continue;
    uint8_t target = EEPROM.read(base + 1);
    if (target != 0 && target != userId) continue;
    if (EEPROM.read(base + 2) != 0) continue;  // 已读
    // TTL
    uint16_t created = EEPROM.read(base + 4) | ((uint16_t)EEPROM.read(base + 5) << 8);
    uint16_t ttl = EEPROM.read(base + 6) | ((uint16_t)EEPROM.read(base + 7) << 8);
    if (ttl != 0xFFFF && (uptimeHours - created) >= ttl) {
      EEPROM.update(base, 0);  // 过期清除
      continue;
    }
    return i;
  }
  return 0xFF;
}

void markRead(uint8_t idx) {
  if (idx >= EE_MSG_MAX) return;
  EEPROM.update(EE_MSG_BASE + (uint16_t)idx * EE_MSG_SZ + 2, 1);
}

void refreshUnreadFlag() {
  hasUnreadMsg = false;
  for (uint8_t i = 0; i < EE_MSG_MAX; i++) {
    uint16_t base = EE_MSG_BASE + (uint16_t)i * EE_MSG_SZ;
    if (EEPROM.read(base) == MSG_ACTIVE_FLAG && EEPROM.read(base + 2) == 0) {
      hasUnreadMsg = true;
      break;
    }
  }
}

// ==================== Log ====================
void logUnlock(uint8_t id) {
  uint8_t head = EEPROM.read(EE_LOG_HEAD) % EE_LOG_MAX;
  uint16_t addr = EE_LOG_BASE + (uint16_t)head * EE_LOG_SZ;
  EEPROM.update(addr, id);
  EEPROM.update(addr + 1, uptimeHours & 0xFF);
  EEPROM.update(addr + 2, (uptimeHours >> 8) & 0xFF);
  EEPROM.update(addr + 3, 0);
  EEPROM.update(addr + 4, 0);
  EEPROM.update(addr + 5, 0);
  EEPROM.update(EE_LOG_HEAD, (head + 1) % EE_LOG_MAX);
}

void printLog() {
  Serial.println(F("== Unlock Log =="));
  uint8_t head = EEPROM.read(EE_LOG_HEAD) % EE_LOG_MAX;
  for (uint8_t i = 0; i < EE_LOG_MAX; i++) {
    uint8_t idx = (head + EE_LOG_MAX - 1 - i) % EE_LOG_MAX;
    uint16_t addr = EE_LOG_BASE + (uint16_t)idx * EE_LOG_SZ;
    uint8_t id = EEPROM.read(addr);
    if (id == 0) continue;
    uint16_t h = EEPROM.read(addr + 1) | ((uint16_t)EEPROM.read(addr + 2) << 8);
    Serial.print(F("ID=")); Serial.print(id);
    Serial.print(F(" hour=")); Serial.print(h);
    Serial.print(F(" ago=")); Serial.print(uptimeHours - h);
    Serial.println('h');
  }
}

// ==================== Uptime/Pet ====================
void updateUptime() {
  if (millis() - lastHourMs >= 3600000UL) {
    lastHourMs += 3600000UL;
    uptimeHours++;
  }
}

void updatePet() {
  // 移动 (在屏幕右半部分游走)
  if (millis() - lastPetMs > 200) {
    lastPetMs = millis();
    petFrame++;
    if (state == S_IDLE && petEmotion != 1) {
      petX += petDX;
      if (petX > 295) { petDX = -1; }
      if (petX < 175) { petDX = 1; }
    }
  }
  // 每分钟思考一次情绪
  if (millis() - lastPetThinkMs > 60000UL) {
    lastPetThinkMs = millis();
    recomputePetEmotion();
  }
}

void petInteract(bool positive) {
  petLastHour = uptimeHours;
  EEPROM.update(EE_PET_LAST, petLastHour & 0xFF);
  EEPROM.update(EE_PET_LAST + 1, (petLastHour >> 8) & 0xFF);
  if (positive) {
    if (petXP < 60000) petXP += 10;
    petEmotion = 2;  // excited
  } else {
    petEmotion = 3;  // sad
  }
  EEPROM.update(EE_PET_XP_LO, petXP & 0xFF);
  EEPROM.update(EE_PET_XP_HI, (petXP >> 8) & 0xFF);
}

void recomputePetEmotion() {
  uint16_t hoursIdle = uptimeHours - petLastHour;
  if (hoursIdle > 24) petEmotion = 1;       // 没人理 -> 睡觉
  else if (hoursIdle > 8) petEmotion = 3;   // 有点伤心
  else petEmotion = 0;                       // happy
}

// ==================== Serial Commands ====================
char cmdBuf[96];
uint8_t cmdLen = 0;

void processCmd(char* line) {
  // 剥离开头空格
  while (*line == ' ') line++;
  if (line[0] == 'h' && line[1] == 'e') { // help
    Serial.println(F("e | d | u<id> <name> <r> <g> <b> <mel> | ul"));
    Serial.println(F("m <text> | m@<id> <text> | m! <text> | ml | md<idx> | mc"));
    Serial.println(F("log | silent | stats"));
    return;
  }
  if (line[0] == 'e') {
    enrollMode = true;
    Serial.print(F("Enroll ID=")); Serial.println(enrollId);
    tft.fillScreen(C_BG);
    drawHeaderBar(F("ENROLL MODE"), C_ACCENT);
    tft.setTextSize(2); tft.setTextColor(C_FG);
    tft.setCursor(20, 80); tft.print(F("Place finger"));
    tft.setCursor(20, 110); tft.print(F("(step 1)"));
    tft.setCursor(20, 140); tft.print(F("ID: ")); tft.print(enrollId);
    setRGB(0, 0, 200);
    return;
  }
  if (line[0] == 'd' && (line[1] == 0 || line[1] == ' ' || line[1] == '\r')) {
    Serial.println(F("Clear all FPs"));
    finger.emptyDatabase();
    enrollId = 1;
    return;
  }
  if (line[0] == 'u' && line[1] == 'l') { listUsers(); return; }
  if (line[0] == 'u' && line[1] == ' ') {
    // u <id> <name> <r> <g> <b> <mel>
    char* p = line + 2;
    uint8_t id = atoi(p);
    while (*p && *p != ' ') p++; if (*p) p++;
    char name[9] = {0};
    uint8_t k = 0;
    while (*p && *p != ' ' && k < 8) name[k++] = *p++;
    name[k] = 0;
    while (*p == ' ') p++;
    uint8_t r = atoi(p); while (*p && *p != ' ') p++; if (*p) p++;
    uint8_t g = atoi(p); while (*p && *p != ' ') p++; if (*p) p++;
    uint8_t b = atoi(p); while (*p && *p != ' ') p++; if (*p) p++;
    uint8_t m = atoi(p);
    if (setUser(id, name, r, g, b, m)) {
      Serial.print(F("Set user ")); Serial.println(id);
    } else Serial.println(F("Bad id"));
    return;
  }
  if (line[0] == 'm' && line[1] == 'l') { listMessages(); return; }
  if (line[0] == 'm' && line[1] == 'c') { clearMessages(); Serial.println(F("Cleared")); return; }
  if (line[0] == 'm' && line[1] == 'd') {
    uint8_t idx = atoi(line + 2);
    deleteMessage(idx);
    Serial.print(F("Deleted ")); Serial.println(idx);
    return;
  }
  if (line[0] == 'm' && line[1] == '@') {
    char* p = line + 2;
    uint8_t id = atoi(p);
    while (*p && *p != ' ') p++; if (*p) p++;
    if (addMessage(id, 0, 0xFFFF, p)) Serial.println(F("Msg saved"));
    else Serial.println(F("Msg full"));
    return;
  }
  if (line[0] == 'm' && line[1] == '!') {
    if (addMessage(0, 1, 0xFFFF, line + 3)) Serial.println(F("Pinned"));
    return;
  }
  if (line[0] == 'm' && line[1] == ' ') {
    if (addMessage(0, 0, 0xFFFF, line + 2)) Serial.println(F("Msg saved"));
    return;
  }
  if (strncmp_P(line, PSTR("log"), 3) == 0) { printLog(); return; }
  if (strncmp_P(line, PSTR("silent"), 6) == 0) {
    silentMode = !silentMode;
    EEPROM.update(EE_SILENT, silentMode ? 1 : 0);
    Serial.print(F("Silent=")); Serial.println(silentMode);
    return;
  }
  if (strncmp_P(line, PSTR("stats"), 5) == 0) {
    Serial.print(F("Unlocks=")); Serial.println(unlockCount);
    Serial.print(F("Up h="));    Serial.println(uptimeHours);
    Serial.print(F("PetXP="));   Serial.println(petXP);
    Serial.print(F("Emotion=")); Serial.println(petEmotion);
    return;
  }
  if (line[0]) { Serial.print(F("?? ")); Serial.println(line); }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      cmdBuf[cmdLen] = 0;
      processCmd(cmdBuf);
      cmdLen = 0;
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

// ==================== Enrollment ====================
void handleEnroll() {
  static uint8_t step = 0;
  static unsigned long enrollT = 0;
  if (step == 0 && enrollT == 0) enrollT = millis();

  if (millis() - enrollT > 30000UL) {
    Serial.println(F("Enroll timeout"));
    enrollMode = false; step = 0; enrollT = 0;
    state = S_IDLE; stateT = millis();
    tft.fillScreen(C_BG);
    return;
  }

  if (step == 0) {
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz(1) == FINGERPRINT_OK) {
        Serial.println(F("Remove finger..."));
        tft.fillScreen(C_BG);
        drawHeaderBar(F("ENROLL"), C_ACCENT);
        tft.setTextSize(2); tft.setTextColor(C_FG);
        tft.setCursor(20, 100); tft.print(F("Remove finger"));
        playEnrollTone();
        unsigned long t0 = millis();
        while (finger.getImage() != FINGERPRINT_NOFINGER) {
          if (millis() - t0 > 8000UL) break;
          delay(80);
        }
        Serial.println(F("Place same finger"));
        tft.fillScreen(C_BG);
        drawHeaderBar(F("ENROLL"), C_ACCENT);
        tft.setTextSize(2); tft.setTextColor(C_FG);
        tft.setCursor(20, 80); tft.print(F("Place same"));
        tft.setCursor(20, 110); tft.print(F("finger again"));
        tft.setCursor(20, 140); tft.print(F("(step 2)"));
        step = 1;
        enrollT = millis();
      }
    }
  } else if (step == 1) {
    if (finger.getImage() == FINGERPRINT_OK) {
      bool ok = finger.image2Tz(2) == FINGERPRINT_OK
             && finger.createModel() == FINGERPRINT_OK
             && finger.storeModel(enrollId) == FINGERPRINT_OK;
      tft.fillScreen(C_BG);
      if (ok) {
        Serial.print(F("Enroll OK ID=")); Serial.println(enrollId);
        drawHeaderBar(F("ENROLL OK"), C_OK);
        tft.setTextSize(3); tft.setTextColor(C_OK);
        tft.setCursor(60, 90); tft.print(F("SAVED"));
        tft.setTextSize(2); tft.setTextColor(C_FG);
        tft.setCursor(80, 150); tft.print(F("ID: ")); tft.print(enrollId);
        playSuccess(0);
        setRGB(0, 220, 0);
        delay(1600);
        enrollId++;
      } else {
        Serial.println(F("Enroll failed"));
        drawHeaderBar(F("ENROLL FAIL"), C_ERR);
        tft.setTextSize(2); tft.setTextColor(C_ERR);
        tft.setCursor(40, 110); tft.print(F("Try again"));
        playFail();
        setRGB(220, 0, 0);
        delay(1500);
      }
      step = 0;
      enrollMode = false;
      enrollT = 0;
      state = S_IDLE; stateT = millis();
      tft.fillScreen(C_BG);
    }
  }
}
