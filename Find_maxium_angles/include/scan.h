#ifndef SCAN_H
#define SCAN_H

#include <gtk/gtk.h>

// 檔案資訊結構
typedef struct {
    char *name;
    double size_kb;
} FileInfo;

// 掃描結果結構
typedef struct {
    FileInfo *files;
    int count;
    int capacity;
    char *error;
    int success;
} ScanResult;

/**
 * 掃描指定資料夾中的所有 TXT 檔案
 * @param folder_path 要掃描的資料夾路徑
 * @return ScanResult 掃描結果，包含檔案列表和統計資訊
 */
ScanResult scan_txt_files(const char *folder_path);

/**
 * 格式化掃描結果為 GString
 * @param result 掃描結果
 * @param folder_path 資料夾路徑
 * @param output_string 輸出的 GString
 */
void format_scan_result(const ScanResult *result, const char *folder_path, GString *output_string);

/**
 * 釋放掃描結果的記憶體
 * @param result 要釋放的掃描結果
 */
void free_scan_result(ScanResult *result);

#endif // SCAN_H
