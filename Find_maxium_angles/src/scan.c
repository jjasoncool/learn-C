#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scan.h"

// 靜態函數聲明
static int is_txt_file(const char *filename);
static int expand_file_array(ScanResult *result);
static void add_file_to_result(ScanResult *result, const char *filename, double size_kb);

// 檢查檔案是否為 TXT 檔案（排除結果檔案）
static int is_txt_file(const char *filename) {
    if (!filename) return 0;

    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    // 檢查是否為 TXT 檔案
    if (g_ascii_strcasecmp(ext, ".txt") != 0) {
        return 0;
    }

    // 排除程式產生的結果檔案
    if (g_ascii_strcasecmp(filename, "angle_analysis_result.txt") == 0 ||
        g_ascii_strcasecmp(filename, "max_angle_result.txt") == 0) {
        return 0;
    }

    return 1;
}

// 擴展檔案陣列容量
static int expand_file_array(ScanResult *result) {
    int new_capacity = result->capacity == 0 ? 10 : result->capacity * 2;
    FileInfo *new_files = realloc(result->files, new_capacity * sizeof(FileInfo));
    if (!new_files) {
        g_printerr("Error: Failed to expand file array to %d items\n", new_capacity);
        return 0;
    }
    result->files = new_files;
    result->capacity = new_capacity;
    return 1;
}

// 添加檔案到結果中
static void add_file_to_result(ScanResult *result, const char *filename, double size_kb) {
    if (result->count >= result->capacity) {
        if (!expand_file_array(result)) {
            return; // 擴展失敗
        }
    }

    result->files[result->count].name = g_strdup(filename);
    if (!result->files[result->count].name) {
        g_printerr("Error: Failed to duplicate filename: %s\n", filename);
        return;
    }
    result->files[result->count].size_kb = size_kb;
    result->count++;
}

// 掃描指定資料夾中的所有 TXT 檔案
ScanResult scan_txt_files(const char *folder_path) {
    ScanResult result = {0};
    GDir *dir = NULL;
    GError *error = NULL;

    if (!folder_path) {
        result.error = g_strdup("資料夾路徑為空");
        g_printerr("Error: scan_txt_files called with NULL folder_path\n");
        return result;
    }

    // 開啟資料夾
    dir = g_dir_open(folder_path, 0, &error);
    if (!dir) {
        result.error = g_strdup_printf("無法開啟資料夾: %s",
                                     error ? error->message : "未知錯誤");
        g_printerr("Error: Failed to open directory '%s': %s\n",
                  folder_path, error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        result.success = 0; // 明確設定失敗
        return result;
    }

    const gchar *filename;
    while ((filename = g_dir_read_name(dir)) != NULL) {
        if (!is_txt_file(filename)) {
            continue;
        }

        // 建構完整檔案路徑
        gchar *file_path = g_build_filename(folder_path, filename, NULL);
        if (!file_path) {
            g_printerr("Error: Failed to build file path for '%s'\n", filename);
            continue;
        }

        // 查詢檔案資訊
        GFile *file = g_file_new_for_path(file_path);
        GFileInfo *file_info = g_file_query_info(file,
                                                G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, &error);

        if (!file_info) {
            g_printerr("Error: Failed to query file info for '%s': %s\n",
                      file_path, error ? error->message : "unknown error");
            if (error) {
                g_error_free(error);
                error = NULL;
            }
            g_object_unref(file);
            g_free(file_path);
            continue;
        }

        // 計算檔案大小（KB）
        goffset size_bytes = g_file_info_get_size(file_info);
        double size_kb = (double)size_bytes / 1024.0;

        // 添加到結果中
        add_file_to_result(&result, filename, size_kb);

        // 清理
        g_object_unref(file_info);
        g_object_unref(file);
        g_free(file_path);
    }

    g_dir_close(dir);
    result.success = 1;
    return result;
}

// 格式化掃描結果為 GString
void format_scan_result(const ScanResult *result, const char *folder_path, GString *output_string) {
    if (!output_string) {
        g_printerr("Error: format_scan_result called with NULL output_string\n");
        return;
    }

    g_string_truncate(output_string, 0);

    if (!result) {
        g_string_append(output_string, "掃描結果為空\n");
        return;
    }

    if (!result->success) {
        g_string_append_printf(output_string, "掃描失敗：%s\n",
                              result->error ? result->error : "未知錯誤");
        return;
    }

    g_string_append_printf(output_string, "掃描資料夾：%s\n===========================================\n",
                          folder_path ? folder_path : "未知路徑");

    if (result->count == 0) {
        g_string_append(output_string, "未找到任何 TXT 檔案\n");
        return;
    }

    for (int i = 0; i < result->count; i++) {
        g_string_append_printf(output_string, "%d. %s (%.2f KB)\n",
                              i + 1, result->files[i].name, result->files[i].size_kb);
    }

    g_string_append_printf(output_string, "\n===========================================\n總共找到 %d 個 TXT 檔案",
                          result->count);
}

// 釋放掃描結果的記憶體
void free_scan_result(ScanResult *result) {
    if (!result) return;

    for (int i = 0; i < result->count; i++) {
        g_free(result->files[i].name);
    }
    g_free(result->files);
    g_free(result->error);

    // 清零避免重複釋放
    memset(result, 0, sizeof(ScanResult));
}
