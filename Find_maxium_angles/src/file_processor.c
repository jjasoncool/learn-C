#include "file_processor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// 初始化掃描結果
static ScanResult init_scan_result() {
    ScanResult result;
    result.files = NULL;
    result.count = 0;
    result.capacity = 0;
    result.success = 0;
    memset(result.error_message, 0, sizeof(result.error_message));
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
    if (filename == NULL) return 0;

    const char *ext = strrchr(filename, '.');
    if (ext == NULL) return 0;

    return (strcmp(ext, ".txt") == 0 || strcmp(ext, ".TXT") == 0);
}

ScanResult scan_txt_files(const char *folder_path) {
    ScanResult result = init_scan_result();

    if (folder_path == NULL) {
        strcpy(result.error_message, "資料夾路徑為空");
        return result;
    }

    DIR *dir = opendir(folder_path);
    if (dir == NULL) {
        snprintf(result.error_message, sizeof(result.error_message),
                "無法開啟資料夾: %s", folder_path);
        return result;
    }

    struct dirent *entry;
    struct stat file_stat;

    while ((entry = readdir(dir)) != NULL) {
        // 跳過目錄項 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 建構完整路徑
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s\\%s", folder_path, entry->d_name);

        // 獲取檔案資訊
        if (stat(full_path, &file_stat) == 0) {
            // 檢查是否為一般檔案且為 TXT 檔案
            if (S_ISREG(file_stat.st_mode) && is_txt_file(entry->d_name)) {
                // 需要擴展陣列嗎？
                if (result.count >= result.capacity) {
                    if (!expand_file_array(&result)) {
                        strcpy(result.error_message, "記憶體分配失敗");
                        closedir(dir);
                        return result;
                    }
                }

                // 添加檔案資訊
                FileInfo *file_info = &result.files[result.count];
                strncpy(file_info->name, entry->d_name, sizeof(file_info->name) - 1);
                file_info->name[sizeof(file_info->name) - 1] = '\0';
                file_info->size_kb = (double)file_stat.st_size / 1024.0;

                result.count++;
            }
        }
    }

    closedir(dir);
    result.success = 1;
    return result;
}

void format_scan_result(const ScanResult *result, const char *folder_path, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) return;

    if (!result->success) {
        snprintf(buffer, buffer_size,
                "掃描失敗\n錯誤: %s", result->error_message);
        return;
    }

    if (result->count == 0) {
        snprintf(buffer, buffer_size,
                "掃描資料夾: %s\n\n未找到任何 TXT 檔案。\n\n"
                "請確認:\n"
                "1. 資料夾中包含 .txt 或 .TXT 檔案\n"
                "2. 檔案不在子資料夾中（目前只掃描當前目錄）",
                folder_path);
        return;
    }

    // 開始構建結果字串
    int written = snprintf(buffer, buffer_size,
            "掃描資料夾: %s\n\n找到的 TXT 檔案:\n"
            "===========================================\n",
            folder_path);

    if (written >= (int)buffer_size) return; // 緩衝區已滿

    // 添加檔案列表
    for (int i = 0; i < result->count && written < (int)buffer_size - 100; i++) {
        int line_written = snprintf(buffer + written, buffer_size - written,
                "%d. %s (%.2f KB)\n",
                i + 1, result->files[i].name, result->files[i].size_kb);

        if (line_written > 0) {
            written += line_written;
        } else {
            break; // 緩衝區空間不足
        }
    }

    // 添加總結
    if (written < (int)buffer_size - 100) {
        snprintf(buffer + written, buffer_size - written,
                "\n===========================================\n"
                "總共找到 %d 個 TXT 檔案", result->count);
    }
}

void free_scan_result(ScanResult *result) {
    if (result != NULL && result->files != NULL) {
        free(result->files);
        result->files = NULL;
        result->count = 0;
        result->capacity = 0;
    }
}
