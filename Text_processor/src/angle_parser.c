#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "angle_parser.h"
#include "scan.h"
#include "safe_getline.h"
#include "max_finder.h"
#include "callbacks.h" // 為了存取 AppState 和 is_cancel_requested

// 全局 mutex 保護 hash table 操作
static GMutex angle_parser_mutex;
static gboolean mutex_initialized = FALSE;

// 靜態函數聲明
static int is_result_file(const char *filename);
static int expand_angle_range_array(AngleAnalysisResult *result);
static AngleAnalysisResult init_angle_analysis_result(void);
static int parse_angle_line(const char *line, AngleData *data);
static void update_angle_range(GHashTable *ranges_table, const AngleData *data);
static void ensure_mutex_initialized(void);

// 確保 mutex 初始化
static void ensure_mutex_initialized(void) {
    if (!mutex_initialized) {
        g_mutex_init(&angle_parser_mutex);
        mutex_initialized = TRUE;
    }
}

// 檢查檔案名稱是否為結果檔案
static int is_result_file(const char *filename) {
    if (!filename) return 0;

    // 明確排除輸出檔案
    if (strcmp(filename, "angle_analysis_result.txt") == 0 ||
        strcmp(filename, "max_angle_result.txt") == 0) {
        return 1;
    }

    // 檢查是否包含結果關鍵字
    if (strstr(filename, "result") || strstr(filename, "output") ||
        strstr(filename, "Result") || strstr(filename, "Output")) {
        return 1;
    }
    return 0;
}

// 初始化角度分析結果
static AngleAnalysisResult init_angle_analysis_result(void) {
    AngleAnalysisResult result = {0};
    result.success = 0;
    result.error = NULL;
    return result;
}

// 擴展角度範圍陣列容量
static int expand_angle_range_array(AngleAnalysisResult *result) {
    int new_capacity = result->capacity == 0 ? 10 : result->capacity * 2;
    AngleRange *new_ranges = realloc(result->ranges, new_capacity * sizeof(AngleRange));
    if (!new_ranges) {
        g_printerr("Error: Failed to expand angle range array to %d items\n", new_capacity);
        return 0;
    }
    result->ranges = new_ranges;
    result->capacity = new_capacity;
    return 1;
}

// 解析單行角度資料
static int parse_angle_line(const char *line, AngleData *data) {
    if (!line || !data) {
        g_printerr("Error: parse_angle_line called with NULL parameters\n");
        return 0;
    }

    // 跳過空行和註釋
    if (line[0] == '\0' || line[0] == '#' || line[0] == '\n') {
        return 0;
    }

    // 嘗試解析三個數字
    int parsed = sscanf(line, "%d %d %lf", &data->first_num, &data->second_num, &data->third_num);
    if (parsed == EOF) {
        g_printerr("Warning: EOF encountered while parsing line\n");
        return 0;
    }
    if (parsed != 3) {
        // 詳細錯誤報告
        g_printerr("Warning: Failed to parse line (got %d/3 fields): %.50s\n", parsed, line);
        return 0;
    }

    // 基本有效性檢查
    if (data->first_num < 0 || data->second_num < 0) {
        g_printerr("Warning: Invalid negative values in line: %d %d %f\n",
                  data->first_num, data->second_num, data->third_num);
        return 0;
    }

    // 檢查 NaN 和無限值
    if (!isfinite(data->third_num)) {
        g_printerr("Warning: Invalid angle value (NaN/Inf) in line: %d %d %f\n",
                  data->first_num, data->second_num, data->third_num);
        return 0;
    }

    return 1;
}

// 更新角度範圍表（thread-safe）
static void update_angle_range(GHashTable *ranges_table, const AngleData *data) {
    if (!ranges_table || !data) {
        g_printerr("Error: update_angle_range called with NULL parameters\n");
        return;
    }

    ensure_mutex_initialized();
    g_mutex_lock(&angle_parser_mutex);

    gint *key = g_new(gint, 1);
    if (!key) {
        g_printerr("Error: Failed to allocate memory for hash key\n");
        g_mutex_unlock(&angle_parser_mutex);
        return;
    }
    *key = data->first_num;

    AngleRange *existing = g_hash_table_lookup(ranges_table, key);
    if (!existing) {
        // 新的第一段數字，創建新範圍
        AngleRange *new_range = g_new(AngleRange, 1);
        if (!new_range) {
            g_printerr("Error: Failed to allocate memory for new angle range\n");
            g_free(key);
            g_mutex_unlock(&angle_parser_mutex);
            return;
        }

        new_range->first_num = data->first_num;
        new_range->min_second = new_range->max_second = data->second_num;
        new_range->min_third = new_range->max_third = data->third_num;
        new_range->angle_diff = 0.0;

        g_hash_table_insert(ranges_table, key, new_range);
    } else {
        // 已存在，更新範圍
        g_free(key); // 不需要這個 key

        if (data->second_num < existing->min_second) {
            existing->min_second = data->second_num;
            existing->min_third = data->third_num;
        }
        if (data->second_num > existing->max_second) {
            existing->max_second = data->second_num;
            existing->max_third = data->third_num;
        }

        // 更新角度差值
        existing->angle_diff = fabs(existing->max_third - existing->min_third);
    }

    g_mutex_unlock(&angle_parser_mutex);
}

// 解析單個 TXT 檔案中的角度資料
AngleAnalysisResult parse_angle_file(const char *file_path, void *user_data) {
    AngleAnalysisResult result = init_angle_analysis_result();
    FILE *file = NULL;
    char *line = NULL;
    GHashTable *ranges_table = NULL;
    AsyncProcessData *async_data = (AsyncProcessData *)user_data;
    AppState *state = async_data ? async_data->app_state : NULL;

    if (!file_path) {
        result.error = g_strdup("檔案路徑為空");
        g_printerr("Error: parse_angle_file called with NULL file_path\n");
        goto cleanup;
    }

    file = fopen(file_path, "r");
    if (!file) {
        result.error = g_strdup_printf("無法開啟檔案: %s", file_path);
        g_printerr("Error: Failed to open file '%s': %s\n", file_path, strerror(errno));
        goto cleanup;
    }

    ranges_table = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);
    if (!ranges_table) {
        result.error = g_strdup("無法創建雜湊表");
        g_printerr("Error: Failed to create hash table\n");
        goto cleanup;
    }

    int line_number = 0;
    while ((line = safe_getline(file)) != NULL) {
        line_number++;

        // 每 1000 行檢查一次取消請求
        if (state && line_number % 1000 == 0) {
            if (is_cancel_requested(state)) {
                result.error = g_strdup("操作已取消");
                free(line);
                line = NULL;
                goto cleanup;
            }
        }

        AngleData data;
        if (parse_angle_line(line, &data)) {
            update_angle_range(ranges_table, &data);
        }

        free(line);
        line = NULL;
    }

    // 檢查是否是因為錯誤而結束
    if (ferror(file)) {
        result.error = g_strdup_printf("讀取檔案時發生錯誤: %s", file_path);
        g_printerr("Error: Error reading file '%s'\n", file_path);
        goto cleanup;
    }

    // 將雜湊表資料轉移到結果陣列
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, ranges_table);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (result.count >= result.capacity) {
            if (!expand_angle_range_array(&result)) {
                result.error = g_strdup("記憶體分配失敗");
                break;
            }
        }

        result.ranges[result.count] = *(AngleRange*)value;
        result.count++;
    }

    result.success = (result.error == NULL);

cleanup:
    if (line) {
        free(line);
    }
    if (file) {
        fclose(file);
    }
    if (ranges_table) {
        g_hash_table_destroy(ranges_table);
    }

    return result;
}

// 處理資料夾中的所有 TXT 檔案並分析角度
AngleAnalysisResult process_angle_files(const char *folder_path, const char *output_file, void *user_data) {
    return process_angle_files_with_progress(folder_path, output_file, NULL, user_data);
}

// 處理資料夾中的所有 TXT 檔案並分析角度（帶進度回調）
AngleAnalysisResult process_angle_files_with_progress(const char *folder_path,
                                                     const char *output_file,
                                                     ProgressCallback progress_callback,
                                                     void *user_data) {
    AngleAnalysisResult final_result = init_angle_analysis_result();
    ScanResult scan_result = {0};
    gchar *output_path = NULL;
    FILE *output_file_handle = NULL;

    if (!folder_path || !output_file) {
        final_result.error = g_strdup("資料夾路徑或輸出檔案名稱為空");
        g_printerr("Error: process_angle_files_with_progress called with NULL parameters\n");
        goto cleanup;
    }

    // 掃描 TXT 檔案
    scan_result = scan_txt_files(folder_path);
    if (!scan_result.success) {
        final_result.error = g_strdup_printf("掃描資料夾失敗: %s", scan_result.error ? scan_result.error : "未知錯誤");
        goto cleanup;
    }

    // 直接打開輸出檔案
    output_path = g_build_filename(folder_path, output_file, NULL);
    if (!output_path) {
        final_result.error = g_strdup("無法建構輸出檔案路徑");
        goto cleanup;
    }

    output_file_handle = fopen(output_path, "w");
    if (!output_file_handle) {
        final_result.error = g_strdup_printf("無法創建輸出檔案: %s", output_path);
        goto cleanup;
    }

    // 寫入檔案標題
    fprintf(output_file_handle, "Maximum Angle Difference Analysis Results (Per File)\n");
    fprintf(output_file_handle, "=====================================================\n\n");

    // 處理每個檔案，直接寫入分析結果
    int processed_files = 0;
    AsyncProcessData *async_data = (AsyncProcessData *)user_data;
    AppState *state = async_data ? async_data->app_state : NULL;

    for (int i = 0; i < scan_result.count; i++) {
        // 在處理每個檔案前檢查取消請求
        if (state && is_cancel_requested(state)) {
            final_result.error = g_strdup("操作已取消");
            goto cleanup;
        }

        const char *filename = scan_result.files[i].name;

        // 跳過結果檔案
        if (is_result_file(filename)) {
            continue;
        }

        // 調用進度回調
        if (progress_callback) {
            progress_callback(i + 1, scan_result.count, filename, user_data);
        }

        gchar *file_path = g_build_filename(folder_path, filename, NULL);
        if (!file_path) {
            g_printerr("Error: Failed to build file path for '%s'\n", filename);
            continue;
        }

        AngleAnalysisResult file_result = parse_angle_file(file_path, user_data);

        if (file_result.success && file_result.count > 0) {
            // 直接分析這個檔案的最大角度差值並寫入
            double max_diff = 0.0;
            int best_profile = -1;
            double best_min_angle = 0.0, best_max_angle = 0.0;
            int best_min_bin = -1, best_max_bin = -1;

            // 找出這個檔案的最大角度差值
            for (int j = 0; j < file_result.count; j++) {
                AngleRange *range = &file_result.ranges[j];
                if (range->angle_diff > max_diff) {
                    max_diff = range->angle_diff;
                    best_profile = range->first_num;
                    best_min_angle = range->min_third;
                    best_max_angle = range->max_third;
                    best_min_bin = range->min_second;
                    best_max_bin = range->max_second;
                }
            }

            // 直接寫入分析結果到檔案
            fprintf(output_file_handle, "File: %s\n", filename);
            fprintf(output_file_handle, "Profile with maximum angle difference: %d\n", best_profile);
            fprintf(output_file_handle, "Angle difference: %.6f\n", max_diff);
            fprintf(output_file_handle, "Min angle: %.6f (bin %d)\n", best_min_angle, best_min_bin);
            fprintf(output_file_handle, "Max angle: %.6f (bin %d)\n", best_max_angle, best_max_bin);
            fprintf(output_file_handle, "Bin range: %d ~ %d\n", best_min_bin, best_max_bin);
            fprintf(output_file_handle, "\n");

            processed_files++;
        }

        free_angle_analysis_result(&file_result);
        g_free(file_path);
    }

    final_result.count = processed_files;
    final_result.success = 1;

cleanup:
    if (output_file_handle) {
        fclose(output_file_handle);
    }
    if (output_path) {
        g_free(output_path);
    }
    free_scan_result(&scan_result);

    return final_result;
}

// 釋放角度分析結果的記憶體
void free_angle_analysis_result(AngleAnalysisResult *result) {
    if (!result) return;

    g_free(result->ranges);
    g_free(result->error);

    // 清零避免重複釋放
    memset(result, 0, sizeof(AngleAnalysisResult));
}
