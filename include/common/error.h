/**
 * @file    error.h
 * @brief   统一错误码体系
 *
 * 所有模块的函数返回值统一使用 canopen_error_t 枚举。
 * 规则:
 *   - 0                 = 成功
 *   - 正值 (1..99)       = 警告 (操作完成但有异常情况)
 *   - 负值 (-1..-99)     = 通用错误
 *   - 负值 (-100..-199)  = NMT 模块错误
 *   - 负值 (-200..-299)  = SDO 模块错误
 *   - 负值 (-300..-399)  = PDO 模块错误
 *   - 负值 (-400..-499)  = EMCY 模块错误
 *   - 负值 (-500..-599)  = CAN 硬件层错误
 */

#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

/* ================================================================
 * 错误码枚举
 * ================================================================ */
typedef enum {
    /* ---- 成功 ---- */
    CANOPEN_OK                    =  0,   /* 操作成功                 */

    /* ---- 警告 (1..99) ---- */
    CANOPEN_WARN_TIMEOUT          =  1,   /* 操作超时但可恢复         */
    CANOPEN_WARN_RETRY            =  2,   /* 操作重试后成功           */

    /* ---- 通用错误 (-1..-99) ---- */
    CANOPEN_ERR_UNKNOWN           = -1,   /* 未知错误                 */
    CANOPEN_ERR_INVALID_PARAM     = -2,   /* 无效参数 (空指针/范围外) */
    CANOPEN_ERR_NOT_IMPLEMENTED   = -3,   /* 功能尚未实现             */
    CANOPEN_ERR_NO_MEMORY         = -4,   /* 内存不足                 */
    CANOPEN_ERR_BUFFER_FULL       = -5,   /* 缓冲区已满               */
    CANOPEN_ERR_NOT_FOUND         = -6,   /* 查找的条目不存在         */
    CANOPEN_ERR_NOT_INITIALIZED   = -7,   /* 模块尚未初始化           */
    CANOPEN_ERR_ALREADY_EXISTS    = -8,   /* 条目已存在               */
    CANOPEN_ERR_STATE             = -9,   /* 当前状态不允许此操作     */
    CANOPEN_ERR_PERMISSION        = -10,  /* 权限不足                 */
    CANOPEN_ERR_DATA_SIZE         = -11,  /* 数据大小不匹配           */

    /* ---- NMT 模块错误 (-100..-199) ---- */
    CANOPEN_ERR_NMT_INVALID_CMD   = -100, /* 无效的 NMT 命令          */
    CANOPEN_ERR_NMT_NO_RESPONSE   = -101, /* NMT 命令无响应           */
    CANOPEN_ERR_NMT_NODE_OFFLINE  = -102, /* 目标节点离线             */
    CANOPEN_ERR_NMT_PROTOCOL      = -103, /* NMT 协议错误             */

    /* ---- SDO 模块错误 (-200..-299) ---- */
    CANOPEN_ERR_SDO_TIMEOUT       = -200, /* SDO 传输超时             */
    CANOPEN_ERR_SDO_ABORT         = -201, /* SDO 传输被对方中止       */
    CANOPEN_ERR_SDO_TOGGLE_BIT    = -202, /* SDO 翻转位错误           */
    CANOPEN_ERR_SDO_DATA_SIZE     = -203, /* SDO 数据大小不支持       */

    /* ---- PDO 模块错误 (-300..-399) ---- */
    CANOPEN_ERR_PDO_MAPPING       = -300, /* PDO 映射配置错误         */
    CANOPEN_ERR_PDO_TIMEOUT       = -301, /* PDO 接收超时             */
    CANOPEN_ERR_PDO_LENGTH        = -302, /* PDO 数据长度不匹配       */

    /* ---- EMCY 模块错误 (-400..-499) ---- */
    CANOPEN_ERR_EMCY_PARSE        = -400, /* EMCY 报文解析失败        */

    /* ---- CAN 硬件层错误 (-500..-599) ---- */
    CANOPEN_ERR_CAN_SEND          = -500, /* CAN 帧发送失败           */
    CANOPEN_ERR_CAN_RECV          = -501, /* CAN 帧接收失败           */
    CANOPEN_ERR_CAN_OPEN          = -502, /* CAN 接口打开失败         */
    CANOPEN_ERR_CAN_SETUP         = -503, /* CAN 接口配置失败         */
} canopen_error_t;

/* ================================================================
 * 错误码转字符串 (调试用)
 * ================================================================ */

/**
 * @brief  将错误码转换为可读的字符串描述
 * @param  err  错误码
 * @return 指向静态字符串的指针 (只读, 不要 free)
 */
const char *canopen_strerror(canopen_error_t err);

#endif /* COMMON_ERROR_H */
