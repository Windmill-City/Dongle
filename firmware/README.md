# CH570/CH572 RF UART Dongle 固件

本目录是 Dongle 板的完整固件工程，使用 C++17 重写。固件在 USB 虚拟串口和
2.4 GHz 私有无线链路之间做双向透传，并带有序号与 ACK 机制。

## 功能

- USB 复合设备：CDC-ACM 虚拟串口 + 仅特性报告的 HID 控制接口，使用系统
  自带驱动，不再兼容 CH341，`VID:PID = 1A86:5720`。
- 2.4 GHz 私有链路，固定 2 Mbps PHY、通道 17、地址 `0x57250425`。
- 主机轮询式链路：只有 REQ/RSP 两种包，长度 0 表示 poll/ACK；设备回显
  `sequence`，重复请求重发上一次响应。
- CDC line coding 不再经 RF 转发，本地固定回 115200 8N1。
- 板载 LED：USB 未枚举时 500 ms 闪烁，枚举成功后常亮。
- 100 MHz 系统时钟（32 MHz HSE + PLL）。
- 支持运行中升级：主机通过 HID 特性报告选择升级应用区或引导区，数据走
  CDC-ACM 串口；普通串口数据不会触发升级。
- 升级应用：应用软复位进入引导程序，由引导程序用同一套帧协议重写
  `0x2000` 之后的应用区。
- 升级 IAP 引导程序：应用只置标志并复位；引导程序把新镜像写入 staging，
  CRC 通过后用 RAM 例程搬运到 `0x0000–0x1FFF`。

## 目录结构

```text
firmware/
├─ Makefile
├─ ld/ch572.ld         应用/引导程序共用链接脚本（Makefile 传入 Flash 窗口）
├─ include/ch572/
│  ├─ config.hpp
│  ├─ platform/       寄存器、中断、时钟、SysTick、Flash
│  ├─ usb/            USB 设备、CDC-ACM/HID 描述符
│  ├─ radio/          RFIP/RFRole、RFEND 校准、RF UART 协议
│  └─ util/           环形缓冲区
├─ src/
│  ├─ boot/           startup_ch572.S、iap_main.cpp、iap_update.cpp
│  ├─ platform/       寄存器、时钟、SysTick、Flash、IAP 标志
│  ├─ usb/
│  ├─ radio/
│  └─ main.cpp
└─ tools/             应用/IAP 升级工具（upgrade.py、common.py）
```

## 分层

- `platform/`：只包含芯片寄存器访问和最小驱动，不依赖应用逻辑。
- `usb/`：`UsbDevice` 负责 USB 设备枚举、CDC-ACM 数据端点、HID 特性报告
  和收发缓冲；`ByteSource` 接口把“向主机发送”的数据源解耦。
- `radio/rfend.*`：RFEND 校准/调谐逻辑的源码实现。
- `radio/rfip.*`：RFIP/RFRole 前端的源码实现，`TxParams`/`RxParams`
  保持与原始接口一致的结构布局。
- `radio/rf_uart.*`：固定地址/信道的透传协议，实现 `usb::ByteSource`。
- `main.cpp`：组合根，只负责初始化顺序和主循环。

## 数据通路

```text
USB OUT (EP2) -> UsbDevice::irq_handler -> RingBuffer<256>

RF REQ -> RfUart::process_rx
       -> UsbDevice::read_host (RingBuffer)
       -> RF RSP payload (USB -> RF)

RF REQ payload -> RfUart::process_rx -> RingBuffer<256>
               -> UsbDevice::poll (ByteSource)
               -> USB IN (EP2)

USB HID 特性报告 (EP0) -> UsbDevice::handle_setup
                       -> app_update_requested / boot_update_requested
```

## 构建

需要 MounRiver Studio 3 自带的 WCH GCC 15 工具链：

```text
C:/MounRiver/MounRiver_Studio3/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
```

### Make（本机已验证）

```sh
cd firmware
make            # 应用：build/app/rf_uart_app.hex
make iap_boot   # 引导程序：build/boot/rf_uart_iap.hex
make clean
```

## 关于 RF 前端

工程不依赖任何预编译 RF 静态库。RFIP/RFRole 前端、RFEND 校准/调谐和
2.4 GHz 收发时序全部位于 `src/radio/`：

- `rfip.cpp`：接入地址、CRC、固定 2 Mbps PHY、收发启动、IRQ 处理。
- `rfend.cpp`：TX 电容/增益校准、RX 滤波器与 ADC 校准、调谐等待逻辑。
- `rf_uart.cpp`：固定地址/信道、REQ/RSP、序号/ACK 和双向透传。

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

首次烧录引导程序和应用使用 SWD 或 WCHISPTool（串口/USB ISP）：
`build/boot/rf_uart_iap.hex`、`build/app/rf_uart_app.hex`。之后两者都可以
在运行中升级，不再依赖 ISP。

### 串口 ISP（首次/恢复）

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
| 应用 | `0x00002000`–`0x00039FFF` | 224 KiB |
| Boot staging | `0x0003A000`–`0x0003BFFF` | 8 KiB |

- IAP 标志放在 SRAM 的 `.noinit` 区（`0x20002FF0`），复位后仍保留，不占用 Flash。
- 标志值 `0xAA` 表示应用初始化完成，`0x55` 表示下一次启动停在引导程序的
  应用升级模式，`0x5B` 表示下一次启动执行 boot 自更新；软复位、看门狗复位
  和手动复位后标志仍然有效，上电复位会丢弃 SRAM 中的旧值。
- 引导程序在跳转应用前先写入 `0x55`，应用仅在 USB 枚举和射频初始化成功后
  写入 `0xAA`。因此应用启动阶段若复位，引导程序会停留在升级模式，而不是反复
  启动损坏的应用。
- 运行时进入升级使用 HID 特性报告 1，不经过串口数据流。报告载荷为
  `命令 | 0x5A | 'I' | 'A' | 'P' | 0 0 0`，命令 `0x01` 升级应用、
  `0x02` 升级引导区。HID 没有中断端点，固件不回状态；两种升级都会复位
  进入引导程序，主机等待 CDC 串口重新出现。
- 升级应用：应用软复位进入引导程序，引导程序用同样的 CDC-ACM 串口和帧协议
  擦写 `0x00002000`–`0x00039FFF`，CRC 通过后确认应用并复位。
- 升级引导区：应用收到命令后只置 `0x5B` 标志并复位；引导程序把新镜像写到
  `0x0003A000`–`0x0003BFFF` 的 staging 区，CRC 通过后关闭中断，用 RAM 中的
  搬运例程擦写 `0x00000000`–`0x00001FFF` 并复位。搬运期间掉电会失去引导，
  需要 SWD/串口 ISP 恢复。

### 升级（应用运行中）

统一使用 `tools/upgrade.py`：先通过 HID 特性报告进入对应升级模式，再通过
CDC-ACM 串口（`VID:PID = 1A86:5720`）发送帧协议。

```sh
pip install pyserial hidapi
python tools/upgrade.py app build/app/rf_uart_app.bin
python tools/upgrade.py iap build/boot/rf_uart_iap.bin
```

- 不指定 `--port` 时自动查找该 VID/PID 的 CDC-ACM 串口；两种升级都会等待设备
  复位后重新出现的串口（引导程序同样使用 CDC-ACM 接口）。
- 升级触发使用标准 HID 特性报告，不需要替换驱动或安装 UsbDk/WinUSB。
  无法触发时用 SWD 或串口 ISP 恢复/烧录。
- 帧格式：`A5 | CMD | LEN_LO LEN_HI | PAYLOAD | XOR`，每条命令回一个状态字节。
  `0x01` 擦除（4 字节长度）、`0x02` 写数据（地址 + 数据）、
  `0x04` 结束（4 字节 CRC32）。
- 升级过程中掉电时需要用 SWD 或串口 ISP 恢复。

## 资源占用（默认配置，GCC 15 `-Os`）

| 目标 | Flash | RAM |
|------|-------|-----|
| 应用 `rf_uart_app` | 6988 B（约 6.8 KiB） | 3920 B（约 3.8 KiB） |
| 引导程序 `rf_uart_iap` | 5224 B（约 5.1 KiB） | 3336 B（约 3.3 KiB） |

- 应用 Flash 可用 224 KiB，引导程序可用 8 KiB（剩余约 2.9 KiB），RAM 均为 12 KiB。

## 验证状态

- `make`：通过。
- `make iap_boot`：通过。
- 串口 ISP 烧录（WCHISPTool，CH570/CH572）：通过。
- 自包含闪存驱动：编译和链接通过，擦写时序需要实机验证。
- boot 自更新（staging + RAM 搬运）：编译和链接通过，需实机验证 staging 擦写、
  RAM 搬运和复位流程。
- 引导程序更新应用区：编译和链接通过，需实机验证应用复位、重新枚举和擦写流程。
- USB CDC-ACM/HID 复合枚举、HID 升级触发、2.4 GHz 透传：需要实机验证。
  链路已改为固定地址/信道和 REQ/RSP，不再支持原配对帧。
