# Flasher 烧录与调试指南（Dongle / CH570·CH572）

本文说明如何用桌面上的 **Flasher**（`..\Flasher`）给本项目的 Dongle 烧录固件，以及如何在烧录后做运行和通信调试。

- Flasher 主机工具：`..\Flasher\firmware\tools\flasher.py`
- Dongle 固件镜像：`firmware\build\rf_uart_app.bin`
- 目标芯片：CH570Q / CH572（WCH USB ISP Bootloader）

---

## 1. 硬件与连接

### 1.1 两个 USB 口

| 接口 | 位置 | 作用 |
| --- | --- | --- |
| USB-C | Flasher 板 | 连 PC，枚举为 CDC ACM 串口 `1A86:5721` |
| USB-A 母座 | Flasher 板 | 插入 Dongle，提供受控的 VBUS 和 D+/D- |

```
PC ──USB-C──> [ Flasher / CH582F ] ──USB-A──> [ Dongle / CH570Q·CH572 ]
                 CDC 1A86:5721                 5V、D+、D- 全部由 Flasher 控制
```

Dongle 是 USB-A 公头，直接插进 Flasher 的 USB-A 口即可，不需要额外接线。

> 注意：烧录和调试期间 **不要** 再给 Dongle 接其他 USB 口或外部 5V。Flasher 通过 MT9700 负载开关给目标供电，外部供电会破坏上电角色采样，也可能倒灌。

### 1.2 USB-A 口的信号复用

Flasher 的 USB-A 数据脚在三种用途之间切换：

| USB-A 引脚 | Flasher CH582F | Dongle CH570/572 | 烧录（ISP） | 主机桥接 | 从机 UART 桥接 |
| --- | --- | --- | --- | --- | --- |
| VBUS | VBUS_A（MT9700，PB4 使能） | +5V | 供电 | 供电 | 供电 |
| D- | PB12 | PA0 | USB D- | USB D- | UART1 RXD ← Dongle PA0 TXD |
| D+ | PB13 | PA1 | USB D+ | USB D+ | UART1 TXD → Dongle PA1 RXD |
| GND | GND | GND | 共地 | 共地 | 共地 |

也就是说，Dongle 的 PA0/PA1 在 USB 模式下是 D-/D+，在从机模式下是 UART TX/RX；Flasher 在 PB12/PB13 上做同样的复用，所以一根 USB-A 口既能烧录/桥接 USB，也能桥接串口。

### 1.3 控制信号

| 信号 | 引脚 | 说明 |
| --- | --- | --- |
| 目标 VBUS 使能 | PB4 | 高电平打开 MT9700，给 USB-A 供电 |
| D+ 强制上拉 | PB13 | 仅 ISP 上电时驱动为高，让 Bootloader 进入下载模式 |
| 状态 LED | PA12 | 高电平点亮 |
| BOOT 按键 | PB22 | 当前固件只做上拉、不参与功能；烧录不需要按它 |

---

## 2. 快速开始

前置条件：

1. 已安装 Python 3.8+ 和 pyserial：
   ```powershell
   pip install pyserial
   ```
2. 已安装 MounRiver Studio 3（默认路径 `C:\MounRiver\MounRiver_Studio3\...`），或修改 `firmware\Makefile` 里的 `MRS_TOOLCHAIN`。若命令行没有 `make`，可使用 MounRiver Studio 自带的 `make`，或安装 `mingw32-make`。
3. Flasher 的 USB-C 已连 PC，Windows 设备管理器中出现 VID/PID 为 `1A86:5721` 的串口。

最短流程（在 Dongle 项目根目录执行）：

```powershell
cd firmware
make
python ..\..\Flasher\firmware\tools\flasher.py flash build\rf_uart_app.bin --verify
```

`flasher.py` 会按 VID/PID 自动找 Flasher 的 COM 口；找不到或多个时显式指定：

```powershell
python ..\..\Flasher\firmware\tools\flasher.py --port COM7 flash build\rf_uart_app.bin --verify
```

---

## 3. 构建 Dongle 固件

```powershell
cd C:\Users\14491\Desktop\Dongle\firmware
make
```

产物：

| 文件 | 用途 |
| --- | --- |
| `build\rf_uart_app.hex` | Intel HEX，可用 WCH 工具烧录 |
| `build\rf_uart_app.bin` | 裸二进制，**Flasher 脚本使用这个** |
| `build\rf_uart_app.elf` | 调试符号 |
| `build\rf_uart_app.map` | 链接映射 |

链接脚本把应用放在 `0x00000000`，Flash 窗口 240 KiB（`0x0003C000`），RAM 12 KiB。Flasher 的 ISP 写入地址也是从 0 开始，二者一致。

清理：

```powershell
make clean
```

> Flasher 自身固件（CH582F）的构建在 `..\Flasher\firmware`，和本项目的 Dongle 固件是两套代码，不要混用镜像。

---

## 4. 烧录

### 4.1 命令

```powershell
# 烧录并校验
python ..\..\Flasher\firmware\tools\flasher.py flash build\rf_uart_app.bin --verify

# 只烧录，不校验（更快）
python ..\..\Flasher\firmware\tools\flasher.py flash build\rf_uart_app.bin

# 打印原始 ISP 帧，排查 Bootloader 问题
python ..\..\Flasher\firmware\tools\flasher.py flash build\rf_uart_app.bin --verify --debug
```

成功时输出类似：

```text
target chip=0x72 type=0x.. uid=................ erase=... KiB
write 10816/10816
verify 194/194
done
```

### 4.2 烧录过程（不需要按键）

`flash` 不是把镜像直接丢给正在运行的应用，而是由 Flasher 完整地“上电进 Bootloader → 擦除 → 写入 → 校验 → 重启”：

1. `BREAK` 让 Flasher 从桥接模式回到命令模式；
2. `ISP_OPEN`：Flasher 先断电，拉高 D+，再上 VBUS，Dongle 的 ROM Bootloader 采样到 D+ 高，进入 USB ISP；
3. 枚举 Bootloader（`4348:55E0`，部分批次为 `1A86:55E0`），读取芯片 ID、UID、Config；
4. 必要时解锁 Code Flash（见 4.3）；
5. `ISP_BEGIN` 再次上电进 ISP，按 1 KiB 扇区擦除（最少 8 个扇区，最多 240 个）；
6. 以 56 字节为单位写入并可选校验；
7. `ISP_FINISH` 结束会话，Flasher 把目标重新上电到应用模式（默认是从机模式，见 5.1）。

整个过程 Flasher 自动控制 VBUS 和 D+，**不需要按 BOOT 键，也不需要手动插拔 Dongle**。

### 4.3 CH570/CH572 的 Code Flash 解锁

CH570/CH572 在 Config 的第一个字里保存 Code Flash 保护标记：

- 标记为 `0xA5 0x5A`：允许 Bootloader 接收 PROGRAM；
- 标记为擦除态 `0xFF`：Bootloader 会对每条 PROGRAM 返回 `... FE`，表现为“能识别、能擦除、但写不进去”。

`flasher.py` 的 `prepare_target()` 会自动处理：

1. 打开 ISP 会话并 IDENTIFY；
2. 如果 `config[0] != 0xA5`，发送 ISP KEY，写入 `config[0]=0xA5, config[1]=0x5A`，重启；
3. 重新进 ISP 确认已解锁，再开始擦写。

这个解锁是**一次性且持久**的，之后再次烧录不会重复写 Config。若需要确认当前状态，用 `--debug` 观察：

```text
CFG ff...: enabling code flash access
CFG now a55a...
```

> 若烧录日志出现 `status 5`（ISP error）或 PROGRAM 返回 `FE`，优先检查这一步是否成功。CH58x 的 `CFG_DEBUG_EN` 分支不适用于 CH570/CH572，脚本对这两颗芯片只走上面的解锁流程。

### 4.4 镜像限制

- 最大 240 KiB（脚本和 Flasher 固件都会检查）；
- 脚本会自动把镜像补齐到 1 KiB 整数倍，不足部分填 `0xFF`；
- 写入地址固定从 `0x00000000` 开始，不要手工拼接偏移；
- 使用 `.bin`，不要用 `.hex`（脚本按裸二进制处理）。

---

## 5. 运行模式与调试

### 5.1 上电角色判定

Dongle 固件在 `firmware\src\main.cpp` 里上电时采样 PA0/PA1：

```text
D+、D- 都为低  -> host = true  -> USB 设备 + 射频主机（master）
否则           -> host = false -> UART + 射频从机（slave）
```

Flasher 通过上电瞬间的 D+/D- 状态控制这个采样结果：

| `flasher.py` 操作 | VBUS | 上电时 D+/D- | Dongle 启动为 |
| --- | --- | --- | --- |
| `flash` / `power isp` | 开 | D+ 强制高 | ROM ISP Bootloader |
| `master` | 开 | Flasher 主机 15k 下拉，两个都低 | USB CDC 主机（master） |
| `slave` / `power app` | 开 | 悬空，Dongle 内部上拉，两个都高 | UART 从机（slave） |
| `power off` | 关 | — | 断电 |

重要结论：

- **烧录完成后默认是从机模式**（`ISP_FINISH` 后 Flasher 用悬空数据线重新上电），不会自动枚举成 USB 设备；
- 想让它作为 USB 主机运行，执行 `master`，或把 Dongle 直接插到 PC 的 USB-A 口（PC 主机的下拉会让它采样为 master）；
- 想让它作为从机运行，执行 `slave` 或 `power app`。

### 5.2 master：把 Dongle 的 USB CDC 桥到 PC

```powershell
python ..\..\Flasher\firmware\tools\flasher.py master --monitor
```

行为：

1. Flasher 断电，切到 USB 主机模式（内部下拉），再上 VBUS；
2. Dongle 采样为 master，初始化 USB CDC（`1A86:5720`）和射频主机；
3. Flasher 枚举 Dongle，把 PC 的 CDC 数据与 Dongle 的 CDC 数据双向透传；
4. PC 端的波特率/校验/停止位设置会通过 CDC `SET_LINE_CODING` 同步给 Dongle，Dongle 再以 Reset 帧把它发给射频从机。

`--monitor` 会把串口数据直接打到 stdout，并把 stdin 的字节发下去；退出时自动发 `BREAK` 释放桥接。不退出时按 `Ctrl+C`。

不加 `--monitor` 时，端口会留在原始桥接模式，后续用任意串口终端打开 Flasher 的 COM 口即可。关闭该终端后，若没有自动发 BREAK，执行一次 `reset` 让 Flasher 回到命令模式（执行 `reset` 前必须先关闭占用 COM 口的终端）：

```powershell
python ..\..\Flasher\firmware\tools\flasher.py reset
```

### 5.3 slave：把 Dongle 的 UART 桥到 PC

```powershell
python ..\..\Flasher\firmware\tools\flasher.py slave --baud 115200 --monitor
```

行为：

1. Flasher 断电，数据线保持悬空，再上 VBUS，Dongle 采样为 slave；
2. Dongle 初始化 PA0/PA1 为 UART（默认 115200 8N1）；
3. Flasher 在同一对引脚上初始化 UART1，与 PC 的 CDC 双向透传；
4. PC 端改波特率时，Flasher 会把同样的 LineCoding 应用到 UART1。

注意：

- `--baud` 只是 Flasher 侧 UART1 的初始波特率；串口终端打开端口时发送的 LineCoding 会被转发并覆盖它。因此终端和 `--baud` 都要与 Dongle 从机 UART 实际使用的波特率一致。Dongle 的 UART 波特率由射频主机通过 Reset 帧下发，默认 `115200`；改 Flasher 的波特率不会改变射频链路上的协商结果。
- 从机固件上电后 UART 处于 **suspended** 状态，必须等射频主机发来带 LineCoding 的 Reset 才会 resume。因此单独用 `slave --monitor` 只能观察/驱动“已经由主机激活”的从机串口；没有射频主机时，从机不会转发 UART 数据。这是固件设计行为，不是 Flasher 故障。
- `slave` 模式不做目标枚举检查，即使没插 Dongle 也会成功返回；确认目标是否真的在跑，请看 `info` 或实际数据。

### 5.4 power：只控制目标电源

```powershell
python ..\..\Flasher\firmware\tools\flasher.py power off   # 断电
python ..\..\Flasher\firmware\tools\flasher.py power app   # 上电，应用模式（slave）
python ..\..\Flasher\firmware\tools\flasher.py power isp   # 上电，ISP Bootloader
```

`app` 和 `isp` 都会先断电再上电，可用于：

- 观察上电/复位行为；
- 让 Dongle 重新采样角色；
- 在 `info` 确认 Bootloader 是否能被枚举；
- 烧录失败后把目标恢复到已知状态。

### 5.5 info 与状态 LED

```powershell
python ..\..\Flasher\firmware\tools\flasher.py info
```

输出：

```text
protocol 1.1 mode=0 target=0 vid=0000 pid=0000
```

| 字段 | 含义 |
| --- | --- |
| `protocol` | Flasher 命令协议版本（当前 1.1） |
| `mode`（协议字段） | 0=idle，1=master 桥接，2=slave 桥接 |
| `target`（协议字段） | 0=无，1=ISP，2=USB CDC，3=USB Bulk，4=UART |
| `vid/pid` | 当前枚举到的目标（ISP 为 `4348:55e0` 或 `1a86:55e0`，Dongle 应用为 `1a86:5720`）；idle 时为 0 |

注意：`info` 等所有一次性命令在执行前都会先发 `BREAK`，因此它会把已经建立的 master/slave 桥接断掉。需要持续监控时，用同一条命令的 `--monitor`，不要另开一次 `info`。

Flasher 板载 LED（PA12）：

| 现象 | 含义 |
| --- | --- |
| 上电瞬间短暂亮 | 程序已进入 `main()` |
| 常亮 | USB CDC 已枚举（PC 已识别 Flasher） |
| 约 1 Hz 闪烁 | 未枚举：USB-C 没连好、线材无数据、或 PC 未加载 CDC 驱动 |

Dongle 板上的 LED 连接在 PA7，当前固件没有驱动它，不能作为运行指示。

### 5.6 BREAK：从桥接模式回到命令模式

桥接模式是一旦进入就持续透传的。以下操作都会发送串口 BREAK 让 Flasher 退出桥接：

- `reset` 子命令；
- 任意一次新的 `flasher.py` 调用（脚本会先 `break_to_command()` 并 PING 重试）；
- `--monitor` 退出时。

Windows 上 USB BREAK 可能晚于后续的 bulk 写到达，脚本因此最多重试 4 次。如果仍报 `Flasher did not return to command mode`，见第 7 节。

---

## 6. 深入调试

### 6.1 看 Bootloader 原始交互

```powershell
python ..\..\Flasher\firmware\tools\flasher.py flash build\rf_uart_app.bin --verify --debug
```

`--debug` 会把每条 ISP 请求/响应按十六进制打印，例如：

```text
  -> a1120000004d4355204953502026205743482e434e
  <- a100...
```

可以据此判断是 IDENTIFY 失败、READ_CONFIG 失败、KEY 校验失败，还是 ERASE/PROGRAM 被拒绝。常见 ISP 命令：

| 命令 | 名称 | 作用 |
| --- | --- | --- |
| `0xA1` | IDENTIFY | 读芯片 ID / 设备类型 |
| `0xA2` | ISP_END | 结束并重启 |
| `0xA3` | ISP_KEY | 用 UID 派生 XOR 密钥 |
| `0xA4` | ERASE | 按 1 KiB 扇区擦除 |
| `0xA5` | PROGRAM | 写数据（每包最多 56 字节） |
| `0xA6` | VERIFY | 回读校验 |
| `0xA7` | READ_CONFIG | 读 UID 和 Config |
| `0xA8` | WRITE_CONFIG | 写 Config（解锁 Code Flash） |

### 6.2 自己写主机脚本时的协议

Flasher 的 CDC 命令帧：

```text
请求： AA 55 | CMD | LEN | PAYLOAD[LEN] | CRC8(CMD,LEN,PAYLOAD)
响应： 55 AA | CMD | LEN | STATUS | DATA[LEN-1] | CRC8(CMD,LEN,STATUS,DATA)
CRC8： poly=0x07，init=0x00，覆盖 CMD、LEN、数据
```

| CMD | 名称 | 请求负载 | 响应 |
| --- | --- | --- | --- |
| `0x01` | PING | 空 | 与 INFO 相同 |
| `0x02` | INFO | 空 | 状态 + 协议/模式/目标/VID/PID/计数 |
| `0x03` | SET_MODE | `mode:u8 + baud:u32` | 模式与目标信息 |
| `0x04` | POWER | `on:u8 + boot:u8` | 状态 |
| `0x10` | ISP_BEGIN | `size:u32` | 芯片 ID/类型/UID/擦除扇区数 |
| `0x11` | ISP_WRITE | `addr:u32 + data(≤56)` | 状态 + 地址 |
| `0x12` | ISP_VERIFY | `addr:u32 + data(≤56)` | 状态 + 地址 |
| `0x13` | ISP_FINISH | 空 | 状态 |
| `0x14` | ISP_OPEN | 空 | 目标 VID/PID |
| `0x15` | ISP_RAW | 原始 ISP 帧（≤63） | 状态 + 原始响应 |
| `0x16` | ISP_CLOSE | 空 | 状态 |

状态码：`0` OK、`1` 坏命令、`2` 坏参数、`3` 无目标、`4` USB 错误、`5` ISP 错误、`6` 忙、`7` 不支持。

### 6.3 SWD 与日志的边界

Flasher 只提供 **USB ISP 烧录** 和 **USB/UART 桥接**，没有引出 SWD。Dongle 的 PA0/PA1 同时是 SWD 调试脚、USB 数据脚和从机 UART 脚，而且 `main.cpp` 在启动时通过 `sysreg::kPinDebugEnable` 主动释放了调试功能，所以：

- 不能通过 Flasher 做单步/断点调试；
- 烧录后 SWD 调试口已被固件让出，WCH-Link 通常也无法在运行时接管；
- 本项目的实际调试手段是：
  1. `master --monitor` 观察 USB CDC 数据流；
  2. `slave --monitor` 观察/注入从机 UART 数据（需主机已激活从机）；
  3. `--debug` 观察 ISP 层；
  4. `info` + LED 判断上电/枚举状态。

如果确实需要源码级调试，需要改板或改固件（保留调试脚、牺牲该场景下的 USB/UART 复用），不属于 Flasher 的能力范围。

---

## 7. 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| `Flasher CDC port not found` | 没装 pyserial；USB-C 线只有供电没有数据；Flasher 没被识别。检查设备管理器中的 `1A86:5721`，或显式 `--port COMx` |
| 能找到端口但 `no response from Flasher` | 端口被其他串口终端占用；先关闭终端，或换一个 COM 口 |
| `Flasher did not return to command mode` | 桥接模式未退出。`reset` 重试；仍失败则拔插 Flasher 的 USB-C，再执行一次命令 |
| `no target` / `target=0` | Dongle 没插好；USB-A 没供电；目标没进 ISP。先 `power isp`，再 `info` 确认 `4348:55e0`/`1a86:55e0` |
| 擦除/写入报 `status 5`，PROGRAM 返回 `FE` | Code Flash 未解锁。确认 `flasher.py` 的 `prepare_target()` 走了 CH570/CH572 分支，并用 `--debug` 看 `CFG` 是否已变成 `a5 5a` |
| 校验失败 | 镜像不是从 `0x00000000` 链接的；镜像超过 240 KiB；烧录过程中 Dongle 松动。重新 `make` 后 `--verify` 再试 |
| 烧录成功但 PC 看不到 Dongle 的串口 | 烧录后目标默认是 **slave**，不会枚举 USB。执行 `master`，或把 Dongle 直接插 PC 的 USB-A 口 |
| `master` 报 `no target` / `unsupported` | 目标没有枚举成 USB CDC：可能还在 ISP、镜像无效、或 Dongle 被采样成了 slave。先 `flash`，再 `master` |
| `slave --monitor` 能连上但收不到数据 | 从机 UART 在收到射频主机的 Reset 前是 suspended；需要另一只 master Dongle 建立链路，或这只是预期的空数据 |
| 串口数据是乱码 | `--baud` 与从机实际波特率不一致；从机波特率由射频 Reset 下发，默认 115200 |
| LED 1 Hz 闪烁且无串口 | Flasher 未枚举：换数据线、换 USB 口、确认 Windows 加载了 CDC 驱动 |
| 烧录时好时坏 | 不要热插拔；确保 Dongle 完全插入；避免在擦除/写入期间碰触 |

---

## 8. 文件与源码索引

| 内容 | 路径 |
| --- | --- |
| 主机工具 | `..\Flasher\firmware\tools\flasher.py` |
| Flasher 命令/模式实现 | `..\Flasher\firmware\src\app\app.cpp` |
| Flasher USB 主机与上电时序 | `..\Flasher\firmware\src\usb\host.cpp` |
| Flasher ISP 实现 | `..\Flasher\firmware\src\isp\isp.cpp` |
| Flasher UART 桥接引脚定义 | `..\Flasher\firmware\include\ch582\serial\uart.hpp` |
| Flasher 固件构建 | `..\Flasher\firmware\Makefile` |
| Dongle 入口与角色判定 | `firmware\src\main.cpp` |
| Dongle 串口 | `firmware\src\serial\uart.cpp` |
| Dongle USB CDC | `firmware\src\usb\usb_device.cpp` |
| Dongle 射频串口协议 | `firmware\src\radio\rf_uart.cpp` |
| Dongle 链接脚本 | `firmware\ld\ch572.ld` |

---

## 9. 一页速查

```powershell
# 1) 构建
cd C:\Users\14491\Desktop\Dongle\firmware
make

# 2) 烧录并校验（自动找 COM 口）
python ..\..\Flasher\firmware\tools\flasher.py flash build\rf_uart_app.bin --verify

# 3) 作为 USB 主机运行并监控
python ..\..\Flasher\firmware\tools\flasher.py master --monitor

# 4) 作为 UART 从机运行并监控
python ..\..\Flasher\firmware\tools\flasher.py slave --baud 115200 --monitor

# 5) 只看状态 / 断电 / 强制进 ISP
python ..\..\Flasher\firmware\tools\flasher.py info
python ..\..\Flasher\firmware\tools\flasher.py power off
python ..\..\Flasher\firmware\tools\flasher.py power isp
```
