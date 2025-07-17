#ifndef ANGLE_PARSER_H
#define ANGLE_PARSER_H

#include <gtk/gtk.h>

// 進度回調函數類型定義
typedef void (*ProgressCallback)(int current, int total, const char *filename, void *user_data);

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
 * 處理資料夾中的所有 TXT 檔案並分析角度（帶進度回調）
 * @param folder_path 資料夾路徑
 * @param output_file 輸出結果檔案名稱
 * @param progress_callback 進度回調函數，可為 NULL
 * @param user_data 傳遞給回調函數的用戶資料
 * @return AngleAnalysisResult 整體分析結果
 */
AngleAnalysisResult process_angle_files_with_progress(const char *folder_path, const char *output_file,
                                                     ProgressCallback progress_callback, void *user_data);

/**
 * 釋放角度分析結果的記憶體
 * @param result 要釋放的分析結果
 */
void free_angle_analysis_result(AngleAnalysisResult *result);

#endif // ANGLE_PARSER_H
