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

// ===========================================
// 角度分析功能
// ===========================================

#include <math.h>

// 檢查檔案名稱是否為結果檔案
int is_result_file(const char *filename) {
    if (!filename) return 0;

    // 明確排除兩個成果輸出檔案
    if (strcmp(filename, "angle_analysis_result.txt") == 0 ||
        strcmp(filename, "max_angle_result.txt") == 0) {
        return 1;
    }

    // 檢查是否包含 "result" 或 "output" 關鍵字
    if (strstr(filename, "result") || strstr(filename, "output") ||
        strstr(filename, "Result") || strstr(filename, "Output")) {
        return 1;
    }
    return 0;
}// 初始化角度分析結果
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
    if (new_ranges == NULL) {
        return 0;
    }
    result->ranges = new_ranges;
    result->capacity = new_capacity;
    return 1;
}

// 解析一行資料
static int parse_line(const char *line, AngleData *data) {
    if (!line || !data) return 0;

    // 跳過空白行和註解
    if (line[0] == '\0' || line[0] == '#' || line[0] == '\n') {
        return 0;
    }

    // 嘗試解析三個數字
    int result = sscanf(line, "%d %d %lf", &data->first_num, &data->second_num, &data->third_num);
    return result == 3;
}

// 解析單個 TXT 檔案
AngleAnalysisResult parse_angle_file(const char *file_path) {
    AngleAnalysisResult result = init_angle_analysis_result();

    if (!file_path) {
        result.error = strdup("檔案路徑為空");
        return result;
    }

    FILE *file = fopen(file_path, "r");
    if (!file) {
        result.error = g_strdup_printf("無法開啟檔案: %s", file_path);
        return result;
    }

    // 使用 GHashTable 來組織資料（以第一段數字為鍵）
    GHashTable *data_groups = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, (GDestroyNotify)g_ptr_array_unref);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        AngleData data;
        if (parse_line(line, &data)) {
            // 找到或創建對應第一段數字的陣列
            gint *key = g_new(gint, 1);
            *key = data.first_num;

            GPtrArray *group = g_hash_table_lookup(data_groups, key);
            if (!group) {
                group = g_ptr_array_new_with_free_func(g_free);
                g_hash_table_insert(data_groups, key, group);
            } else {
                g_free(key); // 如果已存在，釋放新建的 key
            }

            // 添加資料點到組中
            AngleData *new_data = g_new(AngleData, 1);
            *new_data = data;
            g_ptr_array_add(group, new_data);
        }
    }

    fclose(file);

    // 處理每個組，計算最大最小值
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, data_groups);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        int first_num = *(int*)key;
        GPtrArray *group = (GPtrArray*)value;

        if (group->len == 0) continue;

        // 確保陣列有足夠容量
        if (result.count >= result.capacity) {
            if (!expand_angle_range_array(&result)) {
                result.error = strdup("記憶體分配失敗");
                g_hash_table_destroy(data_groups);
                return result;
            }
        }

        AngleRange *range = &result.ranges[result.count];
        range->first_num = first_num;

        // 初始化最大最小值
        AngleData *first_data = (AngleData*)g_ptr_array_index(group, 0);
        range->min_second = first_data->second_num;
        range->max_second = first_data->second_num;
        range->min_third = first_data->third_num;
        range->max_third = first_data->third_num;

        // 找到最大最小值
        for (guint i = 1; i < group->len; i++) {
            AngleData *current = (AngleData*)g_ptr_array_index(group, i);

            if (current->second_num < range->min_second) {
                range->min_second = current->second_num;
                range->min_third = current->third_num;
            }
            if (current->second_num > range->max_second) {
                range->max_second = current->second_num;
                range->max_third = current->third_num;
            }
        }

        // 計算角度差值的絕對值
        range->angle_diff = fabs(range->max_third - range->min_third);
        result.count++;
    }

    g_hash_table_destroy(data_groups);
    result.success = 1;
    return result;
}

// 處理資料夾中的所有 TXT 檔案
AngleAnalysisResult process_angle_files(const char *folder_path, const char *output_file) {
    AngleAnalysisResult final_result = init_angle_analysis_result();

    if (!folder_path || !output_file) {
        final_result.error = strdup("資料夾路徑或輸出檔案名稱為空");
        return final_result;
    }

    // 掃描 TXT 檔案
    ScanResult scan_result = scan_txt_files(folder_path);
    if (!scan_result.success) {
        final_result.error = g_strdup_printf("掃描資料夾失敗: %s", scan_result.error);
        free_scan_result(&scan_result);
        return final_result;
    }

    // 使用 GHashTable 來累積所有檔案的結果
    GHashTable *all_ranges = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);

    // 處理每個檔案
    for (int i = 0; i < scan_result.count; i++) {
        const char *filename = scan_result.files[i].name;

        // 跳過結果檔案
        if (is_result_file(filename)) {
            continue;
        }

        gchar *file_path = g_build_filename(folder_path, filename, NULL);
        AngleAnalysisResult file_result = parse_angle_file(file_path);

        if (file_result.success) {
            // 合併結果到總表中
            for (int j = 0; j < file_result.count; j++) {
                AngleRange *range = &file_result.ranges[j];
                gint *key = g_new(gint, 1);
                *key = range->first_num;

                AngleRange *existing = g_hash_table_lookup(all_ranges, key);
                if (!existing) {
                    // 新的第一段數字，直接添加
                    AngleRange *new_range = g_new(AngleRange, 1);
                    *new_range = *range;
                    g_hash_table_insert(all_ranges, key, new_range);
                } else {
                    // 已存在，更新最大值和最小值
                    g_free(key);
                    if (range->angle_diff > existing->angle_diff) {
                        *existing = *range; // 保留差值更大的範圍
                    }
                }
            }
        }

        free_angle_analysis_result(&file_result);
        g_free(file_path);
    }

    // 將 HashTable 的資料轉移到結果陣列中
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, all_ranges);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (final_result.count >= final_result.capacity) {
            if (!expand_angle_range_array(&final_result)) {
                final_result.error = strdup("記憶體分配失敗");
                break;
            }
        }

        final_result.ranges[final_result.count] = *(AngleRange*)value;
        final_result.count++;
    }

    // 寫入結果檔案
    if (final_result.count > 0) {
        gchar *output_path = g_build_filename(folder_path, output_file, NULL);
        FILE *out_file = fopen(output_path, "w");

        if (out_file) {
            // 寫入標題行
            fprintf(out_file, "profile angle_difference\n");

            for (int i = 0; i < final_result.count; i++) {
                AngleRange *range = &final_result.ranges[i];
                fprintf(out_file, "%d %.2f\n", range->first_num, range->angle_diff);
            }
            fclose(out_file);
        } else {
            final_result.error = g_strdup_printf("無法建立輸出檔案: %s", output_path);
        }

        g_free(output_path);
    }

    g_hash_table_destroy(all_ranges);
    free_scan_result(&scan_result);

    if (!final_result.error) {
        final_result.success = 1;
    }

    return final_result;
}

// 釋放角度分析結果記憶體
void free_angle_analysis_result(AngleAnalysisResult *result) {
    if (result) {
        free(result->ranges);
        free(result->error);
        result->ranges = NULL;
        result->count = 0;
        result->capacity = 0;
        result->error = NULL;
    }
}

// 從角度分析結果檔案中找出最大角度差值的一組數據
int find_max_angle_difference(const char *result_file_path, const char *output_file_path) {
    if (!result_file_path || !output_file_path) {
        return 0;
    }

    FILE *input_file = fopen(result_file_path, "r");
    if (!input_file) {
        return 0;
    }

    int max_first_num = 0;
    double max_angle_diff = 0.0;
    int first_num;
    double angle_diff;

    char line[256];
    while (fgets(line, sizeof(line), input_file)) {
        // 解析每一行: "數字 角度差值"
        if (sscanf(line, "%d %lf", &first_num, &angle_diff) == 2) {
            if (angle_diff > max_angle_diff) {
                max_angle_diff = angle_diff;
                max_first_num = first_num;
            }
        }
    }

    fclose(input_file);

    // 如果找到最大值，寫入輸出檔案
    if (max_angle_diff > 0.0) {
        FILE *output_file = fopen(output_file_path, "w");
        if (!output_file) {
            return 0;
        }

        fprintf(output_file, "maximum angle:\n");
        fprintf(output_file, "===========================================\n");
        fprintf(output_file, "profile: %d\n", max_first_num);
        fprintf(output_file, "angle: %.2f\n", max_angle_diff);

        fclose(output_file);
        return 1;
    }

    return 0;
}
