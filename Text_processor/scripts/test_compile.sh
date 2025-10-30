#!/bin/bash
# GTK 編譯測試腳本
# 在 MSYS2 MinGW64 終端中執行

echo "==========================================="
echo "  GTK 編譯測試"
echo "==========================================="
echo

# 檢查環境
if [[ "$MSYSTEM" != "MINGW64" ]]; then
    echo "警告: 建議在 MSYS2 MinGW64 終端中執行"
fi

echo "檢查編譯環境..."

# 檢查 GCC
if ! command -v gcc &> /dev/null; then
    echo "錯誤: 找不到 gcc 編譯器"
    echo "請執行: pacman -S mingw-w64-x86_64-gcc"
    exit 1
fi

# 檢查 pkg-config
if ! command -v pkg-config &> /dev/null; then
    echo "錯誤: 找不到 pkg-config"
    echo "請執行: pacman -S mingw-w64-x86_64-pkg-config"
    exit 1
fi

# 檢查 GTK3
if ! pkg-config --exists gtk+-3.0; then
    echo "錯誤: 找不到 GTK3 開發套件"
    echo "請執行: pacman -S mingw-w64-x86_64-gtk3"
    exit 1
fi

echo "✓ 環境檢查通過"
echo

# 顯示編譯參數
echo "GTK3 編譯參數:"
echo "CFLAGS: $(pkg-config --cflags gtk+-3.0)"
echo "LIBS: $(pkg-config --libs gtk+-3.0)"
echo

# 編譯程序
echo "正在編譯 txt_processor..."
gcc -Wall -Wextra -std=c99 $(pkg-config --cflags gtk+-3.0) -o txt_processor.exe main.c $(pkg-config --libs gtk+-3.0)

if [ $? -eq 0 ]; then
    echo "✓ 編譯成功！"
    echo
    echo "執行檔案已創建: txt_processor.exe"
    echo "檔案大小: $(ls -lh txt_processor.exe | awk '{print $5}')"
    echo
    echo "是否要立即執行程序? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo "啟動程序..."
        ./txt_processor.exe &
    fi
else
    echo "✗ 編譯失敗！"
    echo "請檢查錯誤訊息並修正問題"
    exit 1
fi
