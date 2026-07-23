/**
 * @file    log.h
 * @brief   日志系统 — 分级日志宏
 *
 * 五个日志等级 (按严重程度):
 *   FATAL (0)   — 致命错误, 程序即将终止
 *   ERROR (1)   — 错误, 当前操作失败
 *   WARN  (2)   — 警告, 可能有问题但不影响运行
 *   INFO  (3)   — 关键运行信息 (默认级别)
 *   DEBUG (4)   — 调试细节 (编译时可通过 LOG_LEVEL 裁剪)
 *
 * 使用方式:
 *   LOG_INFO("can0", "接口已打开, bitrate=%d", 500000);
 *   输出: [2026-07-23 16:30:45] [INFO ] [can0] 接口已打开, bitrate=500000
 *
 * 编译时控制日志级别:
 *   在 Makefile 中定义 LOG_LEVEL 宏:
 *     CFLAGS += -DLOG_LEVEL=4    # 输出所有日志 (DEBUG)
 *     CFLAGS += -DLOG_LEVEL=0    # 只输出 FATAL
 *   未定义时默认为 INFO(3)
 */

#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <stdio.h>
#include <time.h>

/* ---- 日志等级宏 (供 #if 预处理使用) ---- */
#define LOG_LV_FATAL  0
#define LOG_LV_ERROR  1
#define LOG_LV_WARN   2
#define LOG_LV_INFO   3
#define LOG_LV_DEBUG  4

/* ---- 日志等级枚举 ---- */
typedef enum {
    LOG_LV_FATAL_ENUM = LOG_LV_FATAL,
    LOG_LV_ERROR_ENUM = LOG_LV_ERROR,
    LOG_LV_WARN_ENUM  = LOG_LV_WARN,
    LOG_LV_INFO_ENUM  = LOG_LV_INFO,
    LOG_LV_DEBUG_ENUM = LOG_LV_DEBUG,
} log_level_t;

/* ---- 默认日志等级 ---- */
#ifndef LOG_LEVEL
#define LOG_LEVEL  LOG_LV_INFO
#endif

/* ---- 日志输出函数 (内部使用, 不直接调用) ---- */
void _log_write(log_level_t level, const char *tag,
                const char *file, int line,
                const char *fmt, ...);

/* ---- 日志宏 (对外接口) ---- */

#define LOG_FATAL(tag, fmt, ...) \
    _log_write(LOG_LV_FATAL, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(tag, fmt, ...) \
    _log_write(LOG_LV_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(tag, fmt, ...) \
    _log_write(LOG_LV_WARN,  tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(tag, fmt, ...) \
    _log_write(LOG_LV_INFO,  tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* DEBUG 日志仅在 LOG_LEVEL >= 4 时编译 */
#if LOG_LEVEL >= LOG_LV_DEBUG
#define LOG_DEBUG(tag, fmt, ...) \
    _log_write(LOG_LV_DEBUG, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(tag, fmt, ...)  ((void)0)
#endif

#endif /* COMMON_LOG_H */
