#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include <gtk/gtk.h>  // 既然用 GTK，包含 glib 相關

// 檔案資訊結構
typedef struct {
    char *name;  // 動態分配，別固定大小
    double size_kb;
} FileInfo;

// 掃描結果結構
typedef struct {
    FileInfo *files;
    int count;
    int capacity;
    char *error;  // 動態分配的錯誤訊息
    int success;
} ScanResult;

// 角度資料點結構
typedef struct {
    int first_num;    // 第一段數字 (例如 214)
    int second_num;   // 第二段數字 (例如 781-800)
    double third_num; // 第三段數字 (例如 15.84-16.65)
} AngleData;

// 角度範圍結構
typedef struct {
    int first_num;           // 第一段數字
    int min_second;          // 第二段最小值
    int max_second;          // 第二段最大值
    double min_third;        // 對應第三段最小值
    double max_third;        // 對應第三段最大值
    double angle_diff;       // 角度差值 (max_third - min_third 的絕對值)
} AngleRange;

// 角度分析結果結構
typedef struct {
    AngleRange *ranges;      // 角度範圍陣列
    int count;               // 範圍數量
    int capacity;            // 陣列容量
    char *error;             // 錯誤訊息
    int success;             // 成功標誌
} AngleAnalysisResult;

/**
 * 掃描指定資料夾中的所有 TXT 檔案
 * @param folder_path 要掃描的資料夾路徑
 * @return ScanResult 掃描結果，包含檔案列表和統計資訊
 */
ScanResult scan_txt_files(const char *folder_path);

/**
 * 將掃描結果格式化為顯示字串
 * @param result 掃描結果
 * @param folder_path 資料夾路徑
 * @param buffer 輸出緩衝區
 * @param buffer_size 緩衝區大小
 */
void format_scan_result(const ScanResult *result, const char *folder_path, char *buffer, size_t buffer_size);

/**
 * 將掃描結果格式化為 GString（更安全的動態字串版本）
 * @param result 掃描結果
 * @param folder_path 資料夾路徑
 * @param output_string 輸出的 GString
 */
void format_scan_result_gstring(const ScanResult *result, const char *folder_path, GString *output_string);

/**
 * 釋放掃描結果的記憶體
 * @param result 要釋放的掃描結果
 */
void free_scan_result(ScanResult *result);

/**
 * 檢查檔案是否為 TXT 檔案（基於副檔名）
 * @param filename 檔案名稱
 * @return 1 如果是 TXT 檔案，0 如果不是
 */
int is_txt_file(const char *filename);

/**
 * 檢查檔案名稱是否為結果檔案（避免重複解析）
 * @param filename 檔案名稱
 * @return 1 如果是結果檔案，0 如果不是
 */
int is_result_file(const char *filename);

/**
 * 解析單個 TXT 檔案中的角度資料
 * @param file_path 檔案路徑
 * @return AngleAnalysisResult 分析結果
 */
AngleAnalysisResult parse_angle_file(const char *file_path);

/**
 * 處理資料夾中的所有 TXT 檔案並分析角度
 * @param folder_path 資料夾路徑
 * @param output_file 輸出結果檔案名稱
 * @return AngleAnalysisResult 整體分析結果
 */
AngleAnalysisResult process_angle_files(const char *folder_path, const char *output_file);

/**
 * 釋放角度分析結果的記憶體
 * @param result 要釋放的分析結果
 */
void free_angle_analysis_result(AngleAnalysisResult *result);

/**
 * 從角度分析結果檔案中找出最大角度差值的一組數據
 * @param result_file_path 角度分析結果檔案路徑
 * @param output_file_path 輸出檔案路徑
 * @return int 1 成功，0 失敗
 */
int find_max_angle_difference(const char *result_file_path, const char *output_file_path);

#endif // FILE_PROCESSOR_H
