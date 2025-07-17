# Makefile for GTK Application on Windows
# 使用 MSYS2/MinGW64 環境

# 編譯器
CC = gcc

# 目錄設定
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# 目標執行檔名稱 (會根據 BUILD_MODE 動態調整)
# TARGET 在上面的編譯模式設定中定義

# 源檔案
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/scan.c $(SRC_DIR)/angle_parser.c $(SRC_DIR)/max_finder.c $(SRC_DIR)/callbacks.c $(SRC_DIR)/ui.c

# 物件檔案
OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/scan.o $(BUILD_DIR)/angle_parser.o $(BUILD_DIR)/max_finder.o $(BUILD_DIR)/callbacks.o $(BUILD_DIR)/ui.o

# GTK 相關的編譯和連結標誌
# 使用 pkg-config 來取得 GTK3 的編譯參數
BASE_CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall -Wextra -std=c99 -I$(INCLUDE_DIR)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0)

# 編譯模式設定 (預設為 release)
BUILD_MODE ?= release

# 根據編譯模式設定不同的 CFLAGS
ifeq ($(BUILD_MODE), debug)
    CFLAGS = $(BASE_CFLAGS) -g -DDEBUG -O0        # 除錯版本: 包含除錯資訊, 無優化
    TARGET_SUFFIX = _debug
else
    CFLAGS = $(BASE_CFLAGS) -O2 -DNDEBUG -mwindows # 正式版本: 優化編譯, 移除斷言, 隱藏控制台
    TARGET_SUFFIX =
endif

# 重新定義目標檔名
TARGET = $(BUILD_DIR)/txt_processor$(TARGET_SUFFIX).exe

# 預設目標
all: $(BUILD_DIR) $(TARGET)

# 建立建置目錄
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 編譯目標
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# 編譯物件檔案
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 明確定義依賴關係
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INCLUDE_DIR)/ui.h $(INCLUDE_DIR)/callbacks.h
$(BUILD_DIR)/scan.o: $(SRC_DIR)/scan.c $(INCLUDE_DIR)/scan.h
$(BUILD_DIR)/angle_parser.o: $(SRC_DIR)/angle_parser.c $(INCLUDE_DIR)/angle_parser.h $(INCLUDE_DIR)/scan.h
$(BUILD_DIR)/max_finder.o: $(SRC_DIR)/max_finder.c $(INCLUDE_DIR)/max_finder.h
$(BUILD_DIR)/callbacks.o: $(SRC_DIR)/callbacks.c $(INCLUDE_DIR)/callbacks.h $(INCLUDE_DIR)/scan.h $(INCLUDE_DIR)/angle_parser.h $(INCLUDE_DIR)/max_finder.h
$(BUILD_DIR)/ui.o: $(SRC_DIR)/ui.c $(INCLUDE_DIR)/ui.h $(INCLUDE_DIR)/callbacks.h

# 清理編譯產物
clean:
	rm -rf $(BUILD_DIR)

# 執行程序
run: $(TARGET)
	$(TARGET)

# 除錯版本編譯
debug:
	$(MAKE) BUILD_MODE=debug

# 正式版本編譯 (預設)
release:
	$(MAKE) BUILD_MODE=release

# 執行除錯版本
run-debug:
	$(MAKE) BUILD_MODE=debug
	$(BUILD_DIR)/txt_processor_debug.exe

# 顯示目前編譯模式
info:
	@echo "目前編譯模式: $(BUILD_MODE)"
	@echo "編譯參數: $(CFLAGS)"
	@echo "目標檔案: $(TARGET)"

# 安裝依賴（僅提供參考，需要在 MSYS2 環境中執行）
install-deps:
	@echo "請在 MSYS2 環境中執行以下命令來安裝依賴:"
	@echo "pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-pkg-config mingw-w64-x86_64-gcc"

# 開發用指令
dev-setup:
	@echo "開發環境設定..."
	./scripts/install_gtk.sh

test-compile:
	@echo "測試編譯..."
	./scripts/test_compile.sh

.PHONY: all clean run install-deps dev-setup test-compile debug release run-debug info
