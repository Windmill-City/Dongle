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

## 目录结构

```text
firmware/
├─ Makefile
├─ ld/ch572.ld
├─ include/ch572/
│  ├─ config.hpp
│  ├─ platform/       寄存器、中断、时钟、GPIO、SysTick
│  ├─ hal/            LED
│  ├─ usb/            USB 设备、CH341 兼容层、描述符
│  ├─ radio/          RFIP/RFRole、RFEND 校准、RF UART 协议
│  └─ util/           环形缓冲区
├─ src/
│  ├─ boot/startup_ch572.S
│  ├─ platform/
│  ├─ hal/
│  ├─ usb/
│  ├─ radio/
│  └─ main.cpp
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
make
make size
make clean
```

输出：

```text
build/rf_uart_dongle.elf
build/rf_uart_dongle.hex
build/rf_uart_dongle.bin
build/rf_uart_dongle.map
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

使用 `build/rf_uart_dongle.hex`。

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

## 资源占用（默认配置）

- Flash：约 10.5 KiB / 240 KiB
- RAM：约 5.2 KiB / 12 KiB

## 验证状态

- `make`：通过。
- 串口 ISP 烧录（wchisp，CH570/CH572）：通过。
- USB 枚举、2.4 GHz 配对和透传：需要实机验证。
