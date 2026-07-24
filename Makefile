# ================================================================
# CANopen 关节控制器 — 顶层 Makefile
# ================================================================
# 支持目标平台:
#   make                    # 默认: PC (x86_64) 调试编译
#   make CROSS=arm          # 交叉编译: IMX6ULL (ARM Cortex-A7)
#   make test               # 编译并运行单元测试
#   make push               # 交叉编译并 adb push 到板子
#   make clean              # 清理编译产物
# ================================================================

# ---- 项目信息 ----
PROJECT    := monitor_gateway
VERSION    := 2.0.0

# ---- 目录 ----
SRC_DIR    := src
INC_DIR    := include
BUILD_DIR  := build
TEST_DIR   := test

# ---- 源文件 (手动列出, 便于精确控制) ----
COMMON_SRCS := \
    $(SRC_DIR)/common/log.c \
    $(SRC_DIR)/common/error.c \

APP_SRCS := \
    $(SRC_DIR)/app/main.c \

# 阶段 1: HAL 模块源文件
HAL_SRCS := \
    $(SRC_DIR)/hal/imu.c \

ALL_SRCS := $(COMMON_SRCS) $(APP_SRCS) $(HAL_SRCS)

# ---- 自动生成目标文件列表 ----
COMMON_OBJS   := $(COMMON_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
APP_OBJS      := $(APP_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
HAL_OBJS      := $(HAL_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ALL_OBJS      := $(COMMON_OBJS) $(APP_OBJS) $(HAL_OBJS)

# ---- 工具链配置 ----
# 默认: 本地 GCC (PC 调试)
CC       ?= gcc
# 交叉编译时: make CROSS=arm
ifeq ($(CROSS), arm)
    CC      = arm-linux-gnueabihf-gcc
endif

# ---- 编译选项 ----
CFLAGS   := -std=gnu99
CFLAGS   += -Wall -Wextra -Werror
CFLAGS   += -Wshadow -Wundef -Wunused -Wmissing-prototypes
CFLAGS   += -I$(INC_DIR) -I$(INC_DIR)/hal

# ---- 链接选项 ----
LDFLAGS  := -lpthread -lrt

# ARM 交叉编译时静态链接, 避免板子 glibc 2.30 与主机 glibc 2.34 不匹配
ifeq ($(CROSS), arm)
    LDFLAGS += -static
endif

# 调试/发布模式
ifeq ($(BUILD), release)
    CFLAGS   += -O2 -DNDEBUG
    CFLAGS   += -DLOG_LEVEL=2   # 发布版本只输出 WARN+
else
    CFLAGS   += -O0 -g
    CFLAGS   += -DLOG_LEVEL=4   # 开发阶段输出所有日志 (DEBUG)
endif

# ---- 颜色输出 ----
COLOR_GREEN  := \033[0;32m
COLOR_YELLOW := \033[0;33m
COLOR_RED    := \033[0;31m
COLOR_RESET  := \033[0m

# ================================================================
# 主要目标
# ================================================================

.PHONY: all
all: $(BUILD_DIR)/$(PROJECT)
	@echo "$(COLOR_GREEN)[OK]$(COLOR_RESET) 编译完成: $(BUILD_DIR)/$(PROJECT)"

# ---- 链接 ----
$(BUILD_DIR)/$(PROJECT): $(ALL_OBJS)
	@echo "  LD    $@"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ---- 编译 C 文件 ----
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "  CC    $<"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $<

# ---- 生成依赖文件 (.d) ----
$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< > $@

# 自动包含依赖 (如果存在)
DEPS := $(ALL_OBJS:.o=.d)
-include $(DEPS)

# ================================================================
# 辅助目标
# ================================================================

.PHONY: clean
clean:
	@echo "清理编译产物..."
	@rm -rf $(BUILD_DIR)
	@echo "$(COLOR_GREEN)[OK]$(COLOR_RESET) 清理完成"

.PHONY: test
test: all
	@echo ""
	@echo "$(COLOR_YELLOW)========== 运行自检 ==========$(COLOR_RESET)"
	@$(BUILD_DIR)/$(PROJECT) --selftest
	@echo ""
	@echo "$(COLOR_YELLOW)========== 单元测试 ==========$(COLOR_RESET)"
	@echo "(阶段 1.3 实现 — cmocka 测试框架)"
	@echo ""

# 交叉编译 + 推送到板子 (仅当 CROSS=arm 时有效)
.PHONY: push
push:
ifeq ($(CROSS), arm)
	@echo "交叉编译 ARM 版本..."
	@$(MAKE) clean
	@$(MAKE) all CROSS=arm BUILD=release
	@echo "推送到 IMX6ULL..."
	adb push $(BUILD_DIR)/$(PROJECT) /root/$(PROJECT)
	adb shell chmod +x /root/$(PROJECT)
	@echo "$(COLOR_GREEN)[OK]$(COLOR_RESET) 已推送到 /root/$(PROJECT)"
	@echo "运行: adb shell /root/$(PROJECT) --selftest"
else
	@echo "$(COLOR_RED)[ERROR]$(COLOR_RESET) push 仅在 CROSS=arm 时有效"
	@echo "用法: make push CROSS=arm"
endif

# 交叉编译 + 推送 + 运行
.PHONY: run
run: push
	@echo ""
	adb shell /root/$(PROJECT) --selftest

# 显示帮助
.PHONY: help
help:
	@echo "CANopen 关节控制器 — 构建系统"
	@echo ""
	@echo "用法:"
	@echo "  make                 PC 编译 (调试模式, x86_64)"
	@echo "  make CROSS=arm       ARM 交叉编译 (IMX6ULL)"
	@echo "  make BUILD=release   发布版本 (优化 + 精简日志)"
	@echo "  make test            运行自检 + 单元测试"
	@echo "  make push CROSS=arm  交叉编译 + adb push 到板子"
	@echo "  make run  CROSS=arm  交叉编译 + push + 运行"
	@echo "  make clean           清理"
	@echo "  make help            显示此帮助"
	@echo ""
	@echo "编译器: $(CC)"
	@echo "版本:   $(VERSION)"
