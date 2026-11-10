# CH570/CH572 RF UART Dongle 固件

本目录是 Dongle 板的完整固件工程，使用 C++17 重写。固件在 USB 虚拟串口和
2.4 GHz 私有无线链路之间做双向透传，并带有序号与 ACK 机制。

## 功能

- USB CDC-ACM 虚拟串口，使用系统自带驱动，`VID:PID = 1A86:5720`。
- 2.4 GHz 私有链路，固定 2 Mbps PHY、通道 17、地址 `0x57250425`。
- 主机轮询式链路：只有 REQ/RSP 两种包，长度 0 表示 poll/ACK；设备回显
  `sequence`，重复请求重发上一次响应。
- CDC line coding 不再经 RF 转发，本地固定回 115200 8N1。
- 板载 LED：USB 未枚举时 500 ms 闪烁，枚举成功后常亮。
- 100 MHz 系统时钟（32 MHz HSE + PLL）。
- 直接从 `0x00000000` 启动，不包含引导程序；重新烧录使用 SWD 或 ISP。

## 目录结构

```text
firmware/
├─ Makefile
├─ ld/ch572.ld         链接脚本（Makefile 传入 Flash 窗口）
├─ include/ch572/
│  ├─ compiler.hpp
│  ├─ platform/       寄存器、中断、时钟、SysTick
│  ├─ usb/            USB 设备、CDC-ACM 描述符
│  ├─ radio/          RFIP/RFRole、RFEND 校准、RF UART 协议
│  └─ util/           环形缓冲区
├─ src/
│  ├─ platform/       启动代码、时钟、SysTick
│  ├─ usb/
│  ├─ radio/
│  └─ main.cpp
```

## 分层

- `platform/`：只包含芯片寄存器访问和最小驱动，不依赖应用逻辑。
- `usb/`：`UsbDevice` 负责 USB 设备枚举、CDC-ACM 数据端点和收发缓冲；
  `ByteSource` 接口把“向主机发送”的数据源解耦。
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
```

## 构建

需要 MounRiver Studio 3 自带的 WCH GCC 15 工具链：

```text
C:/MounRiver/MounRiver_Studio3/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
```

### Make（本机已验证）

```sh
cd firmware
make            # 应用：build/rf_uart_app.hex
make clean      # 清理构建输出
```

链接地址固定为 `0x00000000`，Flash 窗口为 240 KiB，镜像可直接烧录运行。

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

使用 SWD 或 WCHISPTool（串口/USB ISP）直接烧录：

```text
build/rf_uart_app.hex
build/rf_uart_app.bin
```

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

## 资源占用（GCC 15 `-Os`）

| 目标 | Flash | RAM |
|------|-------|-----|
| `rf_uart_app` | 6620 B（约 6.5 KiB） | 3684 B（约 3.6 KiB） |

- Flash 可用 240 KiB，RAM 为 12 KiB。

## 验证状态

- `make`：通过。
- 串口 ISP 烧录（WCHISPTool，CH570/CH572）：通过。
- USB CDC-ACM 枚举、2.4 GHz 透传：需要实机验证。链路使用固定地址/信道和
  REQ/RSP，不再支持原配对帧。
