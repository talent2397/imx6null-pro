/**
 * @file    imu.c
 * @brief   IMU 模块实现 — 通过 IIO sysfs 接口读取 ICM-20608 六轴数据
 *
 * 实现原理:
 *   1. 构造 sysfs 属性文件路径 (如 /sys/bus/iio/devices/iio:device1/in_accel_x_raw)
 *   2. fopen → fscanf → fclose 读取 _raw 原始值
 *   3. 原始值 × scale 比例因子 = 物理值 (m/s² 或 rad/s)
 *
 * 限制:
 *   - 每次 imu_read() 需要 open/read/close 6 个文件，单次耗时 ~1ms+
 *   - 适合 < 50Hz 的按需采样，不适合 100Hz+ 连续高速采集
 *   - 阶段 2 将升级为 IIO buffer + trigger 模式
 *
 * sysfs 属性文件清单 (ICM-20608):
 *   in_accel_x_raw      — 加速度 X 轴原始值 (带符号整数)
 *   in_accel_y_raw      — 加速度 Y 轴原始值
 *   in_accel_z_raw      — 加速度 Z 轴原始值
 *   in_anglvel_x_raw    — 角速度 X 轴原始值 (注意: 是 anglvel 不是 gyro)
 *   in_anglvel_y_raw    — 角速度 Y 轴原始值
 *   in_anglvel_z_raw    — 角速度 Z 轴原始值
 *   in_accel_scale      — 加速度比例因子 (浮点，如 0.000598)
 *   in_anglvel_scale    — 角速度比例因子 (浮点)
 *   sampling_frequency  — 当前采样频率 (整数，Hz)
 */

#include "hal/imu.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ================================================================
 * 路径构造
 * ================================================================ */

/** sysfs 设备基路径模板 */
#define SYSFS_IIO_BASE     "/sys/bus/iio/devices"

/**
 * @brief  构造 IIO 属性文件的完整路径
 *
 * 拼接规则: /sys/bus/iio/devices/<dev_name>/<attr>
 * 如: imu_sysfs_path(buf, size, "iio:device1", "in_accel_x_raw")
 *     → "/sys/bus/iio/devices/iio:device1/in_accel_x_raw"
 *
 * @param  buf      输出缓冲区
 * @param  size     缓冲区大小
 * @param  dev_name IIO 设备名
 * @param  attr     属性文件名
 * @return buf 指针 (方便链式调用)
 */
static char *imu_sysfs_path(char *buf, size_t size,
                             const char *dev_name, const char *attr)
{
    snprintf(buf, size, "%s/%s/%s", SYSFS_IIO_BASE, dev_name, attr);
    return buf;
}

/* ================================================================
 * sysfs 原子读写 (内部辅助函数)
 * ================================================================ */

/**
 * @brief  从 sysfs 属性文件读取一个整数值
 *
 * 用于读取 _raw 值和 sampling_frequency。
 * 文件内容如 "256\n" 或 "-12\n"。
 *
 * @param  path  sysfs 属性文件完整路径
 * @param  val   输出: 解析出的整数值
 * @return 0 成功, -1 失败 (文件不存在/权限不足/格式错误)
 */
static int imu_read_sysfs_int(const char *path, int *val)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    int ret = fscanf(fp, "%d", val);
    fclose(fp);

    /* fscanf 成功匹配 1 个整数才有效 */
    return (ret == 1) ? 0 : -1;
}

/**
 * @brief  从 sysfs 属性文件读取一个浮点值
 *
 * 用于读取 scale 比例因子。
 * 文件内容如 "0.000598" 或 "5.980000e-04"。
 *
 * @param  path  sysfs 属性文件完整路径
 * @param  val   输出: 解析出的浮点值
 * @return 0 成功, -1 失败
 */
static int imu_read_sysfs_float(const char *path, f32 *val)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    /*
     * 注意: float 的 scanf 格式化用 %f
     * 科学计数法 (如 5.98e-04) 也会被正确解析
     */
    int ret = fscanf(fp, "%f", val);
    fclose(fp);

    return (ret == 1) ? 0 : -1;
}

/**
 * @brief  向 sysfs 属性文件写入一个整数值
 *
 * 用于设置 sampling_frequency。
 *
 * @param  path  sysfs 属性文件完整路径
 * @param  val   待写入的整数值
 * @return 0 成功, -1 失败 (权限不足/数值不支持)
 */
static int imu_write_sysfs_int(const char *path, int val)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    int ret = fprintf(fp, "%d\n", val);
    fclose(fp);

    return (ret > 0) ? 0 : -1;
}

/* ================================================================
 * 公开函数实现
 * ================================================================ */

/**
 * @brief  初始化 IMU 设备
 *
 * 执行流程:
 *   1. 确定 IIO 设备名 (传 NULL 则用默认 IMU_DEFAULT_DEV_NAME)
 *   2. 尝试读取 in_accel_scale 和 in_anglvel_scale
 *   3. 如果 scale 文件读不到 → 设备可能不存在或路径错误 → 返回错误码
 *   4. 缓存 scale 值, 设置 initialized = true
 *
 * 不会修改芯片寄存器，不会改变采样频率。
 */
canopen_error_t imu_init(struct imu_dev *dev, const char *iio_name)
{
    /* ---- 参数校验 ---- */
    if (!dev) {
        return CANOPEN_ERR_INVALID_PARAM;
    }

    /* ---- 清零结构体，避免残留值干扰 ---- */
    memset(dev, 0, sizeof(*dev));

    /* ---- 确定 IIO 设备名 ---- */
    if (iio_name && iio_name[0] != '\0') {
        strncpy(dev->dev_name, iio_name, sizeof(dev->dev_name) - 1);
    } else {
        strncpy(dev->dev_name, IMU_DEFAULT_DEV_NAME, sizeof(dev->dev_name) - 1);
    }
    dev->dev_name[sizeof(dev->dev_name) - 1] = '\0';  /* 保底截断 */

    char path[256];

    /* ---- 读取加速度 scale 比例因子 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_accel_scale");
    if (imu_read_sysfs_float(path, &dev->accel_scale) != 0) {
        LOG_ERROR("IMU", "无法读取加速度 scale: %s (设备可能不存在或权限不足)", path);
        return CANOPEN_ERR_NOT_FOUND;
    }

    /* ---- 读取角速度 scale 比例因子 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_anglvel_scale");
    if (imu_read_sysfs_float(path, &dev->gyro_scale) != 0) {
        LOG_ERROR("IMU", "无法读取角速度 scale: %s", path);
        return CANOPEN_ERR_NOT_FOUND;
    }

    /* ---- 标记初始化成功 ---- */
    dev->initialized = true;

    LOG_INFO("IMU", "设备初始化完成: %s (accel_scale=%.6f, gyro_scale=%.6f)",
             dev->dev_name, (double)dev->accel_scale, (double)dev->gyro_scale);

    return CANOPEN_OK;
}

/**
 * @brief  读取一帧完整的六轴 IMU 数据
 *
 * 执行流程:
 *   1. 检查 dev 是否已初始化
 *   2. 依次读取 6 个 _raw 属性文件 (accel X/Y/Z, gyro X/Y/Z)
 *   3. 每个原始值 × 对应 scale → 物理量填入 imu_data
 *
 * 错误处理: 任意一个文件读取失败就返回错误，
 * 不会对 data 做部分填充（避免调用方误用半脏数据）。
 */
canopen_error_t imu_read(struct imu_dev *dev, struct imu_data *data)
{
    if (!dev || !data) {
        return CANOPEN_ERR_INVALID_PARAM;
    }

    if (!dev->initialized) {
        LOG_ERROR("IMU", "设备未初始化, 无法读取数据");
        return CANOPEN_ERR_NOT_INITIALIZED;
    }

    char path[256];
    int  raw_val;

    /* ---- 加速度 X 轴 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_accel_x_raw");
    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取 accel_x 失败: %s", path);
        return CANOPEN_ERR_IO;
    }
    data->accel_x = (f32)raw_val * dev->accel_scale;

    /* ---- 加速度 Y 轴 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_accel_y_raw");
    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取 accel_y 失败: %s", path);
        return CANOPEN_ERR_IO;
    }
    data->accel_y = (f32)raw_val * dev->accel_scale;

    /* ---- 加速度 Z 轴 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_accel_z_raw");
    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取 accel_z 失败: %s", path);
        return CANOPEN_ERR_IO;
    }
    data->accel_z = (f32)raw_val * dev->accel_scale;

    /* ---- 角速度 X 轴 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_anglvel_x_raw");
    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取 gyro_x 失败: %s", path);
        return CANOPEN_ERR_IO;
    }
    data->gyro_x = (f32)raw_val * dev->gyro_scale;

    /* ---- 角速度 Y 轴 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_anglvel_y_raw");
    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取 gyro_y 失败: %s", path);
        return CANOPEN_ERR_IO;
    }
    data->gyro_y = (f32)raw_val * dev->gyro_scale;

    /* ---- 角速度 Z 轴 ---- */
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "in_anglvel_z_raw");
    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取 gyro_z 失败: %s", path);
        return CANOPEN_ERR_IO;
    }
    data->gyro_z = (f32)raw_val * dev->gyro_scale;

    return CANOPEN_OK;
}

/**
 * @brief  设置采样频率
 *
 * 写入 sampling_frequency 文件，内核驱动会尝试将芯片配置到最接近的
 * 支持频率。写入后建议用 imu_get_sampling_freq() 确认实际值。
 *
 * 注意: 需要 root 权限才能写 sysfs 文件。
 */
canopen_error_t imu_set_sampling_freq(struct imu_dev *dev, u32 freq_hz)
{
    if (!dev) {
        return CANOPEN_ERR_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return CANOPEN_ERR_NOT_INITIALIZED;
    }

    if (freq_hz == 0) {
        return CANOPEN_ERR_INVALID_PARAM;
    }

    char path[256];
    imu_sysfs_path(path, sizeof(path), dev->dev_name, "sampling_frequency");

    if (imu_write_sysfs_int(path, (int)freq_hz) != 0) {
        LOG_ERROR("IMU", "设置采样频率 %u Hz 失败: %s (可能需要 root 权限)",
                  freq_hz, path);
        return CANOPEN_ERR_PERMISSION;
    }

    LOG_INFO("IMU", "采样频率已设置为 %u Hz", freq_hz);
    return CANOPEN_OK;
}

/**
 * @brief  获取当前 IMU 采样频率
 *
 * 从 sampling_frequency sysfs 属性读取实际采样频率。
 */
canopen_error_t imu_get_sampling_freq(struct imu_dev *dev, u32 *freq_hz)
{
    if (!dev || !freq_hz) {
        return CANOPEN_ERR_INVALID_PARAM;
    }

    if (!dev->initialized) {
        return CANOPEN_ERR_NOT_INITIALIZED;
    }

    char path[256];
    int  raw_val;

    imu_sysfs_path(path, sizeof(path), dev->dev_name, "sampling_frequency");

    if (imu_read_sysfs_int(path, &raw_val) != 0) {
        LOG_ERROR("IMU", "读取采样频率失败: %s", path);
        return CANOPEN_ERR_NOT_FOUND;
    }

    *freq_hz = (u32)raw_val;
    return CANOPEN_OK;
}

/**
 * @brief  关闭 IMU 设备
 *
 * sysfs 模式下不需要释放硬件资源（没有 open 的文件描述符，
 * 没有 mmap 的内存），只需清除初始化标志。
 */
canopen_error_t imu_close(struct imu_dev *dev)
{
    if (!dev) {
        return CANOPEN_ERR_INVALID_PARAM;
    }

    dev->initialized = false;
    LOG_INFO("IMU", "设备已关闭");

    return CANOPEN_OK;
}

/**
 * @brief  格式化打印 IMU 六轴数据
 *
 * 输出示例:
 *   [IMU] ACCEL X:+1.234 Y:-0.056 Z:+9.810 | GYRO X:+0.012 Y:-0.003 Z:+0.001
 *
 * 带符号和对齐，方便人眼对比数值。
 */
void imu_print_raw(const struct imu_data *data)
{
    if (!data) {
        return;
    }

    fprintf(stderr,
            "[IMU] "
            "ACCEL  X:%+8.3f  Y:%+8.3f  Z:%+8.3f  |  "
            "GYRO   X:%+8.3f  Y:%+8.3f  Z:%+8.3f\n",
            (double)data->accel_x, (double)data->accel_y, (double)data->accel_z,
            (double)data->gyro_x,  (double)data->gyro_y,  (double)data->gyro_z);
}
