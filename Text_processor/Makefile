# Makefile for GTK Application (Linux + Windows/MSYS2)
# 支援：
#  - Linux: 正常編譯/執行
#  - Windows (MSYS2/MinGW64): 正常編譯/執行，並可一鍵打包成可攜資料夾（不需另外安裝 GTK）

# ===== 編譯器與基本路徑 =====
CC            := gcc
SRC_DIR       := src
INCLUDE_DIR   := include
BUILD_DIR     := build
DIST_DIR      := dist

# ===== 原始碼與物件 =====
SOURCES := $(SRC_DIR)/main.c \
           $(SRC_DIR)/scan.c \
           $(SRC_DIR)/angle_parser.c \
           $(SRC_DIR)/max_finder.c \
           $(SRC_DIR)/callbacks.c \
           $(SRC_DIR)/features/angle_processing.c \
           $(SRC_DIR)/features/file_processing.c \
           $(SRC_DIR)/features/elevation_processing.c \
           $(SRC_DIR)/ui/ui_main.c \
           $(SRC_DIR)/ui/tabs/angle_analysis_tab.c \
           $(SRC_DIR)/ui/tabs/elevation_conversion_tab.c \
           $(SRC_DIR)/ui/tabs/data_conversion_tab.c \
           $(SRC_DIR)/safe_getline.c

OBJECTS := $(BUILD_DIR)/main.o \
           $(BUILD_DIR)/scan.o \
           $(BUILD_DIR)/angle_parser.o \
           $(BUILD_DIR)/max_finder.o \
           $(BUILD_DIR)/callbacks.o \
           $(BUILD_DIR)/angle_processing.o \
           $(BUILD_DIR)/file_processing.o \
           $(BUILD_DIR)/elevation_processing.o \
           $(BUILD_DIR)/ui_main.o \
           $(BUILD_DIR)/angle_analysis_tab.o \
           $(BUILD_DIR)/elevation_conversion_tab.o \
           $(BUILD_DIR)/data_conversion_tab.o \
           $(BUILD_DIR)/safe_getline.o

# ===== 平台偵測 =====
UNAME_S    := $(shell uname -s)
IS_WINDOWS := $(if $(findstring MINGW,$(UNAME_S)),1,0)
IS_LINUX   := $(if $(findstring Linux,$(UNAME_S)),1,0)

# ===== GTK 編譯參數（由 pkg-config 取得）=====
# 注意：請在 MSYS2「MINGW64 shell」內執行（而非 MSYS shell），確保 pkg-config 指向 /mingw64
BASE_CFLAGS := $(shell pkg-config --cflags gtk+-3.0) -Wall -Wextra -std=c99 \
               -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 -I$(INCLUDE_DIR)
BASE_LDLIBS := $(shell pkg-config --libs gtk+-3.0) -lm

# ===== 編譯模式 =====
BUILD_MODE ?= release

ifeq ($(BUILD_MODE),debug)
    CFLAGS        := $(BASE_CFLAGS) -g -DDEBUG -O0
    LDFLAGS_FINAL := $(BASE_LDLIBS)
    TARGET_SUFFIX := _debug
else
    CFLAGS        := $(BASE_CFLAGS) -O2 -DNDEBUG -pipe
    LDFLAGS_FINAL := $(BASE_LDLIBS)
    TARGET_SUFFIX :=
endif

# Windows 下隱藏主控台視窗，放在 LDFLAGS（不要放 CFLAGS）
ifeq ($(IS_WINDOWS),1)
    ifeq ($(BUILD_MODE),release)
        LDFLAGS_FINAL += -mwindows
    endif
    # 靜態連結 libgcc 以減少外部依賴（GTK 本身仍為動態）
    LDFLAGS_FINAL += -static-libgcc
endif

# ===== 目標檔名（依平台加副檔名）=====
ifeq ($(IS_WINDOWS),1)
    TARGET := $(BUILD_DIR)/txt_processor$(TARGET_SUFFIX).exe
else
    TARGET := $(BUILD_DIR)/txt_processor$(TARGET_SUFFIX)
endif

# ===== vpath 與預設目標 =====
vpath %.c $(SRC_DIR) $(SRC_DIR)/ui $(SRC_DIR)/ui/tabs $(SRC_DIR)/features

.PHONY: all clean run debug release run-debug info dist-win dist-linux clean-dist

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ===== 一般編譯規則 =====
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS_FINAL)
ifeq ($(BUILD_MODE),release)
	-@which strip >/dev/null 2>&1 && strip $@ || true
endif

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 明確依賴
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(INCLUDE_DIR)/ui.h $(INCLUDE_DIR)/callbacks.h
$(BUILD_DIR)/scan.o: $(SRC_DIR)/scan.c $(INCLUDE_DIR)/scan.h
$(BUILD_DIR)/angle_parser.o: $(SRC_DIR)/angle_parser.c $(INCLUDE_DIR)/angle_parser.h $(INCLUDE_DIR)/scan.h $(INCLUDE_DIR)/safe_getline.h
$(BUILD_DIR)/max_finder.o: $(SRC_DIR)/max_finder.c $(INCLUDE_DIR)/max_finder.h $(INCLUDE_DIR)/safe_getline.h
$(BUILD_DIR)/callbacks.o: $(SRC_DIR)/callbacks.c $(INCLUDE_DIR)/callbacks.h $(INCLUDE_DIR)/scan.h $(INCLUDE_DIR)/angle_parser.h $(INCLUDE_DIR)/max_finder.h
$(BUILD_DIR)/ui_main.o: $(SRC_DIR)/ui/ui_main.c $(SRC_DIR)/ui/ui.h $(INCLUDE_DIR)/callbacks.h
$(BUILD_DIR)/angle_analysis_tab.o: $(SRC_DIR)/ui/tabs/angle_analysis_tab.c $(SRC_DIR)/ui/ui.h $(INCLUDE_DIR)/callbacks.h
$(BUILD_DIR)/elevation_conversion_tab.o: $(SRC_DIR)/ui/tabs/elevation_conversion_tab.c $(SRC_DIR)/ui/ui.h $(INCLUDE_DIR)/callbacks.h
$(BUILD_DIR)/data_conversion_tab.o: $(SRC_DIR)/ui/tabs/data_conversion_tab.c $(SRC_DIR)/ui/ui.h
$(BUILD_DIR)/safe_getline.o: $(SRC_DIR)/safe_getline.c $(INCLUDE_DIR)/safe_getline.h

# ===== 便利指令 =====
clean:
	rm -rf $(BUILD_DIR)

clean-dist:
	rm -rf $(DIST_DIR)

run: $(TARGET)
	$(TARGET)

debug:
	$(MAKE) BUILD_MODE=debug

release:
	$(MAKE) BUILD_MODE=release

run-debug:
	$(MAKE) BUILD_MODE=debug
	$(BUILD_DIR)/txt_processor_debug$(if $(IS_WINDOWS),.exe,)

info:
	@echo "OS: $(UNAME_S)"
	@echo "BUILD_MODE: $(BUILD_MODE)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS_FINAL: $(LDFLAGS_FINAL)"
	@echo "TARGET: $(TARGET)"

# ===========================================================================================
#                                Windows 打包（可攜資料夾）
# ===========================================================================================
# 需求（MSYS2/MinGW64）：
#   pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-ntldd mingw-w64-x86_64-gdk-pixbuf2 \
#              mingw-w64-x86_64-glib2 mingw-w64-x86_64-pango mingw-w64-x86_64-harfbuzz
#
# 說明：
#   - 產出 dist/txt_processor/ 內含 exe、所需 DLL、gdk-pixbuf loaders、glib schemas、Adwaita icons/themes。
#   - 會自動建立 run.cmd，設定執行所需的環境變數後啟動 exe。
#   - 單檔 exe 幾乎不可行（GTK3 模組化），資料夾式可攜才是官方建議的 Windows 發行方式。

# MSYS2 prefix（用 pkg-config 讀） → /mingw64
MINGW_PREFIX := $(shell pkg-config --variable=prefix gtk+-3.0)

DIST_WIN_DIR := $(DIST_DIR)/txt_processor
NTLDD        := ntldd -R

dist-win: release $(TARGET)
	@echo "==> 準備可攜式 Windows 發行資料夾：$(DIST_WIN_DIR)"
	rm -rf $(DIST_WIN_DIR)
	mkdir -p $(DIST_WIN_DIR)
	cp -f $(TARGET) $(DIST_WIN_DIR)/txt_processor.exe

	@echo "==> 複製依賴 DLL（使用 ntldd 遞迴解析）"
	$(NTLDD) $(TARGET) | sed -n 's/.*=> \(.*\/mingw64\/bin\/[^ ]*\.dll\).*/\1/p' | sort -u | xargs -r -I{} cp -u {} $(DIST_WIN_DIR)/

	@echo "==> 複製 gdk-pixbuf loaders 並產生 loaders.cache"
	mkdir -p $(DIST_WIN_DIR)/lib/gdk-pixbuf-2.0
	cp -r $(MINGW_PREFIX)/lib/gdk-pixbuf-2.0/* $(DIST_WIN_DIR)/lib/gdk-pixbuf-2.0/
	GDK_PIXBUF_MODULEDIR="$(DIST_WIN_DIR)/lib/gdk-pixbuf-2.0/2.10.0/loaders" \
	gdk-pixbuf-query-loaders > $(DIST_WIN_DIR)/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache

	@echo "==> 複製 GLib schemas 並編譯"
	mkdir -p $(DIST_WIN_DIR)/share/glib-2.0
	cp -r $(MINGW_PREFIX)/share/glib-2.0/schemas $(DIST_WIN_DIR)/share/glib-2.0/
	glib-compile-schemas $(DIST_WIN_DIR)/share/glib-2.0/schemas

	@echo "==> 複製圖示與主題（Adwaita）"
	mkdir -p $(DIST_WIN_DIR)/share/icons
	cp -r $(MINGW_PREFIX)/share/icons/Adwaita $(DIST_WIN_DIR)/share/icons/ || true
	-gtk-update-icon-cache -t -f $(DIST_WIN_DIR)/share/icons/Adwaita || true
	mkdir -p $(DIST_WIN_DIR)/share/themes
	cp -r $(MINGW_PREFIX)/share/themes/Adwaita $(DIST_WIN_DIR)/share/themes/ || true

	@echo "==> 複製語系（可選）"
	cp -r $(MINGW_PREFIX)/share/locale $(DIST_WIN_DIR)/share/ || true

	@echo "==> 產生 Windows 啟動器 run.cmd"
	@{ \
	  echo "@echo off"; \
	  echo "setlocal"; \
	  echo "set DIR=%~dp0"; \
	  echo "set PATH=%DIR%;%DIR%\\lib;%DIR%\\bin;%PATH%"; \
	  echo "set GTK_DATA_PREFIX=%DIR%"; \
	  echo "set GSETTINGS_SCHEMA_DIR=%DIR%\\share\\glib-2.0\\schemas"; \
	  echo "set GDK_PIXBUF_MODULE_FILE=%DIR%\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache"; \
	  echo "start \"\" \"%DIR%\\txt_processor.exe\""; \
	} > $(DIST_WIN_DIR)/run.cmd

	@echo "==> 完成：$(DIST_WIN_DIR)"
	@echo "    請在未安裝 GTK 的 Windows 上，直接執行 dist/txt_processor/run.cmd"

# ===========================================================================================
#                                  Linux 打包（可選：AppImage）
# ===========================================================================================
# 建議用 AppImage / Flatpak。以下提供 AppImage 範例（需自行下載 linuxdeploy 與 appimagetool）：
#  linuxdeploy-x86_64.AppImage --appdir $(DIST_DIR)/AppDir \
#       --executable $(TARGET) --desktop-file your.desktop --icon-file your.png \
#       --output appimage
#  在多發行版環境下不要私拷系統 .so，請用上述工具。

dist-linux: release
	@echo "==> Linux 建議用 AppImage/Flatpak 方式打包；此目標僅保留編譯成品於 $(TARGET)"
	@echo "    可參考 linuxdeploy/appimagetool 建立 AppImage。"
