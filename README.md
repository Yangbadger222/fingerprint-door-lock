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
| 🔊 **口令匹配** | MAX9812 录制口令波形包络, 解锁时先验证口令再验指纹 |
| 🤫 **静音模式** | 串口 `silent` 切换, 夜归不扰民 |

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

### 未在引脚表中展开的模块接线

| 模块 | VCC | GND | 信号引脚 | 注意事项 |
|:--|:--|:--|:--|:--|
| KY-038 麦克风 | 5V | GND | DO → D7 | 蓝色电位器调灵敏度 |
| MAX9812 波形 | **3.3V** | GND | OUT → A0 | 不是 5V! |
| ISD1820 语音 | 5V | GND | PLAYE → D4 | 先录音再接 Arduino |
| 继电器 | 5V | GND | IN → D6 | COM 接 12V+, NO 接电磁锁+ |
| TM1026M 指纹 | **3.3V** | GND | TX → D2, RX → D3(**分压**) | D3→1KΩ→节点→2KΩ→GND, 节点→RX |

> 完整接线图、电源拓扑、EEPROM 数据布局见 [`声纹指纹门锁系统-完整方案.md`](./声纹指纹门锁系统-完整方案.md)

---

## 🚀 使用指南

### A. 首次部署 (装机一次)

#### A1. 安装依赖库

**方式 1 — Arduino IDE 库管理器**:
- `Adafruit GFX Library`
- `Adafruit ILI9341` ⚠️ **建议锁定 1.5.6 版本**, 1.6.x 会让 flash 超 30KB
- `Adafruit Fingerprint Sensor Library`

**方式 2 — arduino-cli (推荐, 可控)**:
```bash
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit ILI9341"@1.5.6
arduino-cli lib install "Adafruit Fingerprint Sensor Library"
```

#### A2. 接线

按 [`声纹指纹门锁系统-完整方案.md`](./声纹指纹门锁系统-完整方案.md) 的引脚表逐项接好。**最容易踩坑**:
- TM1026M 的 `RX (白)` 必须经 1KΩ + 2KΩ 分压再连 D3 (5V→3.3V)
- TFT 的 `CS=A1, DC=A2, RST=A3` 是模拟脚当数字用, 不是 D 引脚
- RGB_B 是 **D5** (不是 D11), 蜂鸣器是 **D8** (不是 D5)
- 所有模块共地

#### A3. 录制 ISD1820 欢迎语音 (仅一次)

单独给 ISD1820 接 5V/GND/喇叭 (**先别连 Arduino**):
1. 按住板载 **REC** 按键
2. 对着模块上的麦克风说 **"欢迎回家"** (≤10 秒)
3. 松开 REC
4. 按 **PLAYE** 验证有声
5. 验证完接回 D4

#### A4. 编译上传

```
开发板:  Arduino Nano
处理器:  ATmega328P  (CH340 老板子选 Old Bootloader)
端口:    /dev/cu.usbserial-XXXX (Mac) 或 COMx (Win)
```

**编译验证基线** (实测):
```
Sketch uses 30694 bytes (99%) of program storage space. Maximum is 30720 bytes.
Global variables use 940 bytes (45%) of dynamic memory.
```

> ⚠️ flash 已用 99%, 改任何代码都可能超限。先把库版本固定再改。

#### A5. 录入指纹

打开串口监视器, **波特率 9600**, 行尾 **NL**:

```
> e
Enroll ID=1
[把手指放上指纹模块]
Remove finger...
[抬起手指]
Place same finger
[再放同一手指]
Enroll OK ID=1
```

继续发送 `e` 录入下一个 (最多 8 个用户位, 指纹模块最多存 127 枚)。

#### A6. 配置用户个性化

格式: `u <id> <name> <R> <G> <B> <melodyId>`

```
> u 1 Yang 0   200 80  1     ← ID 1: Yang, 绿色, 海豚音
> u 2 Mom  255 100 100 0     ← ID 2: Mom,  粉色, 经典旋律
> u 3 Dad  50  100 255 3     ← ID 3: Dad,  蓝色, 反向低沉
> ul                          ← 列出所有用户
```

参数:
- `name`: 1-8 个 ASCII 字符
- `R G B`: 0-255 各一项
- `melodyId`: 0=经典上行 / 1=海豚音 / 2=三连音 / 3=反向低沉

**配置全部存在 EEPROM, 断电不丢**, 录完一次永久生效。

---

### B. 日常使用 (装好之后)

#### B1. 基本流程

```
1. 拍手 / 说话 / 敲门
       ↓
2. TFT 进入 Wake 动画 (麦克风扩散圈) + 蓝灯
       ↓
3. 把手指按到指纹模块 (15 秒内)
       ↓
4. ✅ 匹配成功:
       - TFT 大字 HI! + 用户名 + 头像
       - 该用户专属未读留言弹出
       - RGB 渐暗绿光 5 秒
       - ISD1820 播放"欢迎回家"
       - 继电器吸合, 电磁锁打开
       - 5 秒后自动上锁
   ❌ 失败:
       - X 标记 + RGB 红
       - 3 次失败触发锁定 (见 B3)
   ⏰ 15 秒没放指纹:
       - 进入门铃模式, 播 ding-dong
```

#### B2. 屏幕状态识别

| 屏幕显示 | 含义 | RGB 颜色 |
|:--|:--|:--|
| 大锁图标 + "LOCKED" + 小猫 🐱 | 待机, 等声音唤醒 | 红色呼吸 |
| 大锁图标 + 紫色 MSG! 角标 | 待机, **有未读留言** | **紫色慢闪** |
| 麦克风扩散圈 + "Wake" | 检测到声音 | 蓝色 |
| 旋转指纹纹路 + "Finger?" | 等待指纹 | 蓝色脉冲 |
| HI! + 用户名 + 彩色头像 | 匹配成功 | 绿色渐暗 |
| 大 X + "Retries: N" | 指纹错误 | 红色 |
| "BLOCKED" + 倒计时 | 暴力锁定中 | **红灯爆闪** |
| "Bell" + Visitor | 门铃模式 | 黄色 |

#### B3. 桌面宠物状态 🐱

小猫在屏幕右侧来回走动, 4 种情绪:

| 表情 | 触发条件 | 动作 |
|:--:|:--|:--|
| 😊 happy (黄色) | 默认 | 来回走, 摇尾巴 |
| ✨ excited (粉色) | 刚成功解锁 | 星眼 + 心心泡泡 |
| 😢 sad (紫色) | 解锁失败 / 8h 无人理 | 耳朵下垂, 流泪 |
| 😴 sleepy (灰色) | 24h 无人理 | 静止, Z 字符号 |

每次成功开锁 +10 经验 (存 EEPROM)。

#### B4. 暴力锁定

连续 **3 次** 指纹错误 → **30 秒锁定**:
- TFT 显示大字 "BLOCKED" + 倒计时
- RGB 红灯 5Hz 爆闪
- 期间忽略所有声音和指纹输入
- 30 秒后自动恢复

---

### C. 留言板 📝

#### C1. 添加留言

```
> m Welcome home everyone!          ← 群发, 所有人解锁都看
> m@1 Yang, your package arrived    ← 只给 ID=1 看
> m@2 妈, 钥匙在抽屉里 (ASCII)       ← 定向给 ID=2
> m! Today is trash day             ← 置顶留言 (! 在 m 后)
```

留言**最大 80 字符 ASCII**, 最多存 6 条, 满了会拒绝并提示 `Msg full`。

#### C2. 留言显示位置

- **待机屏底部** 横向滚动显示第一条 active 留言
- **解锁后** 弹出该用户的专属留言 (定向 / 群发都算), 看到后自动标 read

#### C3. 留言管理

```
> ml          ← 列出所有留言 (含已读状态)
> md 0        ← 删除第 0 条
> mc          ← 清空所有留言
```

---

### D. 维护 / 高级

#### D1. 完整命令参考

| 命令 | 说明 |
|:--|:--|
| `e` | 录入指纹 (按提示放手指 2 次) |
| `d` | 清空所有指纹 ⚠️ |
| `u <id> <name> <r> <g> <b> <mel>` | 设置用户 |
| `ul` | 列出用户 |
| `m <text>` / `m@<id> <text>` / `m! <text>` | 群发/定向/置顶留言 |
| `ml` / `md <idx>` / `mc` | 列表/删除/清空留言 |
| `log` | 查看最近 10 次解锁记录 |
| `silent` | 切换静音模式 (蜂鸣器和 ISD1820 都不响) |
| `stats` | 查看统计 (开锁次数 / 运行小时数 / 宠物经验 / 当前情绪) |
| `help` | 列出帮助 |

#### D2. 部署后断开 USB

代码全部存 flash + EEPROM, 断电再上电后:
- 用户数据保留 ✅
- 留言保留 ✅
- 解锁次数保留 ✅
- 宠物经验保留 ✅
- 静音模式保留 ✅
- 待机/解锁时间从 0 重新计 (无 RTC)

所以**调试调好后可以彻底断开 USB**, 直接外接 12V 电源跑。

#### D3. 修改个性化的工作流

```
1. 临时插上 USB 数据线
2. 打开串口监视器 9600
3. 发命令 (u / m / md 等)
4. 验证 (ul / ml)
5. 拔掉 USB, 系统继续运行
```

#### D4. 重置全部数据 (恢复出厂)

最简单: 直接擦除 EEPROM
```cpp
// 临时加进 setup() 然后烧一次, 之后再去掉
for (int i = 0; i < 1024; i++) EEPROM.update(i, 0xFF);
```
重启后系统会自动 re-init EEPROM 并填默认值。

---

## 🧪 模块逐项测试

组装完整系统之前, 建议逐个模块独立测试。每个测试是独立 sketch, 单独编译上传。

| 测试目录 | 测试内容 | 引脚 |
|:--|:--|:--|
| `test_tft/` | TFT 全部 9 个界面循环展示 (低闪烁版) | A1/A2/A3/D11/D13 |
| `test_buzzer_led/` | 蜂鸣器全部旋律 + RGB LED 逐色 + 呼吸灯 | D5/D8/D9/D10 |
| `test_mic/` | KY-038 数字触发 + MAX9812 波形柱状图 | D7/A0 |
| `test_isd1820/` | ISD1820 自动播放 (每 5s) + 手动触发 | D4 |
| `test_relay/` | 继电器自动通断 (每 2s) + 手动控制 | D6 |
| `test_fingerprint/` | TM1026M 检测/录入/匹配/清空/计数 | D2/D3 (分压) |
| `test_passphrase/` | 口令波形录制 + 匹配分数测试 | A0/D7 |

**推荐测试顺序**: TFT → 蜂鸣器LED → 麦克风 → 语音 → 继电器 → 指纹 → 口令 → 主程序

#### 各测试接线速查

**test_tft** — TFT 屏幕:
| TFT | Nano | 备注 |
|:--|:--|:--|
| VCC | 5V | |
| GND | GND | |
| CS | A1 | |
| RESET | A3 | |
| DC/RS | A2 | |
| MOSI | D11 | |
| SCK | D13 | |
| LED | 5V | 背光 |

**test_buzzer_led** — 蜂鸣器 + RGB:
| 模块 | Nano | 备注 |
|:--|:--|:--|
| 蜂鸣器 S(+) | D8 | |
| RGB-R | D9 | 串 220Ω |
| RGB-G | D10 | 串 220Ω |
| RGB-B | D5 | 串 220Ω, **不是 D11** |
| RGB-GND | GND | 共阴极 |

**test_mic** — 麦克风:
| 模块 | Nano | 备注 |
|:--|:--|:--|
| KY-038 VCC | 5V | |
| KY-038 DO | D7 | 有声音时 LOW |
| MAX9812 VCC | **3.3V** | 不是 5V! |
| MAX9812 OUT | A0 | 模拟波形 |

**test_isd1820** — 语音模块:
| ISD1820 | Nano | 备注 |
|:--|:--|:--|
| VCC | 5V | |
| GND | GND | |
| PLAYE | D4 | 边沿触发 |
| SP+/- | 喇叭 | 8Ω 0.5W |

> 先录音再测试: 按住 REC → 说"欢迎回家" → 松开 → 验证 → 再接 D4

**test_relay** — 继电器:
| 继电器 | Nano | 备注 |
|:--|:--|:--|
| VCC | 5V | |
| GND | GND | |
| IN | D6 | 高电平触发 |
| COM/NO | 12V+电磁锁 | 可不接, 听咔嗒声即可 |

**test_fingerprint** — 指纹传感器 (**最复杂**):
| TM1026M | Nano | 备注 |
|:--|:--|:--|
| VCC (红) | **3.3V** | 接 5V 会烧! |
| TX (绿) | D2 | SoftwareSerial RX |
| RX (白) | D3 经分压 | D3→1KΩ→节点→2KΩ→GND, 节点→RX |

**test_passphrase** — 口令匹配:
| 模块 | Nano | 备注 |
|:--|:--|:--|
| MAX9812 VCC | **3.3V** | |
| MAX9812 OUT | A0 | 模拟采样 |
| KY-038 DO | D7 | 可选, 快速触发 |

> 完整接线细节见 [`声纹指纹门锁系统-完整方案.md`](./声纹指纹门锁系统-完整方案.md) §13

```bash
# 编译 + 上传示例
arduino-cli compile --fqbn arduino:avr:nano test_tft
arduino-cli upload -p /dev/cu.usbserial-110 --fqbn arduino:avr:nano test_tft
```

---

## 🗂️ 项目结构

```
.
├── README.md                          ← 本文件
├── fingerprint_door_lock/
│   └── fingerprint_door_lock.ino      ← 主程序 v2.0
├── test_tft/                          ← 模块测试: TFT 屏幕
├── test_buzzer_led/                   ← 模块测试: 蜂鸣器 + RGB LED
├── test_mic/                          ← 模块测试: KY-038 + MAX9812
├── test_isd1820/                      ← 模块测试: ISD1820 语音
├── test_relay/                        ← 模块测试: 继电器
├── test_fingerprint/                  ← 模块测试: TM1026M 指纹
├── test_passphrase/                   ← 模块测试: 口令波形匹配
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
| `m <text>` / `m@<id> <text>` / `m! <text>` | 群发/定向/置顶留言 |
| `silent` | 切换静音 |
| `vc` / `vd` / `vs` | 录制/删除/查看口令 |
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
