/**
 * @file    log.c
 * @brief   日志系统实现 — 带时间戳的结构化日志输出
 *
 * 输出格式:
 *   [YYYY-MM-DD HH:MM:SS] [LEVEL] [TAG] 消息内容
 *
 * 示例:
 *   [2026-07-23 16:30:45] [INFO ] [CANopen] 主站初始化完成
 *   [2026-07-23 16:30:46] [ERROR] [NMT   ] 节点2 心跳超时
 */

#include "common/log.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* 日志等级对应的显示字符串 */
static const char *level_names[] = {
    [LOG_LV_FATAL_ENUM] = "FATAL",
    [LOG_LV_ERROR_ENUM] = "ERROR",
    [LOG_LV_WARN_ENUM]  = "WARN ",
    [LOG_LV_INFO_ENUM]  = "INFO ",
    [LOG_LV_DEBUG_ENUM] = "DEBUG",
};

/** 时间缓冲区大小: "YYYY-MM-DD HH:MM:SS" = 19 + '\0' */
#define TIME_BUF_SIZE  20

/**
 * @brief  获取当前时间的格式化字符串
 * @param  buf  输出缓冲区, 至少 TIME_BUF_SIZE 字节
 * @param  size 缓冲区大小
 */
static void get_time_str(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_info;

    if (buf && size >= TIME_BUF_SIZE) {
        localtime_r(&now, &tm_info);
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_info);
    }
}

/**
 * @brief  核心日志输出函数
 *
 * 每个日志宏 (LOG_INFO 等) 最终都会调用此函数。
 * 线程安全: 目前使用单 fprintf 输出。如果未来多线程环境需要加锁，
 *           在这里添加 pthread_mutex_lock 即可。
 *
 * @param level  日志等级
 * @param tag    模块标签 (如 "CANopen", "NMT", "SDO")
 * @param file   源码文件名 (由宏 __FILE__ 传入)
 * @param line   源码行号 (由宏 __LINE__ 传入)
 * @param fmt    printf 格式字符串
 * @param ...    可变参数
 */
void _log_write(log_level_t level, const char *tag,
                const char *file, int line,
                const char *fmt, ...)
{
    /* 等级过滤: 低于当前等级的日志不输出 */
    if (level > LOG_LEVEL) {
        return;
    }

    char time_str[TIME_BUF_SIZE];
    get_time_str(time_str, sizeof(time_str));

    /* 从文件名中提取基本名称 (去掉路径前缀) */
    const char *basename = strrchr(file, '/');
    basename = basename ? basename + 1 : file;
    /* Windows 路径用反斜杠 */
    const char *bs = strrchr(basename, '\\');
    basename = bs ? bs + 1 : basename;

    /* 第一行: 时间 + 等级 + 标签 + 消息 */
    fprintf(stderr, "[%s] [%s] [%-6s] ",
            time_str, level_names[level], tag ? tag : "");

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    /* 第二行: 源码位置 (仅 DEBUG 和 ERROR 输出) */
    if (level <= LOG_LV_ERROR) {
        fprintf(stderr, "  (%s:%d)", basename, line);
    }

    fprintf(stderr, "\n");
    fflush(stderr);
}
