# CH570Q Dongle 固件

基于沁恒 `CH572EVT` 中的 RF_UartDongle 例程，适配本工程的 Dongle 板。
固件仅保留两项功能：

- USB 虚拟串口与 2.4G 无线串口双向透传；
- 板载 LED 指示：未枚举时 500ms 闪烁，枚举成功后常亮。

调试串口、`printf`、日志和未使用的外设驱动均已移除。

## 目录结构

```
firmware/
├─ Makefile
├─ README.md
├─ link.ld                   链接脚本
├─ startup_ch572.s           启动文件
├─ src/
│  ├─ main.c                 应用入口、LED、主循环
│  ├─ app/
│  │  ├─ buf.c               环形缓冲区
│  │  ├─ rf.c                2.4G 收发底层
│  │  ├─ rf_uart_rx.c        USB <-> 2.4G 透传与配对
│  │  └─ usb_uart.c          USB 串口设备
│  └─ drivers/
│     ├─ ch57x_clk.c         时钟
│     ├─ ch57x_gpio.c        GPIO
│     └─ ch57x_sys.c         系统时钟与启动
├─ include/
│  ├─ app/                   应用头文件
│  ├─ drivers/               外设库头文件
│  └─ chip/                  ch572rf.h、core_riscv.h
├─ lib/                      libch57x_rf.a
└─ build/                    编译产物（不提交）
```

## 硬件对应关系

| 板子 | 芯片引脚 | 固件中的用途 |
|------|----------|--------------|
| USB VBUS | V5 (pin 2) | 5V 供电，内部 LDO 出 3.3V |
| USB D- | PA0 (pin 3) | UDM，USB 设备模式 |
| USB D+ | PA1 (pin 4) | UDP，内部 1.5k 上拉 |
| LED | PA7 (pin 5) | 高电平点亮 |
| 32MHz 晶体 | XI/XO (pin 9/8) | HSE + PLL 100MHz |

`main.c` 中的行为：

- 上电点亮 LED；
- USB 未枚举时 LED 500ms 闪烁，枚举成功后常亮；
- 关闭两线调试，把 PA0/PA1 让给 USB；
- 使用 `SYS_GetSysTickCnt()` 做 LED 定时。

## 编译

需要 MounRiver Studio 3 自带的 WCH GCC 15 工具链
（`riscv32-wch-elf-*`，支持 WCH XW 扩展，不能换成通用 GCC）。

在 `firmware` 目录执行：

```
make
make clean
```

工具链不在默认路径时：

```
make MRS_TOOLCHAIN="C:/path/to/RISC-V Embedded GCC15/bin"
```

输出：

```
build/rf_uart_dongle.hex
build/rf_uart_dongle.bin
build/rf_uart_dongle.elf
build/rf_uart_dongle.map
```

编译参数为 `-march=rv32imcb -mabi=ilp32`。

## 烧录

编译后使用 `build/rf_uart_dongle.hex`。可选方式：

1. **USB ISP**：WCHISPTool 选 CH570，USB 下载；上电瞬间 PA1(D+) 要为高。
   本板没有下载按键，需要飞线/探针把 PA1 顶到 3.3V 再插 USB。
2. **串口 ISP（免按键）**：PA0=TXD、PA1=RXD，USB-TTL 接 D-/D+，先点下载再上电。
3. **SWD**：PA0=SWDIO、PA1=SWCLK，WCH-LinkE 直接下载调试。

## USB / 无线说明

- USB：CH341/CH340 兼容的厂商串口设备，`VID:PID = 1a86:7523`，
  产品名 “USB2.0 To Serial Port”。
- 2.4G：默认 2Mbps PHY，通道 17，地址 `0x57250425`，与官方 RF_Uart
  （发送端）例程配对使用；配对成功后 dongle 把无线数据从 USB 串口吐出，
  同时把串口数据回传无线端。
