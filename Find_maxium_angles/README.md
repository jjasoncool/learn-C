# TXT 檔案處理工具

這是一個使用 GTK3 開發的 C 語言程序，用於處理資料夾中的 TXT 檔案。

## 功能特性

- 圖形化界面，使用 GTK3
- 選擇資料夾功能
- 讀取資料夾中的所有 TXT 檔案（功能開發中）
- 顯示處理結果（功能開發中）

## 環境需求

### Windows 環境設置

1. **安裝 MSYS2**
   - 從 https://www.msys2.org/ 下載並安裝 MSYS2
   - 安裝完成後，開啟 MSYS2 終端

2. **安裝必要的套件**
   ```bash
   # 更新套件資料庫
   pacman -Syu

   # 安裝編譯工具和 GTK3
   pacman -S mingw-w64-x86_64-gcc
   pacman -S mingw-w64-x86_64-gtk3
   pacman -S mingw-w64-x86_64-pkg-config
   pacman -S make
   ```

3. **設置環境變數**
   - 將 `C:\msys64\mingw64\bin` 添加到系統 PATH 環境變數中

## 編譯和執行

1. **編譯程序**
   ```bash
   make
   ```

2. **執行程序**
   ```bash
   make run
   # 或者直接執行
   ./txt_processor.exe
   ```

3. **清理編譯產物**
   ```bash
   make clean
   ```

## 使用說明

1. 啟動程序後，會看到一個圖形化界面
2. 點擊「選擇資料夾」按鈕來選擇包含 TXT 檔案的資料夾
3. 點擊「處理檔案」按鈕來處理選中資料夾中的檔案
4. 結果會顯示在下方的文字區域中

## 當前狀態

- ✅ 基本 GTK 界面
- ✅ 資料夾選擇功能
- ⏳ TXT 檔案讀取功能（開發中）
- ⏳ 檔案處理邏輯（開發中）
- ⏳ 結果輸出功能（開發中）

## 程序結構

```
Find_maxium_angles/
├── main.c          # 主程序檔案
├── Makefile        # 編譯配置檔案
└── README.md       # 說明文件
```

## 後續開發計劃

1. 實現 TXT 檔案掃描功能
2. 添加檔案內容讀取功能
3. 實現特定的檔案處理邏輯
4. 改善界面和用戶體驗
5. 添加錯誤處理和狀態回饋

## 快速開始

1. **檢查環境**: 執行 `check_environment.bat` 檢查開發環境
2. **自動安裝**: 在 MSYS2 MinGW64 終端中執行 `./install_gtk.sh`
3. **測試編譯**: 執行 `./test_compile.sh` 測試編譯

## 故障排除

### 找不到 gtk/gtk.h 錯誤

如果出現 `#include <gtk/gtk.h>` 找不到套件的錯誤，請按照以下步驟操作：

#### 步驟 1: 安裝 MSYS2
1. 下載並安裝 MSYS2: https://www.msys2.org/
2. 使用默認安裝路徑: `C:\msys64\`

#### 步驟 2: 安裝開發套件
開啟 **MSYS2 MinGW 64-bit** 終端（重要：不是普通的 MSYS2 終端），執行：

```bash
# 更新套件資料庫
pacman -Syu

# 安裝必要套件
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-gtk3
pacman -S mingw-w64-x86_64-pkg-config
pacman -S make

# 或者一次安裝所有套件
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk3 mingw-w64-x86_64-pkg-config make
```

#### 步驟 3: 驗證安裝
在 MSYS2 MinGW64 終端中執行：
```bash
gcc --version
pkg-config --version
pkg-config --exists gtk+-3.0 && echo "GTK3 OK" || echo "GTK3 NOT FOUND"
```

#### 步驟 4: 編譯方式選擇

**方法 1: 在 MSYS2 終端中編譯（推薦）**
```bash
./test_compile.sh
```

**方法 2: 在 Windows 命令提示字元中編譯**
1. 將 `C:\msys64\mingw64\bin` 添加到系統 PATH 環境變數
2. 重啟命令提示字元
3. 執行 `build.bat`

### 常見錯誤解決方案

#### 錯誤: "gtk+-3.0 not found"
```bash
# 重新安裝 GTK3
pacman -S mingw-w64-x86_64-gtk3
```

#### 錯誤: "pkg-config command not found"
```bash
# 安裝 pkg-config
pacman -S mingw-w64-x86_64-pkg-config
```

#### 錯誤: "gcc command not found"
```bash
# 安裝 GCC 編譯器
pacman -S mingw-w64-x86_64-gcc
```

#### 編譯成功但執行時缺少 DLL
- 確保在 MSYS2 MinGW64 環境中執行程序
- 或者將 `C:\msys64\mingw64\bin` 添加到 PATH

### 執行錯誤
- 確保所有 GTK3 相關的 DLL 檔案可以被找到
- 檢查是否在正確的環境中執行程序
