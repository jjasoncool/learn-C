// 高程轉換處理模組
// 負責處理7欄文字文件和SEP對照文件的高程轉換邏輯

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// 非同步統計行數的資料結構
typedef struct {
    const char *input_path;
    volatile int *total_lines_ptr;  // 指向總行數變數
    volatile gboolean *counting_done_ptr;  // 統計完成標記
    volatile gboolean *cancel_counting_ptr; // 取消統計標記
    volatile int *known_total_lines_ptr;    // 主線程的已知總行數
    GMutex *counting_mutex;  // 保護共享變數
    GCond *counting_cond;    // 條件變數，用於通知主線程
} CountingData;

// 背景統計行數的線程函數
static gpointer counting_thread_func(gpointer data) {
    CountingData *counting_data = (CountingData *)data;

    FILE *count_file = fopen(counting_data->input_path, "r");
    if (!count_file) {
        g_mutex_lock(counting_data->counting_mutex);
        *counting_data->counting_done_ptr = TRUE;
        g_cond_signal(counting_data->counting_cond);
        g_mutex_unlock(counting_data->counting_mutex);
        return NULL;
    }

    char line[8192];
    int lines_count = 0;

    // 統計行數
    while (fgets(line, sizeof(line), count_file)) {
        // 檢查取消請求
        if (*counting_data->cancel_counting_ptr) {
            break;
        }

        g_strstrip(line);
        if (strlen(line) > 0 && line[0] != ';') {
            lines_count++;
        }
    }

    fclose(count_file);

    // 通知主線程統計完成
    g_mutex_lock(counting_data->counting_mutex);
    *counting_data->counting_done_ptr = TRUE;
    *counting_data->known_total_lines_ptr = lines_count;  // 在鎖內更新統計結果
    g_cond_signal(counting_data->counting_cond);  // 發出完成訊號
    g_mutex_unlock(counting_data->counting_mutex);

    return NULL;
}
#include "../../include/callbacks.h"  // 引入 TideDataRow 和 parse_tide_data_row

// Hash table 配置
#define SEP_HASH_SIZE 8192

// 最近鄰資料結構，用於儲存到目標點的距離和調整值
typedef struct {
    double distance;     // 到目標點的距離
    double adjustment;   // SEP調整值
    double latitude;     // 緯度
    double longitude;    // 經度
} NeighborPoint;

// 大圓距離公式 (Haversine formula) 計算兩點間的距離
static double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // 地球半徑（公尺）
    double dlat = (lat2 - lat1) * G_PI / 180.0;
    double dlon = (lon2 - lon1) * G_PI / 180.0;

    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1 * G_PI / 180.0) * cos(lat2 * G_PI / 180.0) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));

    return R * c; // 返回距離（公尺）
}

// SEP對照資料結構
typedef struct SepEntry {
    double longitude;   // 經度
    double latitude;    // 緯度
    double adjustment;  // 調整值
    struct SepEntry *next; // 鏈表下一節點
} SepEntry;

// Hash table 結構
typedef struct {
    SepEntry **buckets;  // 桶陣列
    int size;           // 哈希表大小
    int count;          // 項目總數
} SepHashTable;

// 效能優化：連續陣列儲存SEP點，用於快速最近鄰搜索
typedef struct {
    double *longitudes;
    double *latitudes;
    double *adjustments;
    int count;
    int capacity;
} SepPointArray;

// 地理空間網格索引：第二階段效能優化
typedef struct {
    SepPointArray ***grids;          // 2D網格陣列 [lat][lon]
    int lat_grid_size, lon_grid_size; // 網格尺寸
    double min_lat, max_lat;         // 經緯度範圍
    double min_lon, max_lon;
    double lat_resolution, lon_resolution; // 每個網格的經緯度解析度
} SpatialGrid;

// 簡易複合結構：同時維護hash table和多層索引
typedef struct {
    SepHashTable *hash_table;   // 保留用於精確匹配
    SepPointArray *point_array; // 第一階段：全量陣列
    SpatialGrid *spatial_grid;  // 第二階段：空間網格索引
} SepDataStructure;

// 初始化效能優化的SEP點陣列
static SepPointArray* sep_point_array_init(int initial_capacity) {
    SepPointArray *array = g_new(SepPointArray, 1);
    array->capacity = initial_capacity > 0 ? initial_capacity : 1024;
    array->count = 0;

    array->longitudes = g_new(double, array->capacity);
    array->latitudes = g_new(double, array->capacity);
    array->adjustments = g_new(double, array->capacity);

    return array;
}

// 釋放SEP點陣列
static void sep_point_array_free(SepPointArray *array) {
    if (!array) return;

    g_free(array->longitudes);
    g_free(array->latitudes);
    g_free(array->adjustments);
    g_free(array);
}

// 初始化空間網格索引
static SpatialGrid* spatial_grid_init(int lat_grid_size, int lon_grid_size) {
    SpatialGrid *grid = g_new(SpatialGrid, 1);
    grid->lat_grid_size = lat_grid_size;
    grid->lon_grid_size = lon_grid_size;

    // 初始化經緯度範圍為極端值，會在加入點時更新
    grid->min_lat = G_MAXDOUBLE;
    grid->max_lat = -G_MAXDOUBLE;
    grid->min_lon = G_MAXDOUBLE;
    grid->max_lon = -G_MAXDOUBLE;

    // 配置2D網格陣列
    grid->grids = g_new(SepPointArray**, lat_grid_size);
    for (int lat = 0; lat < lat_grid_size; lat++) {
        grid->grids[lat] = g_new0(SepPointArray*, lon_grid_size);
        for (int lon = 0; lon < lon_grid_size; lon++) {
            grid->grids[lat][lon] = sep_point_array_init(64); // 每個網格初始容量64
        }
    }

    return grid;
}

// 釋放空間網格索引
static void spatial_grid_free(SpatialGrid *grid) {
    if (!grid) return;

    for (int lat = 0; lat < grid->lat_grid_size; lat++) {
        for (int lon = 0; lon < grid->lon_grid_size; lon++) {
            sep_point_array_free(grid->grids[lat][lon]);
        }
        g_free(grid->grids[lat]);
    }
    g_free(grid->grids);
    g_free(grid);
}

// 將經緯度轉換為網格索引
static void lat_lon_to_grid_indices(const SpatialGrid *grid, double latitude, double longitude,
                                   int *lat_index, int *lon_index) {
    if (grid->max_lat == grid->min_lat) {
        *lat_index = 0;
    } else {
        *lat_index = (int)((latitude - grid->min_lat) / grid->lat_resolution);
        *lat_index = CLAMP(*lat_index, 0, grid->lat_grid_size - 1);
    }

    if (grid->max_lon == grid->min_lon) {
        *lon_index = 0;
    } else {
        *lon_index = (int)((longitude - grid->min_lon) / grid->lon_resolution);
        *lon_index = CLAMP(*lon_index, 0, grid->lon_grid_size - 1);
    }
}

// 向陣列添加一個點
static void sep_point_array_add(SepPointArray *array, double longitude, double latitude, double adjustment) {
    // 動態擴容
    if (array->count >= array->capacity) {
        array->capacity *= 2;
        array->longitudes = g_renew(double, array->longitudes, array->capacity);
        array->latitudes = g_renew(double, array->latitudes, array->capacity);
        array->adjustments = g_renew(double, array->adjustments, array->capacity);
    }

    array->longitudes[array->count] = longitude;
    array->latitudes[array->count] = latitude;
    array->adjustments[array->count] = adjustment;
    array->count++;
}

// 向空間網格添加一個點
static void spatial_grid_add_point(SpatialGrid *grid, double longitude, double latitude, double adjustment) {
    // 更新經緯度範圍
    grid->min_lat = MIN(grid->min_lat, latitude);
    grid->max_lat = MAX(grid->max_lat, latitude);
    grid->min_lon = MIN(grid->min_lon, longitude);
    grid->max_lon = MAX(grid->max_lon, longitude);

    // 計算並更新解析度
    if (grid->max_lat > grid->min_lat) {
        grid->lat_resolution = (grid->max_lat - grid->min_lat) / grid->lat_grid_size;
    }
    if (grid->max_lon > grid->min_lon) {
        grid->lon_resolution = (grid->max_lon - grid->min_lon) / grid->lon_grid_size;
    }

    // 直接加到第一個網格，如果範圍還沒確定
    if (grid->lat_resolution == 0 || grid->lon_resolution == 0) {
        sep_point_array_add(grid->grids[0][0], longitude, latitude, adjustment);
        return;
    }

    // 計算網格索引
    int lat_index, lon_index;
    lat_lon_to_grid_indices(grid, latitude, longitude, &lat_index, &lon_index);

    // 添加到對應網格
    sep_point_array_add(grid->grids[lat_index][lon_index], longitude, latitude, adjustment);
}

// 使用空間網格的全域插值查詢 (確保總是能找到最近點)
static double sep_grid_lookup_with_interpolation(const SpatialGrid *grid,
                                                double target_longitude,
                                                double target_latitude) {
    if (!grid) return -99999.0;

    // 初始化最近點數組
    NeighborPoint nearest_points[2];
    int found_count = 0;

    // 初始化為極大距離
    nearest_points[0].distance = G_MAXDOUBLE;
    nearest_points[0].adjustment = -99999.0;
    nearest_points[0].latitude = 0.0;
    nearest_points[0].longitude = 0.0;

    nearest_points[1].distance = G_MAXDOUBLE;
    nearest_points[1].adjustment = -99999.0;
    nearest_points[1].latitude = 0.0;
    nearest_points[1].longitude = 0.0;

    // **全網格搜尋** - 遍歷所有網格，確保總是能找到最近的點
    for (int lat_idx = 0; lat_idx < grid->lat_grid_size; lat_idx++) {
        for (int lon_idx = 0; lon_idx < grid->lon_grid_size; lon_idx++) {
            // 獲取這個網格的點陣列
            SepPointArray *grid_array = grid->grids[lat_idx][lon_idx];
            if (!grid_array || grid_array->count == 0) {
                continue;
            }

            // 在這個網格中搜尋所有點
            for (int i = 0; i < grid_array->count; i++) {
                double distance = calculate_distance(target_latitude, target_longitude,
                                                   grid_array->latitudes[i], grid_array->longitudes[i]);

                // 更新最近的兩個點（全域搜尋）
                if (distance < nearest_points[0].distance) {
                    // 移位第二近的點
                    nearest_points[1] = nearest_points[0];
                    // 設置最近的點
                    nearest_points[0].distance = distance;
                    nearest_points[0].adjustment = grid_array->adjustments[i];
                    nearest_points[0].latitude = grid_array->latitudes[i];
                    nearest_points[0].longitude = grid_array->longitudes[i];
                    found_count = (found_count < 2) ? found_count + 1 : 2;
                } else if (distance < nearest_points[1].distance && found_count >= 1) {
                    // 更新第二近的點
                    nearest_points[1].distance = distance;
                    nearest_points[1].adjustment = grid_array->adjustments[i];
                    nearest_points[1].latitude = grid_array->latitudes[i];
                    nearest_points[1].longitude = grid_array->longitudes[i];
                    found_count = 2;
                }
            }
        }
    }

    // 距離加權線性插值
    if (found_count >= 2) {
        double d1 = nearest_points[0].distance;
        double d2 = nearest_points[1].distance;
        double a1 = nearest_points[0].adjustment;
        double a2 = nearest_points[1].adjustment;

        // 距離加權插值公式: result = (a2 * d1 + a1 * d2) / (d1 + d2)
        double interpolated_adjustment = (a2 * d1 + a1 * d2) / (d1 + d2);
        return interpolated_adjustment;
    }
    // 如果只有一個點，則直接使用最近的點
    else if (found_count == 1) {
        return nearest_points[0].adjustment;
    }

    // 如果找不到任何點，則返回無法插值
    return -99999.0;
}

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

// 初始化複合結構 (包含空間網格)
static SepDataStructure* sep_data_init(void) {
    SepDataStructure *data = g_new(SepDataStructure, 1);
    data->hash_table = NULL;
    data->point_array = NULL;
    data->spatial_grid = NULL;
    return data;
}

// 釋放複合結構 (包含空間網格)
static void sep_data_free(SepDataStructure *data) {
    if (!data) return;

    if (data->hash_table) {
        sep_hash_free(data->hash_table);
    }
    if (data->point_array) {
        sep_point_array_free(data->point_array);
    }
    if (data->spatial_grid) {
        spatial_grid_free(data->spatial_grid);
    }
    g_free(data);
}

// 載入SEP文件到複合結構 (效能優化最終版本)
static SepDataStructure* load_sep_file_optimized(const char *sep_path) {
    FILE *file = fopen(sep_path, "r");
    if (!file) {
        return NULL;
    }

    // 階段1: 同時初始化所有索引結構
    SepDataStructure *data = sep_data_init();
    data->hash_table = sep_hash_init(SEP_HASH_SIZE);
    data->point_array = sep_point_array_init(1024); // 預估容量
    data->spatial_grid = spatial_grid_init(50, 50); // 50x50網格

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
            // 同時插入所有索引結構
            sep_hash_insert(data->hash_table, longitude, latitude, adjustment);
            sep_point_array_add(data->point_array, longitude, latitude, adjustment);
            spatial_grid_add_point(data->spatial_grid, longitude, latitude, adjustment);
        }
        // 忽略格式錯誤的行
    }

    fclose(file);
    return data;
}



// 生成轉換後文件名（完整處理）
static char* generate_converted_filename(const char *input_path) {
    // 查找文件擴展名
    const char *dot_pos = strrchr(input_path, '.');
    if (!dot_pos) {
        return g_strdup_printf("%s_converted", input_path);
    }

    // 從擴展名前插入 "_converted"
    size_t path_len = dot_pos - input_path;
    char *result = g_new(char, path_len + 17); // 多預留空間

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
    // 記錄開始時間
    time_t start_time = time(NULL);

    g_string_append_printf(result_text, "開始處理高程轉換：\n");
    g_string_append_printf(result_text, "===========================================\n");
    g_string_append_printf(result_text, "輸入檔案: %s\n", input_path);
    g_string_append_printf(result_text, "SEP檔案: %s\n\n", sep_path);

    // 1. 載入SEP對照數據 (使用效能優化版本)
    SepDataStructure *sep_data = load_sep_file_optimized(sep_path);
    if (!sep_data) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法載入SEP檔案: %s", sep_path);
        return FALSE;
    }

    g_string_append_printf(result_text, "已載入 %d 個SEP對照點 (空間網格索引最終版本)\n", sep_data->hash_table->count);

    // 2. 生成輸出文件名
    char *converted_path = generate_converted_filename(input_path);
    char *temp_filtered_path = g_strdup_printf("%s.filtered_temp", input_path);
    g_string_append_printf(result_text, "轉換後檔案: %s\n", converted_path);
    g_string_append_printf(result_text, "原始檔案將被修改為過濾後版本\n\n");

    // 3. 打開輸入檔案和輸出檔案
    FILE *input_file = fopen(input_path, "r");
    if (!input_file) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法打開輸入檔案: %s", input_path);
        sep_data_free(sep_data);
        g_free(converted_path);
        g_free(temp_filtered_path);
        return FALSE;
    }

    FILE *converted_file = fopen(converted_path, "w");
    if (!converted_file) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法創建轉換檔案: %s", converted_path);
        fclose(input_file);
        sep_data_free(sep_data);
        g_free(converted_path);
        g_free(temp_filtered_path);
        return FALSE;
    }

    FILE *temp_filtered_file = fopen(temp_filtered_path, "w");
    if (!temp_filtered_file) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "無法創建臨時過濾檔案: %s", temp_filtered_path);
        fclose(input_file);
        fclose(converted_file);
        sep_data_free(sep_data);
        g_free(converted_path);
        g_free(temp_filtered_path);
        return FALSE;
    }

    // 4. 初始化計數器和非同步統計
    int total_lines = 0;
    int processed_lines = 0;
    int filtered_lines = 0;
    int matched_lines = 0;
    int interpolated_lines = 0;

    // 初始化非同步統計
    gboolean counting_done = FALSE;
    gboolean cancel_counting = FALSE;
    GMutex counting_mutex;
    GCond counting_cond;

    g_mutex_init(&counting_mutex);
    g_cond_init(&counting_cond);

    // 使用更大的緩衝區來提高讀取效率
    char temp_line[8192];  // 8KB緩衝區

    g_string_append_printf(result_text, "開始處理數據（背景統計總行數）...\n");

    // 5. 逐行處理 - 支援非同步統計和取消
    int current_line = 0;
    int lines_since_last_update = 0;
    int known_total_lines = 0;  // 已知的總行數

    CountingData counting_data = {
        .input_path = input_path,
        .total_lines_ptr = &total_lines,
        .counting_done_ptr = &counting_done,
        .cancel_counting_ptr = &cancel_counting,
        .known_total_lines_ptr = &known_total_lines,  // 指向主線程的變數
        .counting_mutex = &counting_mutex,
        .counting_cond = &counting_cond
    };

    // 啟動背景統計線程
    GThread *counting_thread = g_thread_new("counting-thread", counting_thread_func, &counting_data);

    while (fgets(temp_line, sizeof(temp_line), input_file)) {
        current_line++;
        total_lines++;  // 動態統計總行數

        // 🔥 **強力取消檢查：每一行開始就檢查** 🔥
        lines_since_last_update++;
        if (lines_since_last_update >= 10000 || current_line % 10000 == 0) {  // 檢查統計更新

            // 檢查統計狀態（確保記憶體可見性）
            if (known_total_lines == 0) {
                g_mutex_lock(&counting_mutex);
                // 統計線程已經在鎖內設定了 known_total_lines
                // 這裡只需要確保可見性
                g_mutex_unlock(&counting_mutex);
            }

            // 進行進度更新
            if (progress_callback) {
                char progress_message[150];
                if (known_total_lines > 0) {
                    // 統計已完成，顯示精確進度
                    double progress = (double)current_line / known_total_lines;
                    sprintf(progress_message, "處理中: %d/%d (%.1f%%)", current_line, known_total_lines, progress * 100.0);
                    progress_callback(progress * 100.0, progress_message);
                } else {
                    // 統計尚未完成，顯示已處理行數
                    sprintf(progress_message, "處理中: 已處理 %d 行 (統計總行數中...)", current_line);
                    progress_callback(-1.0, progress_message);
                }

                // 檢查取消請求
                if (error && *error && g_error_matches(*error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                    g_print("[CANCEL] 檢測到取消請求，正在終止處理循環和統計線程\n");
                    cancel_counting = TRUE;  // 取消統計線程
                    break;  // 立即跳出處理循環
                }
            }

            lines_since_last_update = 0;

            // 允許GUI事件處理
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

        // 寫入過濾後檔案（原始格式，不進行轉換）
        fputs(temp_line, temp_filtered_file);

        // 使用距離加權插值查找SEP對照值 (總是都會進行插值處理)
        double exact_adjustment = sep_hash_lookup(sep_data->hash_table, row.longitude, row.latitude);
        double interpolated_adjustment = sep_grid_lookup_with_interpolation(sep_data->spatial_grid,
                                                                           row.longitude, row.latitude);

        char converted_line[1024];
        gboolean has_exact_match = (exact_adjustment > -99998.0);
        gboolean has_interpolation = (interpolated_adjustment > -99998.0);

        // 決定使用的調整值：總是嘗試插值，每次數據都要有調整！
        double final_adjustment;
        if (has_exact_match) {
            final_adjustment = exact_adjustment;
            matched_lines++;  // 記錄精確匹配數量
        } else if (has_interpolation) {
            final_adjustment = interpolated_adjustment;
            interpolated_lines++;  // 記錄插值匹配數量
        } else {
            // 插值也找不到點時，設定預設值（極端情況）
            final_adjustment = 0.0;
            // 不統計在任何處理類別中，因為這是無法處理的情況
        }

        // 應用調整值到數據行
        row.tide += final_adjustment;
        row.processed_depth -= final_adjustment;

        // 格式化轉換後輸出行（保持原始格式，所有資料都處理）
        snprintf(converted_line, sizeof(converted_line),
                "%s/%.3f/%.7f/%.7f/%.3f/%.3f/%.3f\n",
                row.datetime, row.tide, row.longitude, row.latitude,
                row.processed_depth, row.col6, row.col7);

        // 寫入轉換後檔案
        fputs(converted_line, converted_file);
        processed_lines++;
    }

    // 6. 清理資源並覆蓋原始檔案為過濾版本
    fclose(input_file);
    fclose(temp_filtered_file);
    fclose(converted_file);

    // 用過濾後的臨時檔案覆蓋原始檔案
    if (rename(temp_filtered_path, input_path) != 0) {
        // 顯示詳細的rename錯誤信息
        g_print("[ERROR] rename() 失敗: %s -> %s\n", temp_filtered_path, input_path);
        g_print("[ERROR] 錯誤代碼: %d, 錯誤訊息: %s\n", errno, strerror(errno));

        // 如果 rename 失敗，嘗試複製刪除的方法
        g_print("[INFO] 嘗試備用方案：複製檔案內容...\n");

        FILE *src = fopen(temp_filtered_path, "rb");
        FILE *dst = fopen(input_path, "wb");

        g_print("[DEBUG] 開啟檔案: src=%s (%p), dst=%s (%p)\n", temp_filtered_path, src, input_path, dst);

        if (src && dst) {
            char buffer[8192];
            size_t bytes;
            gboolean copy_success = TRUE;
            size_t total_bytes = 0;

            g_print("[DEBUG] 開始複製檔案內容...\n");

            while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                total_bytes += bytes;
                if (fwrite(buffer, 1, bytes, dst) != bytes) {
                    copy_success = FALSE;
                    g_print("[ERROR] fwrite() 失敗: 寫入 %zu 位元組失敗，錯誤: %s\n", bytes, strerror(errno));
                    break;
                }
            }

            g_print("[DEBUG] 複製完成，共複製 %zu 位元組\n", total_bytes);

            fclose(src);
            fclose(dst);

            if (copy_success) {
                // 刪除臨時檔案
                if (remove(temp_filtered_path) == 0) {
                    g_print("[SUCCESS] 檔案覆蓋成功，使用備用方案\n");
                } else {
                    g_print("[WARNING] 無法刪除臨時檔案: %s (錯誤: %s)\n", temp_filtered_path, strerror(errno));
                }
            } else {
                g_print("[ERROR] 檔案複製失敗，設定錯誤並返回\n");
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           "無法複製過濾結果到原始檔案: %s", input_path);
                sep_data_free(sep_data);
                g_free(converted_path);
                g_free(temp_filtered_path);
                return FALSE;
            }
        } else {
            if (src) fclose(src);
            if (dst) fclose(dst);
            g_print("[ERROR] 無法開啟檔案進行複製: src=%p, dst=%p\n", src, dst);
            if (!src) g_print("[ERROR] 無法開啟來源檔案: %s (錯誤: %s)\n", temp_filtered_path, strerror(errno));
            if (!dst) g_print("[ERROR] 無法開啟目標檔案: %s (錯誤: %s)\n", input_path, strerror(errno));
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       "無法開啟檔案進行複製: %s", input_path);
            sep_data_free(sep_data);
            g_free(converted_path);
            g_free(temp_filtered_path);
            return FALSE;
        }
    } else {
        g_print("[SUCCESS] 檔案覆蓋成功，使用 rename()\n");
    }

    sep_data_free(sep_data);
    g_free(converted_path);
    g_free(temp_filtered_path);

    // 記錄結束時間並計算處理時間
    time_t end_time = time(NULL);
    double processing_time = difftime(end_time, start_time);

    // 7. 最終報告
    g_string_append_printf(result_text, "\n轉換完成統計:\n");
    g_string_append_printf(result_text, "===========================================\n");
    g_string_append_printf(result_text, "總行數: %d\n", total_lines);
    g_string_append_printf(result_text, "過濾行數 (col6/col7=0): %d\n", filtered_lines);
    g_string_append_printf(result_text, "有效處理行數: %d\n", processed_lines);
    g_string_append_printf(result_text, "SEP精確匹配行數: %d\n", matched_lines);
    g_string_append_printf(result_text, "SEP插值匹配行數: %d\n", interpolated_lines);
    g_string_append_printf(result_text, "SEP總匹配行數: %d\n", matched_lines + interpolated_lines);

    int total_searched_lines = processed_lines; // 已處理的有效行數
    double exact_match_rate = total_searched_lines > 0 ? (double)matched_lines / total_searched_lines * 100 : 0;
    double interpolation_rate = total_searched_lines > 0 ? (double)interpolated_lines / total_searched_lines * 100 : 0;
    double total_match_rate = total_searched_lines > 0 ? (double)(matched_lines + interpolated_lines) / total_searched_lines * 100 : 0;

    g_string_append_printf(result_text, "\n匹配率統計:\n");
    g_string_append_printf(result_text, "精確匹配率: %.1f%%\n", exact_match_rate);
    g_string_append_printf(result_text, "插值匹配率: %.1f%%\n", interpolation_rate);
    g_string_append_printf(result_text, "總匹配率: %.1f%%\n", total_match_rate);

    g_string_append_printf(result_text, "\n處理時間統計:\n");
    g_string_append_printf(result_text, "處理時間: %.2f 秒\n", processing_time);

    g_string_append_printf(result_text, "\n高程轉換完成！✅\n");
    g_string_append_printf(result_text, "📊 資料處理統計：\n");
    g_string_append_printf(result_text, "   • 移除了 %d 筆無效資料 (col6或col7為0)\n", filtered_lines);
    g_string_append_printf(result_text, "   • 保留了 %d 筆有效資料\n", processed_lines);
    g_string_append_printf(result_text, "📄 輸出檔案：\n");
    g_string_append_printf(result_text, "   • 過濾後檔案：原始檔案已被修改為過濾版本\n");
    g_string_append_printf(result_text, "   • 轉換後檔案：%s\n", converted_path);
    g_string_append_printf(result_text, "🎯 地理空間插值功能成功啟用\n");

    // 等待統計線程完成並清理資源
    g_thread_join(counting_thread);
    g_mutex_clear(&counting_mutex);
    g_cond_clear(&counting_cond);

    return TRUE;
}
