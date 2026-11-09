# CH570/CH572 RF UART Dongle 固件

本目录是 Dongle 板的完整固件工程，使用 C++17 重写。固件在 USB 虚拟串口和
2.4 GHz 私有无线链路之间做双向透传，并带有配对/绑定、序号与 ACK 机制。

## 功能

- USB 厂商串口设备，兼容 CH341 驱动，`VID:PID = 1A86:7523`。
- 2.4 GHz 私有链路，默认 2 Mbps PHY、通道 17、地址 `0x57250425`。
- 自动配对：接收 `BOUND_REQ`，按 `server_data` 或 RSSI 阈值接受，并回
  `BOUND_RSP`，随机生成新的接入地址和信道。
- 主机轮询式数据链路：`GET_STATUS`、`DATA_FLAG`、`STATUS_RSP`、`DATA_ACK`
  与单调递增的 `sequence`。
- 板载 LED：USB 未枚举时 500 ms 闪烁，枚举成功后常亮。
- 100 MHz 系统时钟（32 MHz HSE + PLL）。
- 支持 USB IAP 升级：应用通过 USB 厂商控制请求主动进入引导程序；普通串口
  数据不再触发升级。
- 支持在应用运行中通过 USB 直接重写 `0x0000–0x1FFF` 的 IAP 引导程序
  （带整区 CRC 校验），同样使用厂商控制请求进入更新模式。

## 目录结构

```text
firmware/
├─ Makefile
├─ ld/ch572_app.ld     应用链接脚本（0x2000 起始）
├─ ld/ch572_iap.ld     引导程序链接脚本（0x0 起始）
├─ include/ch572/
│  ├─ config.hpp
│  ├─ platform/       寄存器、中断、时钟、GPIO、SysTick、Flash
│  ├─ hal/            LED
│  ├─ usb/            USB 设备、CH341 兼容层、ISP、描述符
│  ├─ radio/          RFIP/RFRole、RFEND 校准、RF UART 协议
│  └─ util/           环形缓冲区
├─ src/
│  ├─ boot/           startup_ch572.S、iap_main.cpp
│  ├─ platform/       寄存器、时钟、GPIO、SysTick、Flash、IAP 标志
│  ├─ hal/
│  ├─ usb/
│  ├─ radio/
│  └─ main.cpp
├─ sdk/               WCH 外设库（仅作参考，已不参与构建）
└─ tools/             USB IAP、IAP 厂商请求、引导区更新、HEX 合并脚本
```

## 分层

- `platform/`：只包含芯片寄存器访问和最小驱动，不依赖应用逻辑。
- `usb/`：`Ch341Serial` 负责 USB 设备枚举、端点、CH341 控制请求和串口
  收发缓冲；`ByteSource` 接口把“向主机发送”的数据源解耦。
- `radio/rfend.*`：RFEND 校准/调谐逻辑的源码实现。
- `radio/rfip.*`：RFIP/RFRole 前端的源码实现，`TxParams`/`RxParams`
  保持与原始接口一致的结构布局。
- `radio/rf_uart.*`：配对与透传协议状态机，实现 `usb::ByteSource`。
- `main.cpp`：组合根，只负责初始化顺序和主循环。

## 数据通路

```text
USB OUT (EP2) -> Ch341Serial::irq_handler
              -> RingBuffer<256>
              -> RfUart::process_rx (GET_STATUS / DATA_FLAG)
              -> RF TX

RF RX -> RfUart::process_rx
      -> RingBuffer<512>
      -> Ch341Serial::poll (ByteSource)
      -> USB IN (EP2)
```

## 构建

需要 MounRiver Studio 3 自带的 WCH GCC 15 工具链：

```text
C:/MounRiver/MounRiver_Studio3/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
```

### Make（本机已验证）

```sh
cd firmware
make            # 应用：build/app/rf_uart_dongle_iap.hex
make iap_boot   # 引导程序：build/boot/rf_uart_iap.hex
make clean
```

## 关于 RF 前端

工程不依赖任何预编译 RF 静态库。RFIP/RFRole 前端、RFEND 校准/调谐和
2.4 GHz 收发时序全部位于 `src/radio/`：

- `rfip.cpp`：接入地址、CRC、PHY 模式、收发启动、IRQ 处理。
- `rfend.cpp`：TX 电容/增益校准、RX 滤波器与 ADC 校准、调谐等待逻辑。
- `rf_uart.cpp`：配对、轮询、序号、ACK 和双向透传。

上述实现已经通过编译和结构布局检查，但当前环境没有硬件，射频行为仍需
实机验证。

## 硬件对应关系

| 板子 | 芯片引脚 | 用途 |
|------|----------|------|
| USB VBUS | V5 | 5 V 供电，内部 LDO 3.3 V |
| USB D- | PA0 | UDM |
| USB D+ | PA1 | UDP，内部 1.5k 上拉 |
| LED | PA7 | 高电平点亮 |
| 32 MHz 晶体 | XI/XO | HSE + PLL 100 MHz |

## 烧录

应用使用 `build/app/rf_uart_dongle_iap.bin`（经 USB IAP 刷入）；引导程序使用
`build/boot/rf_uart_iap.hex`（首次用 SWD 刷入一次）。

### 串口 ISP

CH340/USB-TTL 接线：

| USB-TTL | Dongle | 芯片引脚 |
|---------|--------|----------|
| TXD | D- | PA0（UART2 RXD） |
| RXD | D+ | PA1（UART2 TXD） |
| GND | GND | GND |


### 其他方式

1. **USB ISP**：WCHISPTool 选择 CH570，USB 下载；上电瞬间 PA1(D+) 为高。
2. **SWD**：PA0=SWDIO、PA1=SWCLK，WCH-LinkE 直接下载调试。

### IAP 与 Flash 布局

| 区域 | 地址范围 | 大小 |
|------|----------|------|
| 引导程序 | `0x00000000`–`0x00001FFF` | 8 KiB |
| 应用 | `0x00002000`–`0x0003BFFF` | 232 KiB |

- IAP 标志放在 SRAM 的 `.noinit` 区（`0x20002FF0`），软复位后仍保留，不占用 Flash。
- 标志值 `0xAA` 表示应用初始化完成，`0x55` 表示下一个目标仍是 IAP；软复位、
  看门狗复位和手动复位后标志仍然有效，上电复位会丢弃 SRAM 中的旧值。
- 引导程序在跳转应用前先写入 `0x55`，应用仅在 USB 枚举和射频初始化成功后
  写入 `0xAA`。因此应用启动阶段若复位，引导程序会停留在 ISP，而不是反复
  启动损坏的应用。
- 运行时进入 IAP 使用 USB 厂商控制请求（EP0），不经过串口数据流：
  `bmRequestType=0x40`、`bRequest=0xB1`、`wValue=0x5A01`、`wIndex=0x4941`。
  引导程序随后进入 USB ISP。
- USB ISP 设备为 `VID:PID = 4348:55E0`，命令：`0x80` 编程、`0x81` 擦除、
  `0x82` 校验、`0x83` 结束并跳转应用。
- 引导程序保留在 `0x00000000`，应用升级只擦写 `0x00002000` 之后的区域。
- 应用自身也可以更新引导程序区：厂商请求 `wValue=0x5A02` 进入更新模式后，
  直接擦写 `0x00000000`–`0x00001FFF`，最后用整区 CRC32 校验并复位。
  `0x00000000` 的链接和烧写只在应用里发生，引导程序本身无法擦除自己。

运行时触发（需要 `pyusb`；Windows 需 UsbDk 或 WinUSB/libusbK）：

```sh
python tools/iap_trigger.py          # 跳转到 IAP 引导程序
python tools/iap_trigger.py --boot   # 进入引导程序区更新模式
```

### 更新 IAP 引导程序区（应用运行中）

使用 `tools/app_boot_update.py`，先通过 `tools/iap_trigger.py` 的厂商控制请求
（`wValue=0x5A02`）进入更新模式，再通过应用的 CH341 串口
（`VID:PID = 1A86:7523`）发送新的 8 KiB 引导程序镜像：

```sh
python tools/app_boot_update.py build/boot/rf_uart_iap.bin COM5
```

运行时触发依赖 `pyusb` 能在 CH341 驱动占用设备时发送控制请求（Windows 需
UsbDk 或 WinUSB/libusbK）。无法发送控制请求时，用 SWD 或串口 ISP 恢复/烧录。

- 镜像可以是 `.bin`，也可以是起始地址为 `0x0` 的 `.hex`；不足 8 KiB 的部分
  用 `0xFF` 填充。
- 帧格式：`A5 | CMD | LEN_LO LEN_HI | PAYLOAD | XOR`，每条命令回一个状态字节。
  `0x01` 擦除、`0x02` 写数据、`0x03` 校验、`0x04` 结束（4 字节 CRC32）、
  `0x05` 中止（仅在擦除前可用）。
- 擦除开始后必须完整写入并通过 CRC 才会复位；更新过程中掉电时 `0x0` 可能
  只剩下不完整的引导程序，需要用 SWD 或串口 ISP 恢复。

## 资源占用（默认配置，GCC 15 `-Os`）

| 目标 | Flash | RAM |
|------|-------|-----|
| 应用 `rf_uart_dongle_iap` | 12936 B（约 12.6 KiB） | 7652 B（约 7.5 KiB） |
| 引导程序 `rf_uart_iap` | 4652 B（约 4.5 KiB） | 3252 B（约 3.2 KiB） |

- 应用 Flash 可用 232 KiB，引导程序可用 8 KiB，RAM 均为 12 KiB。

## 验证状态

- `make`：通过。
- `make iap_boot`：通过。
- 串口 ISP 烧录（wchisp，CH570/CH572）：通过。
- 自包含闪存驱动：编译和链接通过，擦写时序需要实机验证。
- 应用内更新 IAP 引导程序区：编译和链接通过，需实机验证擦写与复位流程。
- USB 枚举、2.4 GHz 配对和透传：需要实机验证。
