# magfield_processor 使用手冊

## 程式用途
`magfield_processor` 是一個工具，用於處理磁場數據文件。它會讀取指定目錄中的 `.sec` 文件，將 UTC 時間轉換為 UTC+8，計算磁場強度，並將結果保存到新的 `.txt` 文件中。

## 使用方法
1. 將 `magfield_processor.exe` 放到一個目錄中。
2. 準備一個包含 `.sec` 文件的資料夾（例如 `C:\data\magfield`）。
3. 開啟命令提示字元（CMD）。
4. 輸入以下命令並按 Enter：
   ```cmd
   magfield_processor.exe C:\data\magfield
   ```
   （將 `C:\data\magfield` 替換為實際的資料夾路徑）

## 輸入文件格式
`.sec` 文件的前 13 行會被跳過，之後每行應包含以下格式：
```
YYYY-MM-DD HH:MM:SS.sss <忽略欄位> X Y Z <忽略欄位>
```

### 範例（IAGA-2002 格式檔案，部分內容）
```
Format                 IAGA-2002                                    |
Source of Data         Central Weather Administration               |
Station Name           Neicheng                                     |
IAGA Code                                                           |
Geodetic Latitude      24.718                                       |
Geodetic Longitude     121.683                                      |
Elevation              5                                            |
Reported               XYZF                                         |
Sensor Orientation     XYZF                                         |
Digital Sampling       1 second                                     |
Data Interval Type     1-second instantaneous                       |
Data Type              Definitive                                   |
DATE       TIME         DOY     NCGX      NCGY      NCGZ      NCGF   |
2024-09-03 00:00:00.000 247     36287.79  -2090.55  27116.22  88888.00
2024-09-03 00:00:01.000 247     36287.78  -2090.55  27116.21  88888.00
2024-09-03 00:00:02.000 247     36287.77  -2090.56  27116.23  88888.00
```

## 輸出文件格式
程式會生成 `.txt` 文件，每行格式為：
```
MM/DD/YY HH:MM:SS    magnitude
```

### 範例（對應上述輸入文件的部分輸出）
```
09/03/24 08:00:00    45118.35
09/03/24 08:00:01    45118.34
09/03/24 08:00:02    45118.34
```
> 註：`magnitude` 為 `sqrt(NCGX^2 + NCGY^2 + NCGZ^2)` 的計算結果。

## 注意事項
- 確保 `.sec` 文件位於指定的資料夾中。
- 路徑中不要有空格，若有空格請用雙引號括住，例如：
  ```cmd
  magfield_processor.exe "C:\my data\magfield"
  ```
- 如果程式無法運行，可能需要檢查資料夾是否存在或是否有權限。

---

# mergefile 使用手冊

## 程式用途
`mergefile` 是一個工具，用於合併指定資料夾中的多個文字檔案。它會根據每個檔案第二行第 15 欄的數字（`LINE`）進行排序，然後將所有檔案的內容合併到一個名為 `merged_file.txt` 的檔案中。合併時只保留第一個檔案的標頭（若有）。

## 使用方法
1. 將 `mergefile.exe` 放到一個目錄中。
2. 準備一個包含需要合併的文字檔案的資料夾（例如 `C:\data\files`）。
3. 開啟命令提示字元（CMD）。
4. 輸入以下命令並按 Enter：
   ```cmd
   mergefile.exe C:\data\files
   ```
   （將 `C:\data\files` 替換為實際的資料夾路徑）
   - 如果不提供路徑，程式會處理當前目錄中的檔案。

## 輸入文件格式
- 每個文字檔案的第一行可能是標頭（若主要由字母組成）。
- 第二行的第 15 欄必須包含一個數字（`LINE`），用於排序。

### 範例
```
MAG1 SIGNAL1 DEPTH1(m) ALTITUDE1(m) DATE TIME GPS_LON GPS_LAT SHIFT_LON SHIFT_LAT ATARGETS NMAGS GPS_QC GPS_HEIGHT LINE ROUTE GPS-tow(m) Cable(m) EVENT
45328.662000 1150.000 229.593 15.443 08/28/24 19:05:00.935 121.9895671 24.8789984 121.9895358 24.8791095 0 0 7 0.000 88 NO_PLANNED_ROUTE 0.00 12.84 8198
```

## 輸出文件格式
- 輸出檔案名為 `merged_file.txt`，位於指定資料夾中。
- 內容按照 `LINE` 數字排序，第一個檔案的標頭（若有）保留，其餘檔案的標頭被忽略。

### 範例（部分合併後的資料）
```
MAG1 SIGNAL1 DEPTH1(m) ALTITUDE1(m) DATE TIME GPS_LON GPS_LAT SHIFT_LON SHIFT_LAT ATARGETS NMAGS GPS_QC GPS_HEIGHT LINE ROUTE GPS-tow(m) Cable(m) EVENT
45328.662000 1150.000 229.593 15.443 08/28/24 19:05:00.935 121.9895671 24.8789984 121.9895358 24.8791095 0 0 7 0.000 88 NO_PLANNED_ROUTE 0.00 12.84 8198
45328.616000 1147.000 229.934 15.449 08/28/24 19:05:00.951 121.9895666 24.8789985 121.9895358 24.8791095 0 0 7 0.000 88 NO_PLANNED_ROUTE 0.00 12.84 8198
45250.696000 972.000 222.082 12.298 08/28/24 19:15:27.803 121.9828490 24.8732548 121.9827744 24.8733486 0 0 7 0.000 88 NO_PLANNED_ROUTE 0.00 12.84 8216
```

## 注意事項
- 確保資料夾中的檔案至少有兩行，且第二行第 15 欄是數字。
- 路徑中不要有空格，若有空格請用雙引號括住，例如：
  ```cmd
  mergefile.exe "C:\my data\files"
  ```
- 如果程式無法運行，檢查資料夾是否存在或是否有權限。
- 合併完成後，會顯示「檔案已成功合併到 'merged_file.txt'。」訊息。