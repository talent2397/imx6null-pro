# 学习笔记 — 机器人关节智能监测网关

> 格式: 知识点 → 来源文件路径（不贴代码块，用文件路径/行号回溯）
> 每阶段的知识点和 Q&A 按时间顺序追加

---

## 阶段 1: 硬件抽象层 (HAL)

### 知识点

#### 1. 项目架构概览

- **分层架构**: app(应用) → engine(业务) → hal(硬件抽象) → platform(平台) → common(通用)
- **线程模型**: 主线程 + IMU线程 + CAN线程 + Audio线程 + Button线程 + TCP线程
- **数据流**: 传感器 → ringbuf/mqueue → epoll 事件循环 → 诊断引擎 → LED/Audio/网络
- 参考: `docs/需求.md` §5

#### 2. IIO 子系统基础

- IIO (Industrial I/O) 是 Linux 为低速工业传感器（IMU/ADC/DAC）设计的内核子系统
- 设备节点: `/dev/iio:deviceN`，sysfs 属性目录: `/sys/bus/iio/devices/iio:deviceN/`
- 两种读取方式:
  - **sysfs 慢速读取**: 按需读 `_raw`、`_scale` 文件，适合低频(<50Hz)
  - **buffer 高速读取**: 通过 `/dev/iio:deviceN` + trigger，连续批量读，适合 100Hz+
- 物理值 = raw × scale（如 `in_accel_x_raw × in_accel_scale`）
- scale 文件可能用科学计数法表达
- 参考: 暂无代码文件，后续见 `src/hal/imu.c`

#### 3. .ko 内核驱动 vs 应用层封装

- `.ko` (Kernel Object) 是内核模块二进制，运行在内核空间（类比 Windows 的 .sys 驱动）
- **内核驱动做的事**: SPI/I2C 总线操作芯片寄存器 → 数据挂载到 IIO 框架 → IIO 框架生成 sysfs 节点
- **应用层做的事**: fopen/fscanf/fprintf 读写 sysfs 文件 → 不需要碰寄存器
- 当前板子 ICM-20608 驱动已内置，阶段 1 只需写应用层封装 `src/hal/imu.c`
- 参考: `include/hal/imu.h`(待创建), `src/hal/imu.c`(待创建)

#### 4. Linux IMU 驱动全流程（从裸板到读数）

**六步走**: 原理图 → 设备树(.dts) → 驱动源码(.c) → 编译(.ko) → 部署到板子 → 验证

1. **看原理图**: 确定总线类型(SPI/I2C)、片选、中断引脚、最大时钟频率
2. **写设备树**: 告诉内核"什么总线上挂了一个什么芯片"
   - 关键属性: `compatible`（匹配驱动）、`reg`（片选号）、`spi-max-frequency`、`interrupts`
   - SPI 设备挂 `ecspi3` 等总线节点下，I2C 设备挂 `i2cN` 节点下
   - 设备树编译产物: `.dtb`（二进制）、`.dts`（源码）、`.dtsi`（包含文件，放公共定义）
   - 内核通过 `of_match_table` 中的 `compatible` 字符串匹配 `.dts` 和驱动
3. **驱动源码结构** (驱动四层: 硬件 → 总线API → IIO框架 → 用户空间):
   - 寄存器映射（查芯片数据手册，如 WHO_AM_I=0x75, ACCEL_XOUT_H=0x3B）
   - 底层 SPI 读写（`spi_write_then_read`，注意寄存器地址最高位=1表示读）
   - 芯片初始化（读 WHO_AM_I 验证 → 唤醒 → 配置量程/采样率）
   - IIO 回调（`read_raw` 返回 raw/scale，`write_raw` 设置采样率）
   - probe() 入口和 `of_match_table` 匹配表
   - `module_spi_driver()` 宏一键注册 module_init + module_exit
4. **两种编译方式**:
   - 内核源码树内编译: 放 .c 到 `drivers/iio/imu/` → 修改 Kconfig/Makefile → `make modules`
   - 独立编译: 用 `obj-m := icm20608.o` + `make -C $(KDIR) M=$(PWD) modules`（开发阶段最常用）
5. **部署**: `adb push icm20608.ko /lib/modules/.../extra/` → `depmod -a` → `modprobe icm20608`
   - `insmod` = 只加载这一个模块
   - `modprobe` = 自动解析并加载依赖模块
6. **验证**: `lsmod | grep icm20608` → 确认 IIO 设备号 → `cat /sys/bus/iio/devices/iio:deviceN/in_accel_x_raw`

#### 5. 设备树的独立更新（不需要重新烧录镜像）

- 设备树是独立文件，不和内核、文件系统绑死
- SD/eMMC 分区: MLO + zImage(内核) + xxx.dtb(设备树) + rootfs(根文件系统) — 各自独立
- 启动流程: uboot → 加载 zImage → 加载 dtb → 把 dtb 地址传给内核 → 内核解析 dtb → probe 驱动
- **dtb 和内核是独立加载的**
- 更新设备树只需: 改 `.dts` → `make dtbs` → `adb push xxx.dtb /boot/` → `reboot`（几秒钟）
- 不需要拔 SD 卡，不需要重烧整个镜像

| 操作 | 需要重烧? | 需要重启? | 耗时 |
|------|----------|----------|------|
| 只改设备树(.dtb) | ❌ push dtb 即可 | ✅ | ~10s |
| 只加 .ko 驱动 | ❌ push .ko 即可 | ❌ rmmod+insmod | ~5s |
| 改内核配置(zImage) | ❌ 只换 zImage | ✅ | ~1min |
| 改 uboot | ⚠️ 重写 uboot 分区 | ✅ | — |
| 改文件系统内容 | ❌ 直接 push | ❌ | ~1s |

- 当前板子系统已就绪，有 adb shell 可以操作，改什么推什么即可
- 参考: 内核源码 `arch/arm/boot/dts/imx6ull-14x14-evk.dts`, `arch/arm/boot/dts/imx6ull.dtsi`

#### 8. 物理值计算原理与工业实践

- 核心公式: **物理值 = raw × scale**，这是所有 MEMS 传感器标准做法
- 芯片输出的是 ADC 计数值 (无量纲)，scale 由量程决定
- `scale = (量程_g × 9.80665) / 32768` (16-bit signed, 半量程=2^15)
- ICM-20608 加速度: ±2g→0.000598, ±4g→0.001196, ±8g→0.002392, ±16g→0.004784
- 工业上加三层: 低通滤波(去噪) + 温度补偿 + 零偏校准 + 坐标系旋转
- 不硬编码 scale 的原因: 量程可以动态切换，从 sysfs 读永远正确
- 参考: `src/hal/imu.c` `imu_read()` 函数

#### 9. sysfs 全貌 — 不止 9 个值

- IIO 设备下有 30+ 个 sysfs 属性文件
- 核心 9 个: in_accel_{x,y,z}_raw, in_anglvel_{x,y,z}_raw, in_accel_scale, in_anglvel_scale, sampling_frequency
- 未用到: in_temp_raw/scale (芯片温度), in_*_offset (零偏), buffer/* (buffer模式), trigger/* (触发器), power/* (电源)
- 参考: `src/hal/imu.c` — 阶段 1 只封装核心 9 个

#### 10. sysfs vs IIO — 两者不是一回事

- **IIO** = 内核子系统，专门管传感器和 ADC/DAC (同类: Input子系统、ALSA、V4L2)
- **sysfs** = 通用文件系统，IIO 用它暴露数据给用户空间 (其他子系统也在用: /sys/class/net/, /sys/class/power_supply/)
- 交互链路: 用户空间 → sysfs(VFS层) → IIO框架(read_raw回调) → 驱动(SPI操作寄存器) → 硬件
- sysfs 是"文件"，但不是真文件 — 每次 open 触发内核回调实时生成数据
- 用文件接口的好处: 任何语言能用、不用链接库、权限控制简单、cat/echo 就能调试

#### 11. Linux 设备三大类型 + 多种用户空间接口

**内核设备三大类型:**

| | 块设备 | 字符设备 | 网络设备 |
|---|---|---|---|
| /dev 节点 | 有 (b) | 有 (c) | **没有!** |
| API | lseek+read/write | read/write | socket+send/recv |
| 例子 | SSD, SD卡 | 键盘, IMU, 串口 | eth0, can0 |

**两个独立维度必须分开:**
- 维度一: 内核设备类型 (内核怎么写驱动) — block/char/netif
- 维度二: 用户空间接口 (你怎么访问) — /dev/\*, /sys/\*(sysfs), /proc/\*(procfs), socket, ioctl, mmap, netlink

**sysfs 本质**: 文件系统 (ramfs)，不是设备类型。背靠 Kobject 机制——内核里每个设备/驱动/总线都是一个 kobject，sysfs 把 kobject 树暴露成目录结构。

**同一个设备可同时出现在多个接口:**
- /dev/iio:device1 → open/read (当设备)
- /sys/bus/iio/devices/iio:device1/ → fopen/fscanf (当文件)
- /sys/class/net/can0/ → 查看 can0 信息 (但收发数据必须走 socket)

**项目 5 个外设的全路径:**
- IMU: 字符设备, /dev/iio:device1, sysfs (阶段1) → /dev read() (阶段2)
- CAN: 网络设备, **无 /dev 节点**, socket(AF_CAN)
- Button: 字符设备, /dev/input/event1, read(struct input_event)
- Audio: 字符设备, /dev/snd/pcm*, ioctl/mmap (ALSA 库)
- LED: sysfs (/sys/class/gpio/gpio133/), fopen/fprintf

- 参考: `docs/需求.md` §2.1 硬件资源清单, 后续见对应 HAL 模块源码

---

### Q&A

#### Q1: .ko 文件驱动是什么样子的？

**问**: IMU 的 .ko 驱动是什么样？拿到一块新的板卡没有外设，需要添加一个 IMU 外设的完整流程？

**答**: 见上方「知识点 4: Linux IMU 驱动全流程」和「知识点 5: 设备树的独立更新」。

驱动源码的核心骨架:
- 寄存器映射（查芯片手册）
- 底层 SPI 读写函数（`spi_write_then_read`）
- 芯片初始化（`WHO_AM_I` 校验 → 唤醒 → 配置）
- IIO 回调函数（`read_raw`, `write_raw`）
- `probe()` 入口（匹配成功后分配内存、注册 IIO 设备）
- `of_match_table`（和设备树的 `compatible` 一致）
- `module_spi_driver()` 宏注册驱动

#### Q2: 设备树在哪里修改？修改后需要重新烧录镜像吗？

**答**: 见上方「知识点 5: 设备树的独立更新」。

- 设备树源码: `arch/arm/boot/dts/imx6ull-14x14-evk.dts`
- 编译: `make imx6ull-14x14-evk.dtb`（只编设备树，几秒）
- 部署: `adb push xxx.dtb /boot/` → `reboot`
- **不需要重新烧录镜像**，dtb 是独立文件

#### Q3: 物理值 = raw × scale 合理吗？工业上也是这样做吗？

**答**: 合理，这就是标准做法。所有 MEMS 传感器输出的都是 ADC 计数值，必须乘 scale 才能得物理量。

- 芯片内部: 物理量 → MEMS 变形 → 电容变化 → ADC → 16-bit raw
- scale 由量程决定: `scale = (量程_g × 9.80665) / 32768`
- ICM-20608 默认 ±2g → scale=0.000598; ±4g → 0.001196; ±8g → 0.002392; ±16g → 0.004784
- scale 从 sysfs 动态读取而不是硬编码: 驱动知道当前量程，换量程时 scale 自动变
- 工业上核心公式一样，多加了三层: 低通滤波(去噪) + 温度补偿(scale 漂移) + 零偏校准(静止归零) + 坐标系旋转(芯片朝向修正)
- 参考: `src/hal/imu.c` 第 3 部分 `imu_read()` 实现

#### Q4: sysfs 只有 9 个值吗？

**答**: 不止，总共约 30+ 个属性文件，9 个核心的是你目前用到的。

- 在用 (9): in_accel_{x,y,z}_raw, in_anglvel_{x,y,z}_raw, in_accel_scale, in_anglvel_scale, sampling_frequency
- 未用但存在: in_temp_raw/scale (芯片温度), in_accel_offset_{x,y,z}/in_anglvel_offset_{x,y,z} (零偏校准), name (芯片名), buffer/* (buffer模式), trigger/* (触发器), power/* (电源管理)
- 参考: `src/hal/imu.c` — 阶段 1 只封装了核心 9 个

#### Q5: sysfs 和 IIO 到底是什么关系？

**答**: IIO 是内核里管传感器的"部门"，sysfs 是这个部门的"对外开放窗口"。

```
用户空间 fopen/read → sysfs (VFS 文件系统层) → IIO 框架 (传感器抽象层)
  → IIO 回调 read_raw() → 驱动 (SPI 操作寄存器) → 硬件 (ICM-20608)
```

- sysfs 是通用机制: 所有内核子系统都在用 (/sys/class/net/eth0/, /sys/class/power_supply/BAT0/ 等)
- IIO 用 sysfs 暴露数据是因为: 任何语言都能读 (POSIX 标准)、不用链接库、权限控制简单、cat/echo 就能调试
- sysfs 不是"真文件"——每次 fopen 触发内核回调，实时生成数据
- 参考: 上次「知识点 4: 驱动四层结构」

#### Q6: 数据传输不都走 sysfs 吧？不同外设有不同接口？

**答**: 对。不同外设属于不同内核子系统，对外暴露的接口完全不同。项目里 5 个外设 5 种路:

| 外设 | 子系统 | 暴露接口 | API |
|------|--------|---------|-----|
| IMU | IIO | sysfs (慢) / /dev/iio:deviceN (快) | fopen / read() |
| CAN | SocketCAN | socket (AF_CAN) | socket()/bind()/recv() |
| Button | Input | /dev/input/eventN 字符设备 | open()/epoll/read(struct input_event) |
| Audio | ALSA | /dev/snd/pcm* 字符设备 | snd_pcm_open/ioctl/mmap |
| LED | GPIO | sysfs (/sys/class/gpio/) | fopen/fprintf |

三类接口对比: sysfs (字符串，低频配置) vs Socket (二进制 struct，网络协议) vs 字符设备 (二进制 struct，高频批量)

#### Q7: 块设备、字符设备、网络设备是什么？sysfs 属于哪一种？

**答**: 内核设备分三大类，sysfs 不是设备类型，是文件系统。

**三大内核设备类型:**

| | 块设备 (block) | 字符设备 (char) | 网络设备 (netif) |
|---|---|---|---|
| 数据单位 | 块 (512B+) | 字节流/消息 | 包 (packet) |
| 寻址 | 随机 | 顺序/流式 | 无 |
| 有无/dev节点 | 有 (b) | 有 (c) | **没有!** |
| 内核缓冲 | 有(page cache) | 无 | 自己的一套 |
| 例子 | SSD/HDD/SD卡 | 键盘/串口/IMU | eth0/can0 |
| API | lseek+read/write | read/write | socket+send/recv |

**两个维度必须分开:**

```
维度一: 内核设备类型 (内核源码怎么写驱动)
  → block / char / netif

维度二: 用户空间接口 (你怎么访问它)
  → /dev/* (设备节点)
  → /sys/* (sysfs 属性文件)
  → /proc/* (procfs 进程信息)
  → socket (网络协议栈)
  → ioctl / mmap / netlink
```

**sysfs 的本质:** 它是文件系统 (ramfs)，挂载在 /sys。背后的机制是 Kobject——内核里每个设备/驱动/总线都是一个 kobject，sysfs 把 kobject 树暴露成目录结构。每个设备同时在两个地方出现:

```
/dev/iio:device1     ← devtmpfs 设备节点 (当设备, open/read)
/sys/bus/iio/devices/iio:device1/  ← sysfs 属性 (当文件, fopen/fscanf)
/sys/class/net/can0/                ← sysfs 看网络设备信息 (但收发数据必须走 socket!)
```

**项目里每个外设的全路径:**

| 外设 | 内核类型 | /dev 节点 | sysfs 路径 | 数据走哪 |
|------|---------|-----------|-----------|---------|
| IMU | 字符设备 | /dev/iio:device1 | /sys/bus/iio/devices/iio:device1/ | 阶段1: sysfs; 阶段2: /dev |
| CAN | 网络设备 | **无** | /sys/class/net/can0/ | socket(AF_CAN) |
| Button | 字符设备 | /dev/input/event1 | /sys/class/input/event1/ | read(/dev/input/event1) |
| Audio | 字符设备 | /dev/snd/pcmC0D0p | /sys/class/sound/ | ioctl(/dev/snd/*) |
| LED | 字符设备 | /sys/class/gpio/gpio133/ | fopen/fprintf |

#### 6. IMU 模块实现详解 (阶段 1-1)

- **头文件**: `include/hal/imu.h`
  - `struct imu_data` — 6 轴传感器数据 (accel x/y/z m/s², gyro x/y/z rad/s)
  - `struct imu_dev` — 设备句柄，缓存 dev_name / accel_scale / gyro_scale / initialized
  - 6 个公开函数: init / read / set_sampling_freq / get_sampling_freq / close / print_raw
- **实现文件**: `src/hal/imu.c`
  - 内部辅助: `imu_sysfs_path()` 拼接路径, `imu_read_sysfs_int/float()` 读文件, `imu_write_sysfs_int()` 写文件
  - `imu_init()`: 确定设备名 → 读 in_accel_scale + in_anglvel_scale 验证设备存在 → 缓存 scale
  - `imu_read()`: 检查初始化 → 逐个 fopen/fscanf 6 个 `_raw` 文件 → 原始值 × scale = 物理值
  - `imu_set_sampling_freq()`: fprintf 写入 sampling_frequency 文件 (需 root)
  - `imu_print_raw()`: 格式化打印六轴数据到 stderr
  - 错误处理: 每个文件读失败返回 `CANOPEN_ERR_IO`，不会部分填充 data
- **路径模板**: `/sys/bus/iio/devices/{dev_name}/{attr}`，如 `iio:device1/in_accel_x_raw`
- **新增错误码**: `CANOPEN_ERR_IO = -12` (IO 读写失败)，加入 `include/common/error.h` 和 `src/common/error.c`
- **主程序集成**: `src/app/main.c` — selftest 中 IMU 自检：初始化 → 读取采样频率 → 5 帧数据 (20ms 间隔) → 打印 → 关闭。PC 环境下 init 失败为预期行为
- **编译**: `Makefile` 添加 HAL_SRCS、HAL_OBJS、`-I$(INC_DIR)/hal`，零警告通过

#### 7. 阶段 1-1 编译验证结果 (2026-07-24)

- PC 编译: ✅ 零警告通过 (`make`)
- PC 自检: ✅ 基础模块正常 + IMU 模块优雅降级 (PC 无 sysfs)
- 板子验证: ⏳ 待交叉编译推板子 (`make push CROSS=arm`)

---

## 阶段 2: 多线程采集框架 + IPC

> 待记录

---

## 阶段 3: 诊断引擎 + LED + 语音联动

> 待记录

---

## 阶段 4: TCP 监控服务器 + 数据记录

> 待记录

---

## 阶段 5: 系统完善 + 文档

> 待记录

---

> 创建日期: 2026-07-24
> 最后更新: 2026-07-24
