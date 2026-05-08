# 🔐 智能指纹门锁系统 v2.0 (Smart Fingerprint Door Lock)

> 基于 **Arduino Nano (ATmega328P)** 的多模态智能门锁: 声音唤醒 → 指纹验证 → 电磁锁开关 + **2.2″ 彩色 TFT** + 桌面宠物 + 个性化欢迎 + EEPROM 留言板 + 暴力锁定 + 语音播报。

[![Arduino](https://img.shields.io/badge/Arduino-Nano%20ATmega328P-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Display](https://img.shields.io/badge/Display-ILI9341%202.2%22%20240x320-FF6B6B)](#)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](#license)
[![Status](https://img.shields.io/badge/Status-v2.0-brightgreen.svg)](#)

---

## ✨ v2.0 功能亮点

| 模块 | 行为 |
|:--|:--|
| 🎤 **声音唤醒** | KY-038 数字触发 + MAX9812 模拟峰值检测 |
| 👆 **指纹识别** | TM1026M (兼容 AS608), 串口 `e` 录入 / `d` 清空 |
| 🖥️ **彩色 TFT** | 2.2″ ILI9341 240×320, 横屏 UI, 锁屏/唤醒/解锁全套动画 |
| 🐱 **桌面宠物** | 屏幕右侧的小猫, **4 种情绪** (开心/兴奋/伤心/睡觉) |
| 👋 **个性化欢迎** | 每个指纹 ID 独立 **姓名 / RGB 颜色 / 旋律**, 解锁显示彩色头像 |
| 📝 **留言板** | EEPROM 持久化, 支持 **群发 / 定向 / 置顶 / TTL 过期 / 已读** |
| 🛡️ **暴力锁定** | 连续 3 次失败 → 30 秒锁定 + 红灯爆闪 |
| 🔔 **门铃模式** | 唤醒后 15s 没指纹 → 播 ding-dong + "Visitor" 提示 |
| 📊 **解锁日志** | 滚动存最近 10 次开锁的 ID 和相对时间 |
| 🌈 **RGB 状态灯** | 真 PWM 呼吸 (D5/D9/D10), 状态色彩切换 |
| 🔓 **电磁锁** | 5V 继电器控制 12V 电磁锁, 5 秒后自动上锁 |
| 🗣️ **语音播报** | ISD1820 录音模块播放"欢迎回家" |
| 🤫 **静音模式** | 串口 `silent` 切换, 夜归不扰民 |
| 🎁 **彩蛋** | 累计解锁满 5 次播放"超级玛丽"风格升级音 🍄 |

---

## 🛠️ 硬件清单

| 模块 | 型号 | 数量 |
|:--|:--|:--:|
| MCU | Arduino Nano (ATmega328P) | 1 |
| **TFT 屏幕** | **ILI9341 2.2" SPI 240×320** | 1 |
| 指纹模块 | TM1026M (3.3V UART) | 1 |
| 麦克风触发 | KY-038 | 1 |
| 声音波形 | MAX9812 | 1 |
| 蜂鸣器 | 无源蜂鸣器 | 1 |
| 继电器 | 1 路 5V | 1 |
| 语音模块 | ISD1820 + 8Ω 喇叭 | 1 |
| 状态灯 | 共阴极 RGB LED | 1 |
| 电磁锁 | 12V DC | 1 |
| 电源 | 12V/2A 适配器 + LM2596 降压 | 1 |
| 电阻 | 1KΩ + 2KΩ (分压) + 220Ω×3 (限流) | — |
| 续流二极管 | 1N4007 | 1 |

---

## 🔌 引脚接线一览 (v2.0)

| Nano | 模块 | 备注 |
|:--:|:--|:--|
| D2 | 指纹 TX (绿) | SoftwareSerial RX |
| D3 | 指纹 RX (白) | **必须经 1K+2K 分压** |
| D4 | ISD1820 PLAYE | — |
| **D5** | **RGB-B** | PWM (从 D11 迁移) |
| D6 | 继电器 IN | — |
| D7 | KY-038 DO | — |
| **D8** | **蜂鸣器 S** | 新位置 |
| D9 | RGB-R | PWM + 220Ω |
| D10 | RGB-G | PWM + 220Ω |
| **D11** | **TFT MOSI** | SPI |
| D12 | TFT MISO (可不接) | — |
| **D13** | **TFT SCK** | SPI |
| **A1 / A2 / A3** | TFT CS / DC / RST | 数字输出 |
| A0 | MAX9812 OUT | 模拟 |

> 完整接线图、电源拓扑、EEPROM 数据布局见 [`声纹指纹门锁系统-完整方案.md`](./声纹指纹门锁系统-完整方案.md)

---

## 🚀 快速上手

### 1. 安装依赖库 (Arduino IDE → 库管理器)

- `Adafruit GFX Library`
- `Adafruit ILI9341`
- `Adafruit Fingerprint Sensor Library`

### 2. 选择开发板

```
工具 → 开发板 → Arduino AVR Boards → Arduino Nano
工具 → 处理器 → ATmega328P  (老板子选 "ATmega328P (Old Bootloader)")
```

### 3. 录制 ISD1820 语音 (仅需一次)

> 单独给 ISD1820 接 5V/GND/喇叭 → 按住 REC 说"欢迎回家"→ 松开 → 按 PLAYE 验证。

### 4. 上传 & 录入指纹

```
上传 fingerprint_door_lock/fingerprint_door_lock.ino
串口监视器, 9600
发送 'e' → 按提示放手指 2 次
发送 'd' → 清空全部指纹 (谨慎)
```

### 5. 配置个性化 + 留言

```
> u 1 Yang 0 200 80 1     ← 主人, 绿色, 海豚音旋律
> u 2 Mom  255 100 100 0  ← 妈妈, 粉色, 经典旋律
> m Welcome home!         ← 群发留言
> m@1 Yang, package arrived  ← 只给 ID=1 看
> ml                       ← 列出所有留言
```

所有配置存 EEPROM, 重启不丢。

### 6. 体验

```
拍手 / 说话         →  TFT 进入 WAKE 动画
按已录入手指        →  彩色个性化欢迎 + 留言弹出 + 5s 后自动锁
按错指纹 3 次       →  暴力锁定 30s 红灯爆闪
唤醒后 15s 没指纹   →  门铃模式播 ding-dong
连续 5 次成功       →  触发"超级玛丽"彩蛋 🍄
待机时              →  桌面宠物 🐱 走来走去
```

---

## 🗂️ 项目结构

```
.
├── README.md                          ← 本文件
├── fingerprint_door_lock/
│   └── fingerprint_door_lock.ino      ← 主程序 v2.0
├── 声纹指纹门锁系统-完整方案.md       ← 详细接线 / EEPROM / 排错文档
├── 1.贴片式RGB三原色LED/              ← 模块资料
├── 2.声音波形模块(MAX9812)/
├── 3.麦克风声音检测(KY-038)/
├── 7.2.2寸串口触屏/                   ← ILI9341 资料 (新主屏)
├── 8.OLED屏幕 0.96英寸/               ← v1 用过, v2 已不用
├── 9.无源蜂鸣器/
├── 13.继电器/
├── 16.语音模块+喇叭/
└── 32.指纹识别传感器(TM1026M)/
```

---

## 🎬 状态机示意

```
   ┌────────┐  声音    ┌────────┐  1.5s   ┌──────────┐
   │  IDLE  │ ───────► │  WAKE  │ ──────► │ WAIT_FP  │
   │ +宠物  │          └────────┘         └────┬─────┘
   └───▲────┘                                  │
       │                       匹配成功 ┌──────┴──────┐ 不匹配
       │                                ▼             ▼
       │                          ┌─────────┐   ┌─────────┐
       │                          │ MATCH_OK│   │  FAIL   │
       │                          │ 个性化  │   │ +X动画  │
       │                          │ 留言弹  │   └────┬────┘
       │                          └────┬────┘        │
       │                               ▼      失败3次?
       │                          ┌─────────┐        │
       └──────────────  5s ──── ──┤UNLOCKED │        ▼
                                  └─────────┘  ┌──────────┐
                                                │ LOCKOUT  │ 30s
                                                │ 红闪报警 │
                                                └──────────┘
   超时15s无指纹 → DOORBELL → ding-dong → IDLE
```

---

## 📝 串口命令

| 命令 | 说明 |
|:--|:--|
| `e` / `d` | 录入 / 清空指纹 |
| `u <id> <name> <r> <g> <b> <mel>` | 设置用户 |
| `ul` | 列出用户 |
| `m <text>` / `m@<id> <text>` / `m! <text>` | 群发/定向/置顶留言 |
| `ml` / `md <idx>` / `mc` | 列表/删除/清空留言 |
| `log` | 查看解锁日志 |
| `silent` | 切换静音 |
| `stats` | 查看统计 |
| `help` | 帮助 |

---

## 🐛 常见问题

| 现象 | 解决 |
|:--|:--|
| TFT 全白没图像 | 检查 CS/DC/RST = A1/A2/A3, 5V 稳定 |
| 串口无输出 | 波特率 9600; 装 CH340 驱动 |
| 上传失败 | 处理器切 "Old Bootloader" |
| 指纹模块未找到 | TX/RX 是否反; **D3→RX 是否分压**; 电源 3V3 |
| 不会被声音唤醒 | KY-038 电位器调灵敏度; 阈值 60–200 可调 |
| 继电器逻辑反 | 部分模块低电平触发, `RELAY_PIN` HIGH/LOW 互换 |
| RGB 颜色不对 | 共阳极板把 `RGB_COMMON_ANODE` 设 `true` |
| 编译 Sketch too big | flash 紧, 删 `drawSparkle` 烟花和部分动画 |
| 桌面宠物总是 sad | 8 小时没人解锁触发, 解锁一次就回 happy |

---

## 🆕 v1 → v2 升级要点

| 项 | v1 | v2 |
|:--|:--|:--|
| 屏幕 | SSD1306 0.96″ 单色 OLED | **ILI9341 2.2″ 彩色 TFT** |
| RGB_B 引脚 | D11 | **D5** (移走避开 SPI) |
| 蜂鸣器 | D5 | **D8** |
| OLED I²C | A4/A5 | **不再使用** |
| TFT SPI | — | **D11/D12/D13 + A1/A2/A3** |
| 桌面宠物 | ❌ | ✅ 4 情绪 |
| 个性化 | ❌ | ✅ 8 用户 |
| 留言板 | ❌ | ✅ EEPROM 持久 |
| 暴力锁定 | ❌ | ✅ 3 次 30s |
| 解锁日志 | ❌ | ✅ 最近 10 次 |
| 录入超时 | 死循环 bug | ✅ 8s 超时 |
| Tone/PWM 冲突 | 蓝灯抖动 bug | ✅ 已修复 |

---

## 📜 License

MIT © Yangbadger222

---

## 🙌 致谢

- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341)
- [Adafruit Fingerprint Sensor](https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library)
