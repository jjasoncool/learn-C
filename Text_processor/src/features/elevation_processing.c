// 高程轉換處理模組
// 負責處理7欄文字文件和SEP對照文件的高程轉換邏輯

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../include/callbacks.h"  // 引入 TideDataRow 和 parse_tide_data_row

// SEP對照資料結構
typedef struct SepEntry {
    double longitude;   // 經度
    double latitude;    // 緯度
    double adjustment;  // 調整值
    struct SepEntry *next; // 鏈表下一節點
} SepEntry;

// 簡易雜湊表實現，用於經緯度查找
#define SEP_HASH_SIZE 8192

typedef struct SepHashTable {
    SepEntry **buckets;
    int size;
    int count;
} SepHashTable;

// 簡易雜湊函數
static unsigned int hash_double_double(double d1, double d2) {
    // 將兩個double轉為雜湊值
    union {
        double d;
        unsigned int u[2];
    } conv1 = {d1}, conv2 = {d2};

    return (conv1.u[0] ^ conv1.u[1] ^ conv2.u[0] ^ conv2.u[1]) % SEP_HASH_SIZE;
}

// 初始化雜湊表
static SepHashTable* sep_hash_init(int size) {
    SepHashTable *table = g_new(SepHashTable, 1);
    table->size = size;
    table->count = 0;
    table->buckets = g_new0(SepEntry*, size);
    return table;
}

// 釋放雜湊表
static void sep_hash_free(SepHashTable *table) {
    if (!table) return;

    for (int i = 0; i < table->size; i++) {
        g_free(table->buckets[i]);
    }
    g_free(table->buckets);
    g_free(table);
}

// 插入條目到雜湊表
static void sep_hash_insert(SepHashTable *table, double longitude, double latitude, double adjustment) {
    unsigned int hash = hash_double_double(longitude, latitude);
    int index = hash % table->size;

    // 簡單的鏈式衝突解決 - 每次插入到頭部
    SepEntry *entry = g_new(SepEntry, 1);
    entry->longitude = longitude;
    entry->latitude = latitude;
    entry->adjustment = adjustment;

    // 鏈式插入
    entry->next = table->buckets[index];
    table->buckets[index] = entry;
    table->count++;
}

// 查找對應的調整值
static double sep_hash_lookup(SepHashTable *table, double longitude, double latitude) {
    unsigned int hash = hash_double_double(longitude, latitude);
    int index = hash % table->size;

    SepEntry *entry = table->buckets[index];
    while (entry) {
        // 精度比較（考慮浮點數精度問題）
        if (fabs(entry->longitude - longitude) < 1e-10 && fabs(entry->latitude - latitude) < 1e-10) {
            return entry->adjustment;
        }
        entry = entry->next;
    }

    // 未找到
    return -99999.0; // 特殊值表示未找到
}

// 載入SEP文件到雜湊表
static SepHashTable* load_sep_file(const char *sep_path) {
    FILE *file = fopen(sep_path, "r");
    if (!file) {
        return NULL;
    }

    SepHashTable *table = sep_hash_init(SEP_HASH_SIZE);
    char line[512];
    int line_number = 0;

    while (fgets(line, sizeof(line), file)) {
        line_number++;

        // 移除注释和空白
        char *comment_pos = strchr(line, ';');
        if (comment_pos) *comment_pos = '\0';

        // 将制表符和多个空格转换为单个空格
        char *ptr = line;
        while (*ptr) {
            if (*ptr == '\t') *ptr = ' ';
            ptr++;
        }

        // 跳过空行
        g_strstrip(line);
        if (strlen(line) == 0) continue;

        // 解析经纬度和调整值
        double longitude, latitude, adjustment;
        if (sscanf(line, "%lf %lf %lf", &longitude, &latitude, &adjustment) == 3) {
            sep_hash_insert(table, longitude, latitude, adjustment);
        }
        // 忽略格式錯誤的行
    }

    fclose(file);
    return table;
}

// 生成輸出文件名（在原文件名後加上 "_converted"）
static char* generate_output_filename(const char *input_path) {
    // 查找文件擴展名
    const char *dot_pos = strrchr(input_path, '.');
    if (!dot_pos) {
        return g_strdup_printf("%s_converted", input_path);
    }

    // 從擴展名前插入 "_converted"
    size_t path_len = dot_pos - input_path;
    char *result = g_new(char, path_len + 20); // 多預留空間

    // 複製路徑和文件名（不含擴展名）
    memcpy(result, input_path, path_len);
    result[path_len] = '\0';

    // 添加 "_converted" 和原始擴展名
    strcat(result, "_converted");
    strcat(result, dot_pos);

    return result;
}

// 使用共享的 TideDataRow 結構定義（在 include/callbacks.h 中定義）

// 簡化的進度更新回調函數類型（避免與 angle_parser.h 衝突）
typedef void (*ElevationProgressCallback)(double progress, const char *message);

// 主處理函數 - 高程轉換處理
gboolean process_elevation_conversion(const char *input_path, const char *sep_path,
                                    GString *result_text, GError **error, GtkProgressBar *progress_bar);

// 進度回調函數實現
gboolean process_elevation_conversion_with_callback(const char *input_path, const char *sep_path,
                                    GString *result_text, GError **error,
                                    void (*progress_callback)(double, const char*));

// 進度回調通用的實現模式
static void dummy_progress_callback(double progress, const char *message) {
    // 默認不需要做任何事 - 抑制未使用參數警告
    (void)progress;
    (void)message;
}

// 主處理函數 - 高程轉換處理
gboolean process_elevation_conversion(const char *input_path, const char *sep_path,
                                    GString *result_text, GError **error, GtkProgressBar *progress_bar) {
    // 抑制未使用參數警告
    (void)progress_bar;
    return process_elevation_conversion_with_callback(input_path, sep_path, result_text, error, dummy_progress_callback);
}

// 主處理函數 - 高程轉換處理 (支援進度回調)
gboolean process_elevation_conversion_with_callback(const char *input_path, const char *sep_path,
                                    GString *result_text, GError **error, void (*progress_callback)(double, const char*)) {
    g_string_append_printf(result_text, "開始處理高程轉換：\n");
    g_string_append_printf(result_text, "===========================================\n");
    g_string_append_printf(result_text, "輸入檔案: %s\n", input_path);
    g_string_append_printf(result_text, "SEP檔案: %s\n\n", sep_path);

    // 1. 載入SEP對照數據
    SepHashTable *sep_table = load_sep_file(sep_path);
    if (!sep_table) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法載入SEP檔案: %s", sep_path);
        return FALSE;
    }

    g_string_append_printf(result_text, "已載入 %d 個SEP對照點\n", sep_table->count);

    // 2. 生成輸出文件名
    char *output_path = generate_output_filename(input_path);
    g_string_append_printf(result_text, "輸出檔案: %s\n\n", output_path);

    // 3. 打開輸入和輸出文件
    FILE *input_file = fopen(input_path, "r");
    if (!input_file) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法打開輸入檔案: %s", input_path);
        sep_hash_free(sep_table);
        g_free(output_path);
        return FALSE;
    }

    FILE *output_file = fopen(output_path, "w");
    if (!output_file) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法創建輸出檔案: %s", output_path);
        fclose(input_file);
        sep_hash_free(sep_table);
        g_free(output_path);
        return FALSE;
    }

    // 4. 先使用類似 wc -l 的方法統計總行數
    int total_lines = 0;
    int processed_lines = 0;
    int filtered_lines = 0;
    int matched_lines = 0;
    int unmatched_lines = 0;

    // 使用指针追路和fgets統計總行數（類似 wc -l）
    char temp_line[1024];
    rewind(input_file); // 確保從文件開頭開始
    while (fgets(temp_line, sizeof(temp_line), input_file)) {
        // 簡單檢查是否為有效數據行（非空行和非註釋行）
        g_strstrip(temp_line);
        if (strlen(temp_line) > 0 && temp_line[0] != ';') {
            total_lines++;
        }
    }
    fseek(input_file, 0, SEEK_SET); // 重新回到文件開頭

    g_string_append_printf(result_text, "使用快速計數獲得總行數: %d\n\n", total_lines);

    g_string_append_printf(result_text, "開始處理，共 %d 行數據...\n", total_lines);

    // 5. 逐行處理 - 通過進度回調支持多線程UI更新
    int current_line = 0;
    const int PROGRESS_UPDATE_INTERVAL = 100; // 每100行更新一次進度
    int lines_since_last_update = 0;

    while (fgets(temp_line, sizeof(temp_line), input_file)) {
        current_line++;

        // 每處理一定行數更新進度條
        lines_since_last_update++;
        if (lines_since_last_update >= PROGRESS_UPDATE_INTERVAL ||
            current_line == total_lines ||
            (progress_callback && current_line % PROGRESS_UPDATE_INTERVAL == 0)) {

            if (progress_callback && total_lines > 0) {
                double progress = (double)current_line / total_lines;
                char progress_text[100];
                snprintf(progress_text, sizeof(progress_text), "處理中... %d/%d (%.1f%%)",
                        current_line, total_lines, progress * 100);
                progress_callback(progress, progress_text);
            }

            // 如果有進度條（非多線程模式），也更新進度條
            // 如果有進度條（用於回調函數的完成後設置）

            // 重置計數器
            lines_since_last_update = 0;

            // 允許GUI事件處理，避免UI完全凍結
            while (gtk_events_pending()) {
                gtk_main_iteration();
            }
        }

        // 解析數據行
        TideDataRow row;
        if (!parse_tide_data_row(temp_line, &row)) {
            g_string_append_printf(result_text, "警告: 第%d行解析失敗，跳過\n", current_line);
            continue;
        }

        // 過濾：檢查col6和col7是否其中一個為0
        if (row.col6 == 0.0 || row.col7 == 0.0) {
            filtered_lines++;
            continue; // 不寫入輸出文件，直接跳過
        }

        // 查找SEP對照值
        double adjustment = sep_hash_lookup(sep_table, row.longitude, row.latitude);
        char output_line[1024];
        gboolean is_matched = (adjustment > -99998.0); // 檢查是否找到對應值

        if (is_matched) {
            // 應用調整: tide += adjustment, processed_depth -= adjustment
            row.tide += adjustment;
            row.processed_depth -= adjustment;
            matched_lines++;

            // 格式化輸出行（保持原始格式）
            snprintf(output_line, sizeof(output_line),
                    "%s/%.3f/%.7f/%.7f/%.3f/%.3f/%.3f\n",
                    row.datetime, row.tide, row.longitude, row.latitude,
                    row.processed_depth, row.col6, row.col7);

        } else {
            // 未找到匹配，添加(*)標記
            unmatched_lines++;
            snprintf(output_line, sizeof(output_line),
                    "(*)%s/%.3f/%.7f/%.7f/%.3f/%.3f/%.3f\n",
                    row.datetime, row.tide, row.longitude, row.latitude,
                    row.processed_depth, row.col6, row.col7);
        }

        // 寫入輸出文件
        fputs(output_line, output_file);
        processed_lines++;
    }

    // 6. 清理資源
    fclose(input_file);
    fclose(output_file);
    sep_hash_free(sep_table);
    g_free(output_path);

    // 7. 最終報告
    g_string_append_printf(result_text, "\n轉換完成統計:\n");
    g_string_append_printf(result_text, "===========================================\n");
    g_string_append_printf(result_text, "總行數: %d\n", total_lines);
    g_string_append_printf(result_text, "過濾行數 (col6/col7=0): %d\n", filtered_lines);
    g_string_append_printf(result_text, "有效處理行數: %d\n", processed_lines);
    g_string_append_printf(result_text, "SEP匹配行數: %d\n", matched_lines);
    g_string_append_printf(result_text, "SEP未匹配行數: %d\n", unmatched_lines);

    double match_rate = total_lines > 0 ? (double)matched_lines / (processed_lines + matched_lines + unmatched_lines) * 100 : 0;
    g_string_append_printf(result_text, "匹配率: %.1f%%\n", match_rate);

    g_string_append_printf(result_text, "\n高程轉換完成！✅\n");

    return TRUE;
}
