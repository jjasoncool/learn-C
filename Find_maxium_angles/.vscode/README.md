# VS Code 設定說明

這個資料夾包含 VS Code 的專案設定，讓你更容易開發 C 語言程式。

## 檔案說明

### `settings.json` - 基本設定
- 設定預設終端機為 MSYS2 (包含 gcc 編譯器)
- 告訴 VS Code 編譯器的位置

### `c_cpp_properties.json` - C/C++ 設定
- 告訴 VS Code 去哪裡找你的 `.h` 檔案
- 設定使用 C99 標準
- 啟用語法檢查和自動完成

### `tasks.json` - 快捷任務
- **編譯專案**: 按 `Ctrl+Shift+P` → 搜尋 "Tasks: Run Build Task"
- **清理專案**: 按 `Ctrl+Shift+P` → 搜尋 "Tasks: Run Task" → 選擇 "清理專案"
- **執行程式**: 按 `Ctrl+Shift+P` → 搜尋 "Tasks: Run Task" → 選擇 "執行程式"

## 常用快捷鍵

- `Ctrl+Shift+P` - 開啟命令面板
- `Ctrl+Shift+` ` - 開啟/關閉終端機
- `Ctrl+Shift+B` - 快速編譯 (執行預設建置任務)

## 如果有問題

1. **終端機不是 MSYS2**:
   - 關閉終端機，重新開啟 (`Ctrl+Shift+` `)
   - 或手動選擇: 終端機右上角下拉選單 → 選擇 "MSYS2 Bash"

2. **編譯錯誤**:
   - 確認 MSYS2 已安裝 GTK: `pacman -S mingw-w64-x86_64-gtk3`
   - 在終端機手動測試: `make clean && make`

3. **VS Code 找不到標頭檔**:
   - 重新載入 VS Code: `Ctrl+Shift+P` → "Developer: Reload Window"

## 學習建議

初學者建議：
1. 先學會在終端機使用 `make` 指令
2. 熟悉後再使用 VS Code 的快捷任務
3. 有問題時優先檢查終端機的錯誤訊息
