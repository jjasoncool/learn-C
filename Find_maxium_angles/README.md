# 最大角度差分析工具

一個使用 C 語言和 GTK3 開發的圖形化工具，用於分析特定格式的 TXT 檔案，並找出指定剖面 (Profile) 內的最大角度差。

## 功能特色

- 🖥️ **現代化圖形界面**: 使用 GTK3 建構，提供友善的使用者體驗。
- 📂 **資料夾批次處理**: 遞歸掃描指定資料夾中的所有 TXT 檔案。
- ⚡ **非同步處理**: 將耗時的分析工作放在背景執行緒中，避免 UI 凍結。
- 📊 **即時進度反饋**: 提供進度條、狀態文字，並可隨時取消處理。
- 🧩 **模組化程式設計**: 將 UI、事件處理、檔案掃描、資料解析和結果計算等功能清晰分離。
- 🛡️ **完善的錯誤處理**: 對檔案讀取、記憶體分配等潛在問題進行了處理。
- 📝 **詳細結果輸出**:
    - `angle_analysis_result.txt`: 記錄每個檔案中具有最大角度差的剖面及其詳細資訊。
    - `max_angle_result.txt`: 記錄所有檔案中的全域最大角度差及其來源檔案和剖面。

## 專案結構

```
Find_maxium_angles/
├── src/                    # 原始碼檔案
│   ├── main.c             # 應用程式主進入點
│   ├── ui.c               # GTK 介面佈局與建立
│   ├── callbacks.c        # GTK 事件回呼與主邏輯協調
│   ├── scan.c             # 檔案與目錄掃描模組
│   ├── angle_parser.c     # 核心分析邏輯：解析檔案並計算角度差
│   ├── max_finder.c       # 從分析結果中尋找全域最大值
│   └── safe_getline.c     # 安全讀取檔案行的輔助函式
├── include/                # 標頭檔
│   ├── ui.h
│   ├── callbacks.h
│   ├── scan.h
│   ├── angle_parser.h
│   ├── max_finder.h
│   └── safe_getline.h
├── build/                  # 編譯產物 (自動產生)
├── test_data/              # 測試資料
├── scripts/                # 輔助腳本
│   ├── install_gtk.sh     # GTK 環境安裝腳本
│   └── test_compile.sh    # 編譯測試腳本
├── Makefile                # 建置設定
├── README.md               # 專案說明 (本檔案)
└── .gitignore              # Git 忽略清單
```

## 快速開始

### 1. 安裝依賴 (MSYS2/MinGW64)

```bash
# 建議執行安裝腳本
./scripts/install_gtk.sh

# 或手動安裝
pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-pkg-config mingw-w64-x86_64-gcc make
```

### 2. 編譯專案

```bash
# 清理並編譯 (產生 release 版本的 ./build/txt_processor.exe)
make clean
make

# 編譯 debug 版本 (產生 ./build/txt_processor_debug.exe)
make debug
```

### 3. 執行程式

```bash
# 編譯並執行 release 版本
make run

# 或直接執行
./build/txt_processor.exe
```

### 4. 使用說明

1.  啟動程式後，點擊「選擇資料夾」按鈕，選擇包含 `.txt` 資料檔案的資料夾。
2.  點擊「掃描檔案」按鈕 (可選)，程式會列出在該資料夾中找到的所有 `.txt` 檔案。
3.  點擊「分析角度」按鈕，程式會開始在背景進行分析。
4.  進度條會顯示目前的處理進度，此時可點擊「取消」來終止分析。
5.  分析完成後，結果會顯示在下方的文字區域中，同時會在您選擇的資料夾中產生 `angle_analysis_result.txt` 和 `max_angle_result.txt` 兩個報告檔案。

## 模組說明

-   **`main.c`**: 程式的進入點，負責初始化 GTK 應用程式和 `AppState` 狀態結構。
-   **`ui.c` / `ui.h`**: 負責建立所有 GTK 圖形介面元件 (視窗、按鈕、文字視圖等)，並將它們組織在主視窗中。
-   **`callbacks.c` / `callbacks.h`**: 應用程式的核心控制器。負責處理所有 UI 事件 (如按鈕點擊)，協調其他模組完成工作，並管理非同步處理的執行緒。
-   **`scan.c` / `scan.h`**: 提供掃描指定目錄下所有 `.txt` 檔案的功能。
-   **`angle_parser.c` / `angle_parser.h`**: 專案的核心業務邏輯。它負責解析單一 `.txt` 檔案，按 "Profile ID" 分組，並計算每個 Profile 內最大和最小 "Bin" 之間的角度差。
-   **`max_finder.c` / `max_finder.h`**: 負責讀取 `angle_parser.c` 產生的中間結果檔案，並從中找出全域最大角度差。
-   **`safe_getline.c` / `safe_getline.h`**: 一個健壯的輔助函式，用於安全地從檔案中讀取任意長度的行，避免緩衝區溢位。

## 故障排除

如果遇到 `gtk/gtk.h` 找不到或缺少 DLL 的問題，請參考以下步驟。

#### 步驟 1: 安裝與設定 MSYS2
1.  下載並安裝 MSYS2: https://www.msys2.org/
2.  開啟 **MSYS2 MinGW 64-bit** 終端 (重要提示：不是 MSYS2 MSYS 或其他終端)。

#### 步驟 2: 安裝開發套件
在 MSYS2 MinGW 64-bit 終端中執行：
```bash
# 更新套件資料庫
pacman -Syu

# 一次安裝所有必要套件
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk3 mingw-w64-x86_64-pkg-config make
```

#### 步驟 3: 驗證安裝
在 MSYS2 MinGW64 終端中執行：
```bash
gcc --version
pkg-config --version
pkg-config --exists gtk+-3.0 && echo "GTK3 OK" || echo "GTK3 NOT FOUND"
```
如果 GTK3 顯示 "OK"，表示您的環境已準備就緒。

#### 步驟 4: 編譯
在專案根目錄下，於 MSYS2 MinGW64 終端中執行 `make`。
