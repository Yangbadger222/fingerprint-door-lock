# 🔐 声纹指纹门锁系统 (Fingerprint Door Lock)

> 基于 **Arduino Nano (ATmega328P)** 的多模态智能门锁: 声音唤醒 → 指纹验证 → 电磁锁开关 + OLED 动画 + 蜂鸣器旋律 + RGB 状态灯 + 语音播报。

[![Arduino](https://img.shields.io/badge/Arduino-Nano%20ATmega328P-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](#license)
[![Status](https://img.shields.io/badge/Status-Working-brightgreen.svg)](#)

---

## ✨ 功能亮点

| 模块 | 行为 |
|:--|:--|
| 🎤 **声音唤醒** | KY-038 数字触发 + MAX9812 模拟峰值检测, 拍手 / 说话即可唤醒 |
| 👆 **指纹识别** | TM1026M (兼容 AS608), 最多 127 枚, 串口 `e` 录入 / `d` 清空 |
| 🖥️ **OLED 动画** | 扫描线开机、呼吸圆环待机、声波扩散、旋转指纹、放射烟花、抖动 X |
| 🔊 **蜂鸣器旋律** | 开机和弦、叮咚唤醒、上行成功、下行警告、上锁三音, 含 **超级玛丽彩蛋** |
| 🌈 **RGB 状态灯** | 真 PWM 呼吸 (D9/D10/D11), 红待机 / 蓝等待 / 绿渐暗解锁 / 红失败 |
| 🔓 **电磁锁** | 5V 继电器控制 12V 电磁锁, 5 秒后自动上锁 |
| 🗣️ **语音播报** | ISD1820 录音模块播放"欢迎回家" |
| 🎁 **彩蛋** | 累计解锁满 5 次播放"超级玛丽"风格升级音 🍄 |

---

## 🛠️ 硬件清单

| 模块 | 型号 | 数量 |
|:--|:--|:--:|
| MCU | Arduino Nano (ATmega328P) | 1 |
| OLED 显示 | SSD1306 0.96" I²C | 1 |
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

## 🔌 引脚接线一览

| Nano | 模块 | 备注 |
|:--:|:--|:--|
| D2 | 指纹 TX (绿) | SoftwareSerial RX |
| D3 | 指纹 RX (白) | **必须经 1K+2K 分压** |
| D4 | ISD1820 PLAYE | — |
| D5 | 蜂鸣器 S | PWM |
| D6 | 继电器 IN | — |
| D7 | KY-038 DO | — |
| D9 | RGB-R | PWM + 220Ω |
| D10 | RGB-G | PWM + 220Ω |
| D11 | RGB-B | PWM + 220Ω |
| A0 | MAX9812 OUT | 模拟 |
| A4 / A5 | OLED SDA / SCL | I²C |

> 完整接线图、续流保护、电源拓扑见 [`声纹指纹门锁系统-完整方案.md`](./声纹指纹门锁系统-完整方案.md)

---

## 🚀 快速上手

### 1. 安装依赖库 (Arduino IDE → 库管理器)

- `Adafruit SSD1306`
- `Adafruit GFX Library`
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
上传 fingerprint_door_lock.ino
打开串口监视器, 波特率 9600
发送 'e' → 按提示放手指 2 次
发送 'd' → 清空所有指纹 (谨慎!)
```

### 5. 体验

```
拍手 / 说话  →  按指纹  →  开锁 + 播报 + 烟花动画
              连续 5 次 →  触发"超级玛丽"彩蛋 🎵
```

---

## 🗂️ 项目结构

```
.
├── README.md                       ← 本文件
├── fingerprint_door_lock.ino       ← 主程序
├── 声纹指纹门锁系统-完整方案.md     ← 详细接线 / 操作 / 排错文档
├── 1.贴片式RGB三原色LED/           ← 模块资料
├── 2.声音波形模块(MAX9812)/
├── 3.麦克风声音检测(KY-038)/
├── 8.OLED屏幕 0.96英寸/
├── 9.无源蜂鸣器/
├── 13.继电器/
├── 16.语音模块+喇叭/
└── 32.指纹识别传感器(TM1026M)/
```

---

## 🎬 状态机示意

```
        ┌────────┐  声音/拍手   ┌────────┐
        │  IDLE  │ ───────────► │  WAKE  │
        │ (锁屏) │              │ (动画) │
        └────▲───┘              └───┬────┘
             │                      │ 1.5s
             │ timeout              ▼
             │                ┌──────────┐
             │   不匹配       │  WAIT    │
             │ ◄────────────  │  FINGER  │
             │                └────┬─────┘
        ┌────┴───┐                 │ 匹配成功
        │  FAIL  │                 ▼
        │ (X+音) │           ┌───────────┐
        └────────┘           │ MATCH_OK  │
                             │ +彩蛋(5x) │
                             └─────┬─────┘
                                   ▼
                             ┌───────────┐  5s
                             │ UNLOCKED  │ ───► IDLE
                             └───────────┘
```

---

## 🐛 常见问题

| 现象 | 解决 |
|:--|:--|
| 串口无输出 | 波特率改 9600; 装 CH340 驱动 |
| 上传失败 | 处理器切 "Old Bootloader" |
| 指纹模块未找到 | TX/RX 是否反; **D3→RX 是否分压**; 电源是 3V3 |
| OLED 黑屏 | I²C 地址换 0x3D; 检查 SDA/SCL |
| 不会被声音唤醒 | KY-038 电位器调灵敏度; 阈值 60–200 可调 |
| 继电器逻辑反 | 部分模块低电平触发, `RELAY_PIN` HIGH/LOW 互换 |
| RGB 颜色不对 | 共阳极板把 `RGB_COMMON_ANODE` 设 `true` |

---

## 📜 License

MIT © Yangbadger222

---

## 🙌 致谢

- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit Fingerprint Sensor](https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library)
