#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "max_finder.h"
#include "safe_getline.h"

// 從每個檔案的最大角度差值分析結果中找出全域最大的結果
int find_global_max_from_analysis_result(const char *analysis_result_file_path, const char *output_file_path) {
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    char *line = NULL;
    char *best_filename = NULL;
    int best_profile = -1;
    double best_max_diff = 0.0;
    char *current_filename = NULL;
    int current_profile = -1;
    int success = 0;

    if (!analysis_result_file_path || !output_file_path) {
        g_printerr("Error: find_global_max_from_analysis_result called with NULL parameters\n");
        return 0;
    }

    // 開啟輸入檔案
    input_file = fopen(analysis_result_file_path, "r");
    if (!input_file) {
        g_printerr("Error: Failed to open analysis result file '%s': %s\n",
                  analysis_result_file_path, strerror(errno));
        goto cleanup;
    }

    // 解析檔案內容，找出最大角度差值
    while ((line = safe_getline(input_file)) != NULL) {
        // 檢查是否是檔案行
        if (strncmp(line, "File: ", 6) == 0) {
            free(current_filename);
            current_filename = strdup(line + 6);
            if (current_filename) {
                // 移除行尾的換行符
                size_t len = strlen(current_filename);
                while (len > 0 && (current_filename[len-1] == '\n' || current_filename[len-1] == '\r')) {
                    current_filename[len-1] = '\0';
                    len--;
                }
            }
            current_profile = -1; // 重設 profile
        }
        // 檢查是否是 profile 行
        else if (strncmp(line, "Profile with maximum angle difference: ", 39) == 0) {
            if (sscanf(line, "Profile with maximum angle difference: %d", &current_profile) != 1) {
                current_profile = -1;
            }
        }
        // 檢查是否是角度差值行
        else if (strncmp(line, "Angle difference: ", 18) == 0 && current_filename && current_profile != -1) {
            double angle_diff;
            if (sscanf(line, "Angle difference: %lf", &angle_diff) == 1) {
                if (angle_diff > best_max_diff) {
                    best_max_diff = angle_diff;
                    best_profile = current_profile;
                    free(best_filename);
                    best_filename = strdup(current_filename);
                }
            }
        }

        free(line);
        line = NULL;
    }

    // 檢查是否是因為錯誤而結束讀取
    if (ferror(input_file)) {
        g_printerr("Error: Error reading analysis result file '%s'\n", analysis_result_file_path);
        goto cleanup;
    }

    if (!best_filename) {
        g_printerr("Warning: No file results found in analysis result file '%s'\n", analysis_result_file_path);
        goto cleanup;
    }

    // 寫入結果檔案
    output_file = fopen(output_file_path, "w");
    if (!output_file) {
        g_printerr("Error: Failed to create output file '%s': %s\n",
                  output_file_path, strerror(errno));
        goto cleanup;
    }

    fprintf(output_file, "Global Maximum Angle Difference Analysis Result\n");
    fprintf(output_file, "===============================================\n");
    fprintf(output_file, "File with maximum angle difference: %s\n", best_filename);
    fprintf(output_file, "Profile with maximum angle difference: %d\n", best_profile);
    fprintf(output_file, "Maximum angle difference: %.6f\n", best_max_diff);

    success = 1;

cleanup:
    if (line) {
        free(line);
    }
    free(current_filename);
    free(best_filename);
    if (input_file) {
        fclose(input_file);
    }
    if (output_file) {
        fclose(output_file);
    }

    return success;
}
