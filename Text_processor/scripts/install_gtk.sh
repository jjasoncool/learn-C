#!/bin/bash
# MSYS2 GTK 開發環境安裝腳本
# 請在 MSYS2 MinGW64 終端中執行此腳本

echo "==========================================="
echo "  MSYS2 GTK 開發環境自動安裝腳本"
echo "==========================================="
echo

# 檢查是否在 MSYS2 環境中
if [[ "$MSYSTEM" != "MINGW64" ]]; then
    echo "警告: 請在 MSYS2 MinGW64 終端中執行此腳本"
    echo "請開啟 'MSYS2 MinGW 64-bit' 終端"
    exit 1
fi

echo "正在更新套件資料庫..."
pacman -Sy

echo
echo "正在安裝 GCC 編譯器..."
pacman -S --needed --noconfirm mingw-w64-x86_64-gcc

echo
echo "正在安裝 GTK3 開發套件..."
pacman -S --needed --noconfirm mingw-w64-x86_64-gtk3

echo
echo "正在安裝 pkg-config..."
pacman -S --needed --noconfirm mingw-w64-x86_64-pkg-config

echo
echo "正在安裝 make..."
pacman -S --needed --noconfirm make

echo
echo "正在安裝其他有用的開發工具..."
pacman -S --needed --noconfirm mingw-w64-x86_64-gdb
pacman -S --needed --noconfirm mingw-w64-x86_64-ninja

echo
echo "==========================================="
echo "  安裝完成！"
echo "==========================================="
echo

# 驗證安裝
echo "正在驗證安裝..."
echo

if command -v gcc &> /dev/null; then
    echo "✓ GCC: $(gcc --version | head -n1)"
else
    echo "✗ GCC 未找到"
fi

if pkg-config --exists gtk+-3.0; then
    echo "✓ GTK3: $(pkg-config --modversion gtk+-3.0)"
else
    echo "✗ GTK3 未找到"
fi

if command -v pkg-config &> /dev/null; then
    echo "✓ pkg-config: $(pkg-config --version)"
else
    echo "✗ pkg-config 未找到"
fi

if command -v make &> /dev/null; then
    echo "✓ make: $(make --version | head -n1)"
else
    echo "✗ make 未找到"
fi

echo
echo "安裝完成！您現在可以編譯 GTK 程序了。"
echo
echo "建議將以下路徑添加到 Windows PATH 環境變數："
echo "$(cygpath -w $MINGW_PREFIX/bin)"
echo
echo "或者始終在 MSYS2 MinGW64 終端中進行編譯。"
