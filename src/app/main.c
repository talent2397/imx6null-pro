/**
 * @file    main.c
 * @brief   机器人关节智能监测网关 — 程序入口
 *
 * 阶段 0: 项目骨架 + 外设实测
 *   - 日志系统正常工作
 *   - 错误码体系正常
 *   - 编译零警告 (x86_64 + ARM)
 *
 * 用法:
 *   ./monitor_gateway                  # 正常运行
 *   ./monitor_gateway --version        # 显示版本
 *   ./monitor_gateway --selftest       # 自检模式
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/types.h"
#include "common/error.h"
#include "common/log.h"

#define PROGRAM_NAME    "monitor_gateway"
#define PROGRAM_VERSION "2.0.0"
#define PROGRAM_AUTHOR  "talent2397"

/* ---- 自检: 验证错误码和日志系统 ---- */
static void run_selftest(void)
{
    LOG_INFO("SELFTEST", "========== 自检开始 ==========");

    /* 测试错误码 → 字符串转换 */
    canopen_error_t test_codes[] = {
        CANOPEN_OK,
        CANOPEN_ERR_INVALID_PARAM,
        CANOPEN_ERR_CAN_OPEN,
        CANOPEN_ERR_SDO_TIMEOUT,
        CANOPEN_ERR_NMT_NODE_OFFLINE,
    };

    for (size_t i = 0; i < ARRAY_SIZE(test_codes); i++) {
        const char *desc = canopen_strerror(test_codes[i]);
        LOG_INFO("SELFTEST", "  错误码 %4d → %s", test_codes[i], desc);
    }

    /* 测试各等级日志 */
    LOG_DEBUG("SELFTEST", "这是一条 DEBUG 日志 (编译时可能被裁掉)");
    LOG_INFO("SELFTEST",  "这是一条 INFO 日志");
    LOG_WARN("SELFTEST",  "这是一条 WARN 日志");
    LOG_ERROR("SELFTEST", "这是一条 ERROR 日志");

    /* 测试类型大小 */
    LOG_INFO("SELFTEST", "类型大小: u8=%zu u16=%zu u32=%zu u64=%zu",
             sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64));

    /* 测试常用宏 */
    u32 test_arr[] = {10, 20, 30, 40, 50};
    LOG_INFO("SELFTEST", "ARRAY_SIZE(test_arr)=%zu (期望=5)", ARRAY_SIZE(test_arr));
    LOG_INFO("SELFTEST", "MIN(3,8)=%d MAX(3,8)=%d CLAMP(15,0,10)=%d",
             MIN(3, 8), MAX(3, 8), CLAMP(15, 0, 10));

    LOG_INFO("SELFTEST", "========== 自检结束 ==========");
}

/* ---- 打印版本信息 ---- */
static void print_version(void)
{
    printf("%s v%s -- Robot Joint Monitoring Gateway\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("Author: %s\n", PROGRAM_AUTHOR);
    printf("Build:  %s %s\n", __DATE__, __TIME__);
    printf("\n");
    printf("项目: 机器人关节智能监测网关\n");
    printf("平台: IMX6ULL-Pro (ARM Cortex-A7) + Linux\n");
    printf("目标: 多传感器采集 + CAN通信 + 故障诊断 + 远程监控\n");
}

/* ---- 打印帮助 ---- */
static void print_help(void)
{
    printf("用法: %s [选项]\n", PROGRAM_NAME);
    printf("\n");
    printf("选项:\n");
    printf("  --help       显示此帮助信息\n");
    printf("  --version    显示版本信息\n");
    printf("  --selftest   运行自检程序\n");
    printf("\n");
    printf("无选项时进入交互模式 (阶段 1.5 实现)\n");
}

int main(int argc, char *argv[])
{
    /* 解析命令行参数 */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[1], "--selftest") == 0) {
            run_selftest();
            return 0;
        }
        LOG_ERROR(PROGRAM_NAME, "未知参数: %s", argv[1]);
        print_help();
        return 1;
    }

    /* 默认: 版本信息 + 提示 */
    print_version();
    printf("\n");
    LOG_INFO(PROGRAM_NAME, "项目骨架初始化完成!");
    LOG_INFO(PROGRAM_NAME, "使用 --selftest 运行自检");
    LOG_INFO(PROGRAM_NAME, "交互模式将在阶段 1.5 实现");
    printf("\n");

    return 0;
}
