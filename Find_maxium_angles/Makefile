# Makefile for GTK Application on Windows
# 使用 MSYS2/MinGW64 環境

# 編譯器
CC = gcc

# 目標執行檔名稱
TARGET = txt_processor.exe

# 源檔案
SOURCES = main.c file_processor.c

# 物件檔案
OBJECTS = main.o file_processor.o

# GTK 相關的編譯和連結標誌
# 使用 pkg-config 來取得 GTK3 的編譯參數
CFLAGS = $(shell pkg-config --cflags gtk+-3.0) -Wall -Wextra -std=c99
LDFLAGS = $(shell pkg-config --libs gtk+-3.0)

# 預設目標
all: $(TARGET)

# 編譯目標
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# 編譯物件檔案
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 明確定義依賴關係
main.o: main.c file_processor.h
file_processor.o: file_processor.c file_processor.h

# 清理編譯產物
clean:
	rm -f $(TARGET) $(OBJECTS)

# 執行程序
run: $(TARGET)
	./$(TARGET)

# 安裝依賴（僅提供參考，需要在 MSYS2 環境中執行）
install-deps:
	@echo "請在 MSYS2 環境中執行以下命令來安裝依賴:"
	@echo "pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-pkg-config mingw-w64-x86_64-gcc"

.PHONY: all clean run install-deps
