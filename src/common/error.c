/**
 * @file    error.c
 * @brief   错误码枚举 → 可读字符串的转换表
 *
 * 维护原则: 每个 canopen_error_t 枚举值都必须有对应的字符串条目。
 * 新增错误码时, 在此文件中同步添加。
 */

#include "common/types.h"
#include "common/error.h"

/* ---- 错误码 → 字符串映射表 ---- */
typedef struct {
    canopen_error_t  code;
    const char       *desc;
} error_desc_t;

static const error_desc_t error_table[] = {
    /* 成功 */
    { CANOPEN_OK,                   "操作成功" },

    /* 警告 */
    { CANOPEN_WARN_TIMEOUT,         "操作超时(可恢复)" },
    { CANOPEN_WARN_RETRY,           "操作重试后成功" },

    /* 通用错误 */
    { CANOPEN_ERR_UNKNOWN,          "未知错误" },
    { CANOPEN_ERR_INVALID_PARAM,    "无效参数" },
    { CANOPEN_ERR_NOT_IMPLEMENTED,  "功能未实现" },
    { CANOPEN_ERR_NO_MEMORY,        "内存不足" },
    { CANOPEN_ERR_BUFFER_FULL,      "缓冲区已满" },
    { CANOPEN_ERR_NOT_FOUND,        "未找到" },
    { CANOPEN_ERR_NOT_INITIALIZED,  "模块未初始化" },
    { CANOPEN_ERR_ALREADY_EXISTS,   "已存在" },
    { CANOPEN_ERR_STATE,            "状态不允许" },
    { CANOPEN_ERR_PERMISSION,       "权限不足" },
    { CANOPEN_ERR_DATA_SIZE,        "数据大小不匹配" },

    /* NMT 模块 */
    { CANOPEN_ERR_NMT_INVALID_CMD,  "无效NMT命令" },
    { CANOPEN_ERR_NMT_NO_RESPONSE,  "NMT命令无响应" },
    { CANOPEN_ERR_NMT_NODE_OFFLINE, "目标节点离线" },
    { CANOPEN_ERR_NMT_PROTOCOL,     "NMT协议错误" },

    /* SDO 模块 */
    { CANOPEN_ERR_SDO_TIMEOUT,      "SDO传输超时" },
    { CANOPEN_ERR_SDO_ABORT,        "SDO传输中止" },
    { CANOPEN_ERR_SDO_TOGGLE_BIT,   "SDO翻转位错误" },
    { CANOPEN_ERR_SDO_DATA_SIZE,    "SDO数据大小不支持" },

    /* PDO 模块 */
    { CANOPEN_ERR_PDO_MAPPING,      "PDO映射错误" },
    { CANOPEN_ERR_PDO_TIMEOUT,      "PDO接收超时" },
    { CANOPEN_ERR_PDO_LENGTH,       "PDO数据长度不匹配" },

    /* EMCY 模块 */
    { CANOPEN_ERR_EMCY_PARSE,       "EMCY报文解析失败" },

    /* CAN 硬件层 */
    { CANOPEN_ERR_CAN_SEND,         "CAN发送失败" },
    { CANOPEN_ERR_CAN_RECV,         "CAN接收失败" },
    { CANOPEN_ERR_CAN_OPEN,         "CAN接口打开失败" },
    { CANOPEN_ERR_CAN_SETUP,        "CAN接口配置失败" },
};

/**
 * @brief  将错误码转为可读字符串
 *
 * 如果错误码未找到 (理论上不应该发生), 返回 "未知错误码"。
 *
 * @param  err  错误码
 * @return 静态字符串指针 (不需要 free)
 */
const char *canopen_strerror(canopen_error_t err)
{
    for (size_t i = 0; i < ARRAY_SIZE(error_table); i++) {
        if (error_table[i].code == err) {
            return error_table[i].desc;
        }
    }
    return "未知错误码";
}
