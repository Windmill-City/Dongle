# 烧录与调试

Dongle 通过 [Flasher](https://github.com/Windmill-City/Flasher) 烧录和调试。Flasher 提供 USB-C 到 PC、USB-A 到 Dongle 的受控供电与数据桥接。

## 连接

```text
PC ──USB-C──> Flasher ──USB-A──> Dongle
```

Dongle 是 USB-A 公头，直接插入 Flasher 的 USB-A 口即可。烧录和调试期间不要额外供电或热插拔。

## 快速开始

前置条件：Python 3 + pyserial、GNU Make、MounRiver Studio 3 的 RISC-V GCC 工具链（路径可在 `firmware/Makefile` 中修改）。

在本仓库根目录执行：

```powershell
pip install pyserial
cd firmware
make
cd ..
python ..\Flasher\firmware\tools\flasher.py flash firmware\build\rf_uart_app.bin --verify
```

`flasher.py` 会按 VID/PID 自动查找串口；找不到时加 `--port COMx`。

## 运行模式

| 目的 | 命令（在本仓库根目录执行） |
| --- | --- |
| USB 主机（master） | `python ..\Flasher\firmware\tools\flasher.py master --monitor` |
| UART 从机（slave） | `python ..\Flasher\firmware\tools\flasher.py slave --baud 115200 --monitor` |
| 断电 / 应用 / ISP | `python ..\Flasher\firmware\tools\flasher.py power off`、`app`、`isp` |
| 查看状态 | `python ..\Flasher\firmware\tools\flasher.py info` |
| 退出桥接 | `python ..\Flasher\firmware\tools\flasher.py reset` |

说明：

- **master**：Dongle 枚举为 CDC `1A86:5720`，PC 侧看到的是 Flasher 的 `1A86:5721`，数据经无线链路透传。
- **slave**：`PA0`/`PA1` 作为 UART（默认 115200 8N1）。从机 UART 上电后处于 suspended，需收到无线主机的 Reset 帧后才启用；单独执行 `slave --monitor` 收不到数据是正常现象。
- 烧录完成后目标默认以 **slave** 启动，不会自动枚举 USB；需要 USB 模式时执行 `master`。
- 桥接模式下发送 **BREAK** 可返回命令模式；每次调用 `flasher.py` 也会先自动 BREAK。

## 烧录说明

- 使用 `.bin` 镜像，最大 240 KiB，从 `0x00000000` 开始；脚本自动补齐到 1 KiB 整数倍。
- Flasher 自动控制 VBUS 和 D+，无需按 BOOT 键：进入 ISP → 识别 → 必要时解锁 → 擦除 → 写入 → 校验 → 重启到应用。
- CH570Q 的 Code Flash 保护标记位于 Config 首字；需要时脚本自动写入 `0xA5 0x5A` 解锁，只需一次。
- 每次写入最多 56 字节；`--verify` 会在写入后回读校验。

## 常见问题

| 现象 | 处理 |
| --- | --- |
| `Flasher CDC port not found` | 安装 pyserial；更换数据线或 USB 口；或用 `--port COMx` |
| `no response from Flasher` | COM 口被其他串口终端占用，先关闭终端 |
| `no target` | Dongle 未插好；先执行 `power isp`，再用 `info` 确认 |
| `status 5` / 写不进去 | 解锁失败，重新执行 `flash` |
| 烧录成功但 PC 看不到串口 | 默认是 slave，执行 `master` |
| `slave` 收不到数据 | 从机等待无线主机的 Reset，属正常 |
| 串口乱码 | 波特率不一致，默认 115200 |

## 参考

命令协议、ISP 实现与 KiCad 3D 渲染见 [Flasher README](https://github.com/Windmill-City/Flasher)。
