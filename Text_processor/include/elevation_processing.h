// 高程轉換處理模組頭文件

#ifndef ELEVATION_PROCESSING_H
#define ELEVATION_PROCESSING_H

#include <glib.h>

// 简化的进度更新回调函数类型（避免与 angle_parser.h 冲突）
typedef void (*ElevationProgressCallback)(double progress, const char *message);

/**
 * 處理高程轉換的核心函數
 *
 * @param xyz_path XYZ坐標點雲文件的路徑
 * @param sep_path SEP參數文件的路徑
 * @param result_text 用於存儲處理結果的GString
 * @param error 如果發生錯誤，會設置錯誤信息
 * @param progress_bar 可選的進度條控件，用於顯示處理進度
 *
 * @return TRUE 如果處理成功，FALSE 如果發生錯誤
 */
gboolean process_elevation_conversion(const char *xyz_path, const char *sep_path, GString *result_text, GError **error, GtkProgressBar *progress_bar);

/**
 * 帶進度回調的高程轉換處理函數（線程安全）
 *
 * @param xyz_path 文件路徑
 * @param sep_path SEP參數文件路徑
 * @param result_text 結果字符串
 * @param error 錯誤信息
 * @param progress_callback 進度更新回调函数
 *
 * @return TRUE 如果處理成功，FALSE 如果發生錯誤
 */
gboolean process_elevation_conversion_with_callback(const char *xyz_path, const char *sep_path, GString *result_text, GError **error, ElevationProgressCallback progress_callback);

#endif // ELEVATION_PROCESSING_H
