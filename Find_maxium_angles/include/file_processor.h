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

#endif // FILE_PROCESSOR_H
