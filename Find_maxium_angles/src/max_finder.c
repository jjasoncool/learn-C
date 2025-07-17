#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "max_finder.h"

// 靜態函數聲明
static int parse_result_line(const char *line, MaxAngleData *data);
static int compare_angle_data(const void *a, const void *b);

// 解析結果檔案中的單行資料
static int parse_result_line(const char *line, MaxAngleData *data) {
    if (!line || !data) {
        g_printerr("Error: parse_result_line called with NULL parameters\n");
        return 0;
    }

    // 跳過空行和註釋
    if (line[0] == '\0' || line[0] == '#' || line[0] == '\n') {
        return 0;
    }

    // 嘗試解析三個數字
    int parsed = sscanf(line, "%d %d %lf", &data->profile, &data->bin, &data->angle);
    if (parsed != 3) {
        g_printerr("Warning: Failed to parse result line (got %d/3 fields): %.50s\n", parsed, line);
        return 0;
    }

    // 基本有效性檢查
    if (data->profile < 0 || data->bin < 0) {
        g_printerr("Warning: Invalid negative values in result line: %d %d %f\n",
                  data->profile, data->bin, data->angle);
        return 0;
    }

    return 1;
}

// 比較函數，用於找出最大角度差值
static int compare_angle_data(const void *a, const void *b) {
    const MaxAngleData *data_a = (const MaxAngleData *)a;
    const MaxAngleData *data_b = (const MaxAngleData *)b;

    // 先按 profile 分組
    if (data_a->profile != data_b->profile) {
        return data_a->profile - data_b->profile;
    }

    // 同一 profile 內按角度排序
    if (data_a->angle < data_b->angle) return -1;
    if (data_a->angle > data_b->angle) return 1;
    return 0;
}

// 從角度分析結果檔案中找出最大角度差值的一組數據
int find_max_angle_difference(const char *result_file_path, const char *output_file_path) {
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    char line[1024];  // 使用固定大小緩衝區
    MaxAngleData *data_array = NULL;
    int data_count = 0;
    int data_capacity = 0;
    int success = 0;

    if (!result_file_path || !output_file_path) {
        g_printerr("Error: find_max_angle_difference called with NULL parameters\n");
        return 0;
    }

    // 開啟輸入檔案
    input_file = fopen(result_file_path, "r");
    if (!input_file) {
        g_printerr("Error: Failed to open result file '%s': %s\n",
                  result_file_path, strerror(errno));
        goto cleanup;
    }

    // 讀取所有資料
    int line_number = 0;
    while (fgets(line, sizeof(line), input_file) != NULL) {
        line_number++;

        MaxAngleData data;
        if (!parse_result_line(line, &data)) {
            continue;
        }

        // 擴展陣列容量
        if (data_count >= data_capacity) {
            int new_capacity = data_capacity == 0 ? 100 : data_capacity * 2;
            MaxAngleData *new_array = realloc(data_array, new_capacity * sizeof(MaxAngleData));
            if (!new_array) {
                g_printerr("Error: Failed to expand data array to %d items\n", new_capacity);
                goto cleanup;
            }
            data_array = new_array;
            data_capacity = new_capacity;
        }

        data_array[data_count] = data;
        data_count++;
    }

    // 檢查是否是因為錯誤而結束讀取
    if (ferror(input_file)) {
        g_printerr("Error: Error reading result file '%s'\n", result_file_path);
        goto cleanup;
    }

    if (data_count == 0) {
        g_printerr("Warning: No valid data found in result file '%s'\n", result_file_path);
        goto cleanup;
    }

    // 排序資料以便分組處理
    qsort(data_array, data_count, sizeof(MaxAngleData), compare_angle_data);

    // 找出每個 profile 的最大角度差值
    double global_max_diff = 0.0;
    int best_profile = -1;
    double best_min_angle = 0.0, best_max_angle = 0.0;
    int best_min_bin = -1, best_max_bin = -1;

    int i = 0;
    while (i < data_count) {
        int current_profile = data_array[i].profile;
        double min_angle = data_array[i].angle;
        double max_angle = data_array[i].angle;
        int min_bin = data_array[i].bin;
        int max_bin = data_array[i].bin;

        // 處理同一 profile 的所有資料
        while (i < data_count && data_array[i].profile == current_profile) {
            if (data_array[i].angle < min_angle) {
                min_angle = data_array[i].angle;
                min_bin = data_array[i].bin;
            }
            if (data_array[i].angle > max_angle) {
                max_angle = data_array[i].angle;
                max_bin = data_array[i].bin;
            }
            i++;
        }

        // 計算這個 profile 的角度差值
        double angle_diff = max_angle - min_angle;
        if (angle_diff > global_max_diff) {
            global_max_diff = angle_diff;
            best_profile = current_profile;
            best_min_angle = min_angle;
            best_max_angle = max_angle;
            best_min_bin = min_bin;
            best_max_bin = max_bin;
        }
    }

    // 寫入結果檔案
    output_file = fopen(output_file_path, "w");
    if (!output_file) {
        g_printerr("Error: Failed to create output file '%s': %s\n",
                  output_file_path, strerror(errno));
        goto cleanup;
    }

    if (fprintf(output_file, "Maximum Angle Difference Analysis Results\n") < 0 ||
        fprintf(output_file, "=========================================\n") < 0 ||
        fprintf(output_file, "Profile with maximum angle difference: %d\n", best_profile) < 0 ||
        fprintf(output_file, "Angle difference: %.6f\n", global_max_diff) < 0 ||
        fprintf(output_file, "Min angle: %.6f (bin %d)\n", best_min_angle, best_min_bin) < 0 ||
        fprintf(output_file, "Max angle: %.6f (bin %d)\n", best_max_angle, best_max_bin) < 0 ||
        fprintf(output_file, "Bin range: %d ~ %d\n", best_min_bin, best_max_bin) < 0) {
        g_printerr("Error: Failed to write to output file '%s'\n", output_file_path);
        goto cleanup;
    }

    success = 1;

cleanup:
    if (input_file) {
        fclose(input_file);
    }
    if (output_file) {
        fclose(output_file);
    }
    free(data_array);

    return success;
}
