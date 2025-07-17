#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "angle_parser.h"
#include "scan.h"

// 靜態函數聲明
static int is_result_file(const char *filename);
static int expand_angle_range_array(AngleAnalysisResult *result);
static AngleAnalysisResult init_angle_analysis_result(void);
static int parse_angle_line(const char *line, AngleData *data);
static void update_angle_range(GHashTable *ranges_table, const AngleData *data);

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

    return 1;
}

// 更新角度範圍表
static void update_angle_range(GHashTable *ranges_table, const AngleData *data) {
    if (!ranges_table || !data) {
        g_printerr("Error: update_angle_range called with NULL parameters\n");
        return;
    }

    gint *key = g_new(gint, 1);
    if (!key) {
        g_printerr("Error: Failed to allocate memory for hash key\n");
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
}

// 解析單個 TXT 檔案中的角度資料
AngleAnalysisResult parse_angle_file(const char *file_path) {
    AngleAnalysisResult result = init_angle_analysis_result();
    FILE *file = NULL;
    char line[1024];  // 使用固定大小緩衝區

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

    GHashTable *ranges_table = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);
    if (!ranges_table) {
        result.error = g_strdup("無法創建雜湊表");
        g_printerr("Error: Failed to create hash table\n");
        goto cleanup;
    }

    int line_number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        AngleData data;
        if (parse_angle_line(line, &data)) {
            update_angle_range(ranges_table, &data);
        }
    }

    // 檢查是否是因為錯誤而結束
    if (ferror(file)) {
        result.error = g_strdup_printf("讀取檔案時發生錯誤: %s", file_path);
        g_printerr("Error: Error reading file '%s'\n", file_path);
        g_hash_table_destroy(ranges_table);
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

    g_hash_table_destroy(ranges_table);
    result.success = (result.error == NULL);

cleanup:
    if (file) {
        fclose(file);
    }

    return result;
}

// 處理資料夾中的所有 TXT 檔案並分析角度
AngleAnalysisResult process_angle_files(const char *folder_path, const char *output_file) {
    return process_angle_files_with_progress(folder_path, output_file, NULL, NULL);
}

// 處理資料夾中的所有 TXT 檔案並分析角度（帶進度回調）
AngleAnalysisResult process_angle_files_with_progress(const char *folder_path,
                                                     const char *output_file,
                                                     ProgressCallback progress_callback,
                                                     void *user_data) {
    AngleAnalysisResult final_result = init_angle_analysis_result();
    ScanResult scan_result = {0};
    GHashTable *all_ranges = NULL;
    FILE *output = NULL;

    if (!folder_path || !output_file) {
        final_result.error = g_strdup("資料夾路徑或輸出檔案名稱為空");
        g_printerr("Error: process_angle_files_with_progress called with NULL parameters\n");
        goto cleanup;
    }

    // 掃描 TXT 檔案
    scan_result = scan_txt_files(folder_path);
    if (!scan_result.success) {
        final_result.error = g_strdup_printf("掃描資料夾失敗: %s", scan_result.error);
        goto cleanup;
    }

    // 創建雜湊表來累積所有檔案的結果
    all_ranges = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);
    if (!all_ranges) {
        final_result.error = g_strdup("無法創建雜湊表");
        g_printerr("Error: Failed to create hash table for all ranges\n");
        goto cleanup;
    }

    // 處理每個檔案
    for (int i = 0; i < scan_result.count; i++) {
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

        AngleAnalysisResult file_result = parse_angle_file(file_path);

        if (file_result.success) {
            // 合併結果到總表中
            for (int j = 0; j < file_result.count; j++) {
                AngleRange *range = &file_result.ranges[j];
                gint *key = g_new(gint, 1);
                if (!key) {
                    g_printerr("Error: Failed to allocate memory for hash key\n");
                    free_angle_analysis_result(&file_result);
                    g_free(file_path);
                    goto cleanup;
                }
                *key = range->first_num;

                AngleRange *existing = g_hash_table_lookup(all_ranges, key);
                if (!existing) {
                    // 新的第一段數字，直接添加
                    AngleRange *new_range = g_new(AngleRange, 1);
                    if (!new_range) {
                        g_printerr("Error: Failed to allocate memory for new range\n");
                        g_free(key);
                        free_angle_analysis_result(&file_result);
                        g_free(file_path);
                        goto cleanup;
                    }
                    *new_range = *range;
                    g_hash_table_insert(all_ranges, key, new_range);
                } else {
                    // 已存在，更新最大差值的範圍
                    g_free(key);
                    if (range->angle_diff > existing->angle_diff) {
                        *existing = *range;
                    }
                }
            }
        } else {
            g_printerr("Warning: Failed to parse file '%s': %s\n",
                      file_path, file_result.error ? file_result.error : "unknown error");
        }

        free_angle_analysis_result(&file_result);
        g_free(file_path);
    }

    // 將雜湊表的資料轉移到結果陣列中
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, all_ranges);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (final_result.count >= final_result.capacity) {
            if (!expand_angle_range_array(&final_result)) {
                final_result.error = g_strdup("記憶體分配失敗");
                goto cleanup;
            }
        }

        final_result.ranges[final_result.count] = *(AngleRange*)value;
        final_result.count++;
    }

    // 寫入結果檔案
    gchar *output_path = g_build_filename(folder_path, output_file, NULL);
    if (!output_path) {
        final_result.error = g_strdup("無法建構輸出檔案路徑");
        g_printerr("Error: Failed to build output file path\n");
        goto cleanup;
    }

    output = fopen(output_path, "w");
    if (!output) {
        final_result.error = g_strdup_printf("無法創建輸出檔案: %s", output_path);
        g_printerr("Error: Failed to create output file '%s': %s\n", output_path, strerror(errno));
        g_free(output_path);
        goto cleanup;
    }

    for (int i = 0; i < final_result.count; i++) {
        AngleRange *range = &final_result.ranges[i];
        if (fprintf(output, "%d %d %.6f\n", range->first_num, range->min_second, range->min_third) < 0 ||
            fprintf(output, "%d %d %.6f\n", range->first_num, range->max_second, range->max_third) < 0) {
            final_result.error = g_strdup("寫入輸出檔案失敗");
            g_printerr("Error: Failed to write to output file '%s'\n", output_path);
            g_free(output_path);
            goto cleanup;
        }
    }

    g_free(output_path);
    final_result.success = 1;

cleanup:
    if (output) {
        fclose(output);
    }
    if (all_ranges) {
        g_hash_table_destroy(all_ranges);
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
