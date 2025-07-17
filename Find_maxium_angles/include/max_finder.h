#ifndef MAX_FINDER_H
#define MAX_FINDER_H

// 最大角度分析資料結構
typedef struct {
    int profile;     // Profile 編號
    int bin;         // Bin 編號
    double angle;    // 角度值
} MaxAngleData;

// 檔案最大角度差值結果結構
typedef struct {
    char *filename;              // 檔案名稱
    int best_profile;            // 最大角度差值的 profile
    double max_diff;             // 最大角度差值
    double min_angle;            // 最小角度
    double max_angle;            // 最大角度
    int min_bin;                 // 最小角度對應的 bin
    int max_bin;                 // 最大角度對應的 bin
} FileMaxAngleResult;

/**
 * 從每個檔案的最大角度差值分析結果中找出全域最大的結果
 * @param analysis_result_file_path 包含每個檔案分析結果的檔案路徑
 * @param output_file_path 輸出檔案路徑
 * @return int 1 成功，0 失敗
 */
int find_global_max_from_analysis_result(const char *analysis_result_file_path, const char *output_file_path);

/**
 * 從資料夾中的所有 TXT 檔案找出全域最大角度值
 * @param folder_path 資料夾路徑
 * @param output_file_path 輸出檔案路徑
 * @return int 1 成功，0 失敗
 */
int find_global_max_angle(const char *folder_path, const char *output_file_path);

/**
 * 從角度分析結果檔案中找出每個檔案的最大角度差值
 * @param result_file_path 角度分析結果檔案路徑
 * @param output_file_path 輸出檔案路徑
 * @return int 1 成功，0 失敗
 */
int find_max_angle_difference_per_file(const char *result_file_path, const char *output_file_path);

/**
 * 從角度分析結果檔案中找出最大角度差值的一組數據（舊版本，保持相容性）
 * @param result_file_path 角度分析結果檔案路徑
 * @param output_file_path 輸出檔案路徑
 * @return int 1 成功，0 失敗
 */
int find_max_angle_difference(const char *result_file_path, const char *output_file_path);

#endif // MAX_FINDER_H
