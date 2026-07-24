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
