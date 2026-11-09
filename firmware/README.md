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
- 支持 USB IAP 升级：应用可通过串口魔术字主动进入引导程序，USB 枚举失败
  5 s 后也会自动回退到引导程序。

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
└─ tools/             USB IAP、IAP 魔术字、HEX 合并脚本
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
- 标志值 `0xAA` 表示应用有效，`0x55` 表示请求进入 IAP；软复位、看门狗复位和
  手动复位后标志仍然有效（应用请求 IAP 后若先被看门狗复位也能进入 ISP），
  上电复位会丢弃 SRAM 中的旧值。
- 应用运行中收到 USB 串口魔术字 `55 AA 49 41 50` 后写入 `0x55` 并软复位，
  引导程序随后进入 USB ISP。
- USB ISP 设备为 `VID:PID = 4348:55E0`，命令：`0x80` 编程、`0x81` 擦除、
  `0x82` 校验、`0x83` 结束并跳转应用。
- 引导程序保留在 `0x00000000`，应用升级只擦写 `0x00002000` 之后的区域。

## 资源占用（默认配置，GCC 15 `-Os`）

| 目标 | Flash | RAM |
|------|-------|-----|
| 应用 `rf_uart_dongle_iap` | 10632 B（约 10.4 KiB） | 6596 B（约 6.4 KiB） |
| 引导程序 `rf_uart_iap` | 4620 B（约 4.5 KiB） | 3252 B（约 3.2 KiB） |

- 应用 Flash 可用 232 KiB，引导程序可用 8 KiB，RAM 均为 12 KiB。

## 验证状态

- `make`：通过。
- `make iap_boot`：通过。
- 串口 ISP 烧录（wchisp，CH570/CH572）：通过。
- 自包含闪存驱动：编译和链接通过，擦写时序需要实机验证。
- USB 枚举、2.4 GHz 配对和透传：需要实机验证。
