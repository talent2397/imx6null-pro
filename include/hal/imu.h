/**
 * @file    imu.h
 * @brief   IMU 模块接口 — ICM-20608 6轴传感器驱动封装
 *
 * 通过 IIO 子系统的 sysfs 接口读取加速度和角速度数据。
 * 阶段 1 使用 sysfs 慢速读取方式（open/read/close 每个属性文件），
 * 阶段 2 将升级为 IIO buffer 高速模式以支撑 100Hz+ 采样。
 *
 * 使用示例:
 * @code
 *   struct imu_dev imu;
 *   struct imu_data data;
 *
 *   imu_init(&imu, "iio:device1");
 *   imu_set_sampling_freq(&imu, 100);
 *   imu_read(&imu, &data);
 *   imu_print_raw(&data);
 *   imu_close(&imu);
 * @endcode
 *
 * 硬件: ICM-20608-G (SPI), 挂载在 /dev/iio:device1
 * 参考: ICM-20608 数据手册, Linux IIO 子系统文档
 */

#ifndef HAL_IMU_H
#define HAL_IMU_H

#include "common/types.h"
#include "common/error.h"

/* ================================================================
 * IMU 默认 IIO 设备名称 (板子上实测为 iio:device1)
 * ================================================================ */
#define IMU_DEFAULT_DEV_NAME    "iio:device1"

/**
 * @brief IMU 六轴传感器数据
 *
 * 物理量单位:
 *   - accel_* : m/s² (加速度)
 *   - gyro_*  : rad/s (角速度)
 *
 * 原始值 → 物理值转换:
 *   物理值 = raw_value × scale_factor
 *   如: accel_x = 2048 × 0.000598 = 1.225 m/s²
 */
struct imu_data {
    f32 accel_x;    /**< X 轴加速度 (m/s²) */
    f32 accel_y;    /**< Y 轴加速度 (m/s²) */
    f32 accel_z;    /**< Z 轴加速度 (m/s²) */
    f32 gyro_x;     /**< X 轴角速度 (rad/s)   */
    f32 gyro_y;     /**< Y 轴角速度 (rad/s)   */
    f32 gyro_z;     /**< Z 轴角速度 (rad/s)   */
};

/**
 * @brief IMU 设备句柄
 *
 * 保存 IIO 设备信息及缓存的比例因子，sysfs 方式不需要持有文件描述符。
 * 每次 imu_read() 临时打开属性文件读取后马上关闭。
 */
struct imu_dev {
    char dev_name[64];      /**< IIO 设备名，如 "iio:device1" */
    f32  accel_scale;       /**< 加速度比例因子 (从 in_accel_scale 读取) */
    f32  gyro_scale;        /**< 角速度比例因子 (从 in_anglvel_scale 读取) */
    bool initialized;       /**< 初始化成功标志 */
};

/* ================================================================
 * 公开函数
 * ================================================================ */

/**
 * @brief  初始化 IMU 设备
 *
 * 读取并缓存 scale 比例因子，验证 sysfs 路径可访问。
 * 此函数不改变芯片配置，仅确认设备存在且可读。
 *
 * @param  dev      设备句柄指针
 * @param  iio_name IIO 设备名，如 "iio:device1"，传 NULL 则用默认值
 * @return CANOPEN_OK 成功，否则为 CANOPEN_ERR_* 错误码
 */
canopen_error_t imu_init(struct imu_dev *dev, const char *iio_name);

/**
 * @brief  读取一帧 IMU 数据 (6 轴)
 *
 * 依次打开 6 个 _raw sysfs 属性文件，读取原始值后乘以 scale 得到物理值。
 * 每次调用 open → fscanf → close 6 个文件，阶段 1 采样率 < 50Hz 时够用。
 *
 * @param  dev   已初始化的设备句柄
 * @param  data  输出：传感器数据
 * @return CANOPEN_OK 成功，CANOPEN_ERR_NOT_INITIALIZED 如果未初始化
 */
canopen_error_t imu_read(struct imu_dev *dev, struct imu_data *data);

/**
 * @brief  设置 IMU 采样频率
 *
 * 往 IIO sysfs 的 sampling_frequency 属性写入目标频率值。
 * 并非所有频率都被硬件支持，写入后建议调用 imu_get_sampling_freq 确认实际值。
 *
 * @param  dev      已初始化的设备句柄
 * @param  freq_hz  目标采样频率 (Hz)，如 50, 100, 200
 * @return CANOPEN_OK 成功
 */
canopen_error_t imu_set_sampling_freq(struct imu_dev *dev, u32 freq_hz);

/**
 * @brief  获取当前 IMU 采样频率
 *
 * 从 IIO sysfs 的 sampling_frequency 属性读取当前实际采样频率。
 *
 * @param  dev      已初始化的设备句柄
 * @param  freq_hz  输出：当前采样频率 (Hz)
 * @return CANOPEN_OK 成功
 */
canopen_error_t imu_get_sampling_freq(struct imu_dev *dev, u32 *freq_hz);

/**
 * @brief  关闭 IMU 设备，释放资源
 *
 * sysfs 模式下不需要特殊的硬件操作，主要是重置状态标志。
 *
 * @param  dev  设备句柄指针
 * @return CANOPEN_OK
 */
canopen_error_t imu_close(struct imu_dev *dev);

/**
 * @brief  格式化打印 IMU 六轴数据到 stderr (调试用)
 *
 * 输出格式:
 *   [IMU] ACCEL  X:+1.234  Y:-0.056  Z:+9.810  |  GYRO  X:+0.012  Y:-0.003  Z:+0.001
 *
 * @param  data  传感器数据指针
 */
void imu_print_raw(const struct imu_data *data);

#endif /* HAL_IMU_H */
