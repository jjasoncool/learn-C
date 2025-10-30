# VS Code 專案設定文件

本目錄包含 Visual Studio Code 的專案配置檔案，用於 C 語言 GTK 專案的開發環境設定。

## 設定檔案說明

### `settings.json` - 工作區設定
- 配置預設終端機為 MSYS2 MinGW64 環境
- 指定 GCC 編譯器路徑
- 設定終端機環境變數

### `c_cpp_properties.json` - C/C++ IntelliSense 設定
- 定義標頭檔搜尋路徑
- 配置編譯器路徑與標準
- 支援 Windows 和 Linux 跨平台開發
- 啟用語法分析與程式碼自動完成

### `tasks.json` - 建置任務設定
- **編譯專案**: 編譯 Release 版本 (最佳化，無除錯資訊)
- **編譯除錯版**: 編譯 Debug 版本 (包含除錯資訊，無最佳化)
- **清理專案**: 移除所有編譯產物
- **執行程式**: 執行 Release 版本
- **執行除錯版**: 執行 Debug 版本

### `launch.json` - 除錯設定
- 配置 GDB 除錯器
- 自動編譯除錯版本
- 支援中斷點與變數檢視

## 編譯模式說明

### Debug 模式
- 編譯參數：`-g -DDEBUG -O0`
- 輸出檔案：`txt_processor_debug.exe`
- 特點：包含除錯資訊，啟用斷言，無最佳化

### Release 模式
- 編譯參數：`-O2 -DNDEBUG`
- 輸出檔案：`txt_processor.exe`
- 特點：程式碼最佳化，移除除錯資訊和斷言

## 快捷鍵與操作

| 功能 | 快捷鍵 | 說明 |
|------|--------|------|
| 建置專案 | `Ctrl+Shift+B` | 執行預設建置任務 (Release 模式) |
| 開始除錯 | `F5` | 編譯並啟動除錯工作階段 |
| 命令面板 | `Ctrl+Shift+P` | 存取所有 VS Code 指令 |
| 切換終端機 | `Ctrl+Shift+` ` | 開啟/關閉整合式終端機 |
| 除錯面板 | `Ctrl+Shift+D` | 開啟除錯與執行檢視 |

## 問題排除

### 終端機環境
- 確認使用 MSYS2 MinGW64 終端機
- 檢查環境變數 `MSYSTEM=MINGW64`
- 終端機右上角可手動切換設定檔

### 編譯問題
- 驗證 GTK3 開發套件安裝：`pkg-config --exists gtk+-3.0`
- 檢查編譯器版本：`gcc --version`
- 測試手動編譯：`make clean && make debug`

### IntelliSense 問題
- 重新載入設定：`Ctrl+Shift+P` → "C/C++: Reload IntelliSense Database"
- 檢查編譯器路徑設定
- 確認 include 路徑配置正確

## 開發工作流程

### 日常開發
1. 使用 `make debug` 或「編譯除錯版」任務
2. 設定中斷點進行除錯
3. 使用 F5 啟動除錯工作階段

### 發布準備
1. 執行 `make clean` 清理編譯產物
2. 使用 `make release` 編譯最佳化版本
3. 測試 Release 版本功能

### 跨平台開發
- Windows：使用 MSYS2 MinGW64 環境
- Linux：使用系統 GCC 與 GTK3 開發套件
- 設定檔會根據平台自動選擇對應配置
