# 文本數據處理工具

一個使用 C 語言和 GTK3 開發的圖形化工具，用於處理地理數據的**角度分析**和**高程轉換**。

## 功能特色

- 🖥️ **現代化圖籤式界面**: 使用 GTK3 Notebook 建構，提供多功能整合體驗。
- 📂 **資料夾批次處理**: 遞歸掃描指定資料夾中的所有 TXT 檔案。
- 🏔️ **高程轉換**: 支援 SEP 對照文件的地理空間插值，進行高精度高程調整。
- 📐 **角度分析**: 分析特定格式的 TXT 檔案，找出剖面內的最大角度差。
- ⚡ **非同步處理**: 將耗時的分析工作放在背景執行緒中，避免 UI 凍結。
- 📊 **即時進度反饋**: 提供進度條、狀態文字，並可隨時取消處理。
- 🧩 **模組化程式設計**: 將 UI、業務邏輯、檔案處理等功能清晰分離。
- 🛡️ **完善的錯誤處理**: 對檔案讀取、記憶體分配等潛在問題進行了處理。
- 🎯 **地理空間插值**: 使用距離加權插值和高效率空間索引。
- 📝 **詳細結果輸出**:
    - `angle_analysis_result.txt`: 記錄每個檔案中具有最大角度差的剖面及其詳細資訊。
    - `max_angle_result.txt`: 記錄所有檔案中的全域最大角度差及其來源檔案和剖面。
    - 高程轉換後檔案：帶有 `_converted` 後綴的處理結果檔案。

## 專案結構

```
Text_processor/
├── src/                    # 原始碼檔案
│   ├── main.c             # 🚀 應用程式主進入點
│   ├── callbacks.c        # 🎮 GTK事件處理與業務協調
│   ├── scan.c             # 🔍 檔案與目錄掃描模組
│   ├── angle_parser.c     # 📐 角度分析核心邏輯
│   ├── max_finder.c       # 🏆 全域最大值尋找
│   ├── safe_getline.c     # 🛡️ 安全檔案讀取工具
│   ├── features/          # ⚙️ 業務功能模組
│   │   ├── elevation_processing.c    # 🏔️ 高程轉換核心
│   │   ├── angle_processing.c        # 📐 角度處理邏輯
│   │   └── file_processing.c         # 📄 檔案處理工具
│   └── ui/               # 🖥️ 使用者介面層
│       ├── ui_main.c     # 主UI佈局
│       └── tabs/         # 功能頁籤
│           ├── angle_analysis_tab.c      # 角度分析頁籤
│           ├── elevation_conversion_tab.c # 高程轉換頁籤
│           └── data_conversion_tab.c      # 數據轉換頁籤
├── include/                # 📋 標頭檔
│   ├── callbacks.h        # 主狀態與回調定義
│   ├── elevation_processing.h # 高程處理介面
│   ├── ui.h               # UI介面定義
│   ├── scan.h             # 掃描功能介面
│   ├── angle_parser.h     # 角度解析介面
│   ├── max_finder.h       # 最大值尋找介面
│   └── safe_getline.h     # 安全讀取介面
├── build/                  # 🏗️ 編譯產物 (自動產生)
├── test_data/              # 🧪 測試資料
│   └── elevation/         # 高程測試檔案
├── scripts/                # 🔧 輔助腳本
│   ├── install_gtk.sh     # GTK環境安裝
│   └── test_compile.sh    # 編譯測試
├── Makefile                # ⚙️ 建置設定
├── README.md               # 📖 專案說明 (本檔案)
└── .gitignore              # 🚫 Git忽略清單
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

#### 📐 角度分析功能
1.  啟動程式後，切換到「角度分析」頁籤。
2.  點擊「選擇資料夾」按鈕，選擇包含 `.txt` 資料檔案的資料夾。
3.  點擊「掃描檔案」按鈕 (可選)，程式會列出在該資料夾中找到的所有 `.txt` 檔案。
4.  點擊「分析角度」按鈕，程式會開始在背景進行分析。
5.  進度條會顯示目前的處理進度，此時可點擊「取消」來終止分析。
6.  分析完成後，結果會顯示在下方的文字區域中，同時會在您選擇的資料夾中產生 `angle_analysis_result.txt` 和 `max_angle_result.txt` 兩個報告檔案。

#### 🏔️ 高程轉換功能
1.  啟動程式後，切換到「高程轉換」頁籤。
2.  點擊「選擇檔案」按鈕，選擇要處理的 `.txt` 數據檔案。
3.  點擊「選擇SEP檔案」按鈕，選擇對應的 SEP 對照檔案。
4.  點擊「執行轉換」按鈕，程式會開始進行高程轉換處理。
5.  處理過程中會顯示進度，並可隨時取消。
6.  轉換完成後，原始檔案會被修改為過濾後版本，並產生帶有 `_converted` 後綴的轉換結果檔案。

#### 📊 數據轉換功能
- 提供額外的數據格式轉換工具。

## 模組說明

### 🚀 核心模組
-   **`main.c`**: 程式的進入點，負責初始化 GTK 應用程式和 `AppState` 狀態結構。
-   **`callbacks.c` / `callbacks.h`**: 應用程式的核心控制器。負責處理所有 UI 事件，協調各模組工作，並管理非同步處理的執行緒。包含 `TideDataRow` 結構定義。

### 🖥️ 使用者介面層
-   **`ui/ui_main.c` / `ui/ui.h`**: 主UI佈局管理，建立 Notebook 頁籤式介面。
-   **`ui/tabs/angle_analysis_tab.c`**: 角度分析功能頁籤的UI實現。
-   **`ui/tabs/elevation_conversion_tab.c`**: 高程轉換功能頁籤的UI實現。
-   **`ui/tabs/data_conversion_tab.c`**: 數據轉換功能頁籤的UI實現。

### ⚙️ 業務邏輯層
-   **`features/elevation_processing.c`**: 🏔️ **高程轉換核心** - 實現地理空間插值、SEP數據載入、多層索引優化。使用 Context 模式管理複雜的處理狀態。
-   **`features/angle_processing.c`**: 📐 角度處理邏輯模組。
-   **`features/file_processing.c`**: 📄 檔案處理工具模組。

### 🔧 基礎工具模組
-   **`scan.c` / `scan.h`**: 提供遞歸掃描指定目錄下所有 `.txt` 檔案的功能。
-   **`angle_parser.c` / `angle_parser.h`**: 角度分析核心邏輯。解析檔案並計算 Profile 內的角度差。
-   **`max_finder.c` / `max_finder.h`**: 從分析結果中尋找全域最大角度差。
-   **`safe_getline.c` / `safe_getline.h`**: 安全檔案讀取工具，避免緩衝區溢位。

### 📋 介面定義
-   **`include/elevation_processing.h`**: 高程處理模組的介面定義。
-   **`include/callbacks.h`**: 包含 `TideDataRow` 結構和應用狀態定義。

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
