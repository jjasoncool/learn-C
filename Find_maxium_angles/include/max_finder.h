#ifndef MAX_FINDER_H
#define MAX_FINDER_H

// 最大角度分析資料結構
typedef struct {
    int profile;     // Profile 編號
    int bin;         // Bin 編號
    double angle;    // 角度值
} MaxAngleData;

/**
 * 從角度分析結果檔案中找出最大角度差值的一組數據
 * @param result_file_path 角度分析結果檔案路徑
 * @param output_file_path 輸出檔案路徑
 * @return int 1 成功，0 失敗
 */
int find_max_angle_difference(const char *result_file_path, const char *output_file_path);

#endif // MAX_FINDER_H
