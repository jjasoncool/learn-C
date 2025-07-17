#include "file_processor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>  // 用 glib 的 GDir 和 g_build_filename，GTK 依賴它

// 初始化掃描結果
static ScanResult init_scan_result(void) {
    ScanResult result = {0};  // C99 初始化，簡單可靠
    result.success = 0;
    result.error = NULL;
    return result;
}

// 擴展檔案陣列容量
static int expand_file_array(ScanResult *result) {
    int new_capacity = result->capacity == 0 ? 10 : result->capacity * 2;
    FileInfo *new_files = realloc(result->files, new_capacity * sizeof(FileInfo));
    if (new_files == NULL) {
        return 0; // 記憶體分配失敗
    }
    result->files = new_files;
    result->capacity = new_capacity;
    return 1;
}

int is_txt_file(const char *filename) {
    if (!filename) return 0;
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    return strcasecmp(ext, ".txt") == 0;  // case insensitive，用 strcasecmp
}

ScanResult scan_txt_files(const char *folder_path) {
    ScanResult result = init_scan_result();
    if (!folder_path) {
        result.error = strdup("資料夾路徑為空");
        return result;
    }

    GDir *dir = g_dir_open(folder_path, 0, NULL);
    if (!dir) {
        result.error = g_strdup_printf("無法開啟資料夾: %s", folder_path);
        return result;
    }

    const gchar *filename;
    while ((filename = g_dir_read_name(dir)) != NULL) {
        if (g_strcmp0(filename, ".") == 0 || g_strcmp0(filename, "..") == 0) {
            continue;
        }

        gchar *full_path = g_build_filename(folder_path, filename, NULL);
        GFile *file = g_file_new_for_path(full_path);
        GError *error = NULL;
        GFileInfo *info = g_file_query_info(file,
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                           G_FILE_QUERY_INFO_NONE,
                                           NULL,
                                           &error);

        if (info && !error) {
            if (g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
                g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR &&
                is_txt_file(filename)) {
                if (result.count >= result.capacity) {
                    if (!expand_file_array(&result)) {
                        result.error = strdup("記憶體分配失敗");
                        g_object_unref(info);
                        g_object_unref(file);
                        g_free(full_path);
                        g_dir_close(dir);
                        return result;
                    }
                }

                FileInfo *file_info = &result.files[result.count];
                file_info->name = strdup(filename);  // 動態

                // 安全地獲取檔案大小
                if (g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
                    file_info->size_kb = (double)g_file_info_get_size(info) / 1024.0;
                } else {
                    file_info->size_kb = 0.0;  // 無法獲取大小時設為 0
                }

                result.count++;
            }
            g_object_unref(info);
        } else {
            // 處理錯誤情況
            if (error) {
                g_error_free(error);
            }
        }

        g_object_unref(file);
        g_free(full_path);
    }

    g_dir_close(dir);
    result.success = 1;
    return result;
}

void format_scan_result(const ScanResult *result, const char *folder_path, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return;

    if (!result->success) {
        snprintf(buffer, buffer_size, "掃描失敗\n錯誤: %s", result->error ? result->error : "未知錯誤");
        return;
    }

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "掃描資料夾: %s\n\n找到的 TXT 檔案:\n===========================================\n", folder_path);

    for (int i = 0; i < result->count && offset < buffer_size - 1; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, "%d. %s (%.2f KB)\n", i + 1, result->files[i].name, result->files[i].size_kb);
    }

    if (offset < buffer_size - 1) {
        snprintf(buffer + offset, buffer_size - offset, "\n===========================================\n總共找到 %d 個 TXT 檔案", result->count);
    }
}

void free_scan_result(ScanResult *result) {
    if (result) {
        for (int i = 0; i < result->count; i++) {
            free(result->files[i].name);
        }
        free(result->files);
        free(result->error);
        result->files = NULL;
        result->count = 0;
        result->capacity = 0;
        result->error = NULL;
    }
}

void format_scan_result_gstring(const ScanResult *result, const char *folder_path, GString *output_string) {
    if (!output_string) return;
    g_string_truncate(output_string, 0);

    if (!result->success) {
        g_string_append_printf(output_string, "掃描失敗：%s\n", result->error ? result->error : "未知錯誤");
        return;
    }

    g_string_append_printf(output_string, "掃描資料夾：%s\n===========================================\n", folder_path);

    if (result->count == 0) {
        g_string_append(output_string, "未找到任何 TXT 檔案\n");
        return;
    }

    for (int i = 0; i < result->count; i++) {
        g_string_append_printf(output_string, "%d. %s (%.2f KB)\n", i + 1, result->files[i].name, result->files[i].size_kb);
    }

    g_string_append_printf(output_string, "\n===========================================\n總共找到 %d 個 TXT 檔案", result->count);
}
