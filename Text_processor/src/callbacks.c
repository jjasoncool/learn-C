#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callbacks.h"
#include "scan.h"
#include "angle_parser.h"
#include "max_finder.h"
#include "elevation_processing.h"

// 初始化應用狀態
void init_app_state(AppState *state) {
    if (!state) return;

    memset(state, 0, sizeof(AppState));
    g_mutex_init(&state->cancel_mutex);
    state->cancel_requested = FALSE;
    state->is_processing = FALSE;
}

// 清理應用狀態
void cleanup_app_state(AppState *state) {
    if (!state) return;

    g_free(state->selected_folder_path);
    g_free(state->selected_file_path);
    g_free(state->selected_sep_path);
    g_mutex_clear(&state->cancel_mutex);
    memset(state, 0, sizeof(AppState));
}

// 檢查是否請求取消
gboolean is_cancel_requested(AppState *state) {
    if (!state) return FALSE;

    g_mutex_lock(&state->cancel_mutex);
    gboolean result = state->cancel_requested;
    g_mutex_unlock(&state->cancel_mutex);

    return result;
}

// 設定取消請求
void set_cancel_requested(AppState *state, gboolean cancel) {
    if (!state) return;

    g_mutex_lock(&state->cancel_mutex);
    state->cancel_requested = cancel;
    g_mutex_unlock(&state->cancel_mutex);
}

// 設定處理狀態
void set_processing_state(AppState *state, gboolean processing) {
    if (!state) {
        g_printerr("Error: set_processing_state called with NULL state\n");
        return;
    }

    state->is_processing = processing;

    // 控制按鈕是否可用
    gtk_widget_set_sensitive(state->folder_button, !processing);

    // 顯示或隱藏進度條和取消按鈕
    if (processing) {
        gtk_widget_show(state->progress_container);
        gtk_widget_show(state->cancel_button);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), 0.0);
        gtk_label_set_text(GTK_LABEL(state->progress_label), "準備開始處理...");
    } else {
        gtk_widget_hide(state->progress_container);
        gtk_widget_hide(state->cancel_button);
        // 重置取消標記
        set_cancel_requested(state, FALSE);
    }
}

// 文件分析結果結構
typedef struct {
    char **lines;        // 存放前十行的內容
    int line_count;      // 實際讀取的行數
    int max_lines;       // 最大儲存行數 (10)
    GError *error;       // 錯誤資訊
    GString *parsed_info; // 解析后的字段信息
} FileAnalysisResult;

// 解析Tide數據行
gboolean parse_tide_data_row(const char *line, TideDataRow *row) {
    // 創建副本進行解析
    char *line_copy = g_strdup(line);

    // 第一個字段是datetime，到最後一個'/'為止
    char *first_slash_pos = strchr(line_copy, '/');
    if (!first_slash_pos || line_copy == first_slash_pos) {
        g_free(line_copy);
        return FALSE; // 無效格式
    }

    // 找到datetime字段（格式為 YYYY/MM/DD/HH:MM:SS.mmm）
    // datetime應該有4個'/'
    char *datetime_end = NULL;
    int slash_count = 0;

    for (char *ptr = first_slash_pos; ptr && *ptr; ptr = strchr(ptr + 1, '/')) {
        slash_count++;
        if (slash_count == 4) { // datetime有4个'/'
            datetime_end = ptr;
            break;
        }
    }

    if (!datetime_end) {
        g_free(line_copy);
        return FALSE; // 無法找到datetime結束位置
    }

    // 提取datetime部分
    size_t datetime_len = datetime_end - line_copy;
    if (datetime_len >= sizeof(row->datetime)) {
        g_free(line_copy);
        return FALSE; // datetime太長
    }
    memcpy(row->datetime, line_copy, datetime_len);
    row->datetime[datetime_len] = '\0';

    // 解析剩餘的數值字段
    char *remaining = datetime_end + 1; // 跳過datetime後的第一個'/'
    int parsed_count = sscanf(remaining, "%lf/%lf/%lf/%lf/%lf/%lf",
                             &row->tide, &row->longitude, &row->latitude,
                             &row->processed_depth, &row->col6, &row->col7);

    if (parsed_count != 6) {
        g_free(line_copy);
        return FALSE; // 數值字段解析失敗
    }

    g_free(line_copy);
    return TRUE;
}

// 清理檔案分析結果
static void free_file_analysis_result(FileAnalysisResult *result) {
    if (!result) return;

    if (result->lines) {
        for (int i = 0; i < result->line_count; i++) {
            g_free(result->lines[i]);
        }
        g_free(result->lines);
    }

    if (result->error) {
        g_error_free(result->error);
    }

    g_free(result);
}

// 分析文件前十行 (更兼容的版本)
static gboolean analyze_file_first_10_lines(const char *file_path, FileAnalysisResult *result) {
    FILE *file = NULL;
    char buffer[2048];  // 使用固定大小緩衝區，避免 getline 的兼容性問題

    // 初始化結果結構
    result->lines = g_new0(char*, 10);
    result->line_count = 0;
    result->max_lines = 10;
    result->error = NULL;

    // 嘗試打開文件
    file = fopen(file_path, "r");
    if (!file) {
        result->error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                   "無法打開文件: %s", file_path);
        return FALSE;
    }

// 讀取並分析前十行，並增加字段解析檢查
    while (result->line_count < result->max_lines &&
           fgets(buffer, sizeof(buffer), file) != NULL) {

        // 移除換行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }

        // 如果行太長，截取並添加省略號
        if (len >= sizeof(buffer) - 1) {
            // 行太長的情況比較少見，但在固定緩衝區中需要處理
            buffer[sizeof(buffer) - 4] = '.';
            buffer[sizeof(buffer) - 3] = '.';
            buffer[sizeof(buffer) - 2] = '.';
            buffer[sizeof(buffer) - 1] = '\0';
        }

        // 複製到結果中
        result->lines[result->line_count] = g_strdup(buffer);
        result->line_count++;
    }

    // 關閉文件
    fclose(file);

    return TRUE;
}

// 選擇檔案的回調函數
void on_select_file(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("選擇檔案",
                                         GTK_WINDOW(state->window),
                                         action,
                                         "_取消",
                                         GTK_RESPONSE_CANCEL,
                                         "_選擇",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    // 設置文件過濾器 (可選的 TXT 文件)
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "文字檔案");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);

        if (!filename) {
            gtk_label_set_text(GTK_LABEL(state->status_label), "選擇檔案失敗：無法獲取路徑");
            gtk_widget_destroy(dialog);
            return;
        }

        // 分析文件前十行
        FileAnalysisResult *analysis_result = g_new(FileAnalysisResult, 1);
        if (analyze_file_first_10_lines(filename, analysis_result)) {
            // 格式化顯示結果，包含字段解析檢查
            GString *display_text = g_string_new("");
            g_string_append_printf(display_text, "檔案分析結果:\n");
            g_string_append_printf(display_text, "===========================================\n");
            g_string_append_printf(display_text, "檔案: %s\n\n", filename);

            // 檢查是否為高程數據格式（7欄）
            gboolean is_elevation_format = g_str_has_prefix(filename, "sample_original") ||
                                         g_str_has_suffix(filename, ".txt") ||
                                         strstr(filename, "LAT-EL");

            if (is_elevation_format) {
                // 高程數據格式分析
                g_string_append_printf(display_text, "檔案格式: 高程數據 (7欄)\n");
                g_string_append_printf(display_text, "期望格式: datetime/tide/經度/緯度/ProcessedDepth/col6/col7\n\n");
            } else {
                // 角度數據格式分析
                g_string_append_printf(display_text, "檔案格式: 角度數據 (變動欄位)\n\n");
            }

            g_string_append_printf(display_text, "前 %d 行內容 (含字段檢查):\n", analysis_result->line_count);
            g_string_append_printf(display_text, "==================================================\n");

            // 分析每一行並檢查字段解析
            for (int i = 0; i < analysis_result->line_count; i++) {
                g_string_append_printf(display_text, "第 %d 行: %s\n", i + 1, analysis_result->lines[i]);

                if (is_elevation_format) {
                    // 試著解析7欄數據
                    TideDataRow test_row;
                    if (parse_tide_data_row(analysis_result->lines[i], &test_row)) {
                        g_string_append_printf(display_text,
                            "       ├──解析成功: DT=%s, Tide=%.3f, Lon=%.7f, Lat=%.7f, Depth=%.3f, C6=%.3f, C7=%.3f\n",
                            test_row.datetime, test_row.tide, test_row.longitude,
                            test_row.latitude, test_row.processed_depth, test_row.col6, test_row.col7);

                        // 檢查過濾條件
                        if (test_row.col6 == 0.0 || test_row.col7 == 0.0) {
                            g_string_append_printf(display_text, "       ├──過濾條件: ❌ 會被過濾 (col6或col7為0)\n");
                        } else {
                            g_string_append_printf(display_text, "       ├──過濾條件: ✅ 會被處理\n");
                        }
                    } else {
                        g_string_append_printf(display_text, "       ├──解析失敗: 格式不正確\n");
                    }
                }
                g_string_append_printf(display_text, "\n");
            }

            if (analysis_result->line_count == 0) {
                g_string_append(display_text, "(檔案是空的)\n");
            }

            g_string_append_printf(display_text, "==================================================\n");
            g_string_append_printf(display_text, "總共分析了 %d 行\n", analysis_result->line_count);

            // 添加文件格式建議
            if (is_elevation_format) {
                g_string_append_printf(display_text, "\n高程數據格式建議:\n");
                g_string_append_printf(display_text, "• 使用 '/' 作為分隔符\n");
                g_string_append_printf(display_text, "• 確保有7個字段\n");
                g_string_append_printf(display_text, "• 數值字段保持適當精確度\n");
                g_string_append_printf(display_text, "• col6與col7非0的行會被處理\n");
            }

            // 更新界面 - 根據當前活動標籤頁選擇正確的文本緩衝區
            GtkTextBuffer *target_buffer = state->text_buffer;  // 預設使用角度分析的緩衝區

            // 檢查當前活動標籤頁
            if (state->notebook) {
                int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook));
                // 如果是高程轉換標籤頁（設為第一個標籤頁，索引 1），使用高程緩衝區
                if (current_page == 1 && state->altitude_text_buffer) {
                    target_buffer = state->altitude_text_buffer;
                }
            }

    // 儲存選擇的檔案路徑（用於高程轉換）
    g_free(state->selected_file_path);
    state->selected_file_path = g_strdup(filename);

    gtk_text_buffer_set_text(target_buffer, display_text->str, -1);
    g_string_free(display_text, TRUE);

    char *status_text = g_strdup_printf("已分析檔案: %s", filename);
    gtk_label_set_text(GTK_LABEL(state->status_label), status_text);
    g_free(status_text);

        } else {
            // 處理錯誤
            char *error_msg = g_strdup_printf("分析檔案失敗: %s",
                analysis_result->error ? analysis_result->error->message : "未知錯誤");
            gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);

            // 錯誤訊息也根據當前標籤頁選擇正確的緩衝區
            GtkTextBuffer *error_buffer = state->text_buffer;
            if (state->notebook) {
                int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook));
                if (current_page == 1 && state->altitude_text_buffer) {
                    error_buffer = state->altitude_text_buffer;
                }
            }
            gtk_text_buffer_set_text(error_buffer, error_msg, -1);
            g_free(error_msg);
        }

        // 清理資源
        free_file_analysis_result(analysis_result);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// 選擇SEP檔案的回調函數
void on_select_sep_file(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("選擇SEP檔案",
                                         GTK_WINDOW(state->window),
                                         action,
                                         "_取消",
                                         GTK_RESPONSE_CANCEL,
                                         "_選擇",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    // 設置SEP文件過濾器
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SEP檔案");
    gtk_file_filter_add_pattern(filter, "*.sep");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);

        if (!filename) {
            gtk_label_set_text(GTK_LABEL(state->status_label), "選擇SEP檔案失敗：無法獲取路徑");
            gtk_widget_destroy(dialog);
            return;
        }

        // 儲存選擇的SEP檔案路徑
        g_free(state->selected_sep_path);
        state->selected_sep_path = g_strdup(filename);

        char *status_text = g_strdup_printf("已選擇SEP檔案: %s", filename);
        gtk_label_set_text(GTK_LABEL(state->status_label), status_text);
        g_free(status_text);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// 高程轉換處理工作線程數據
typedef struct {
    AppState *app_state;
    GString *result_text;
    GError *error;
    char *input_path;
    char *sep_path;
    double current_progress;
    char progress_text[200];
} ElevationProcessData;

// 更新進度條的回調函數（線程安全）
static gboolean update_progress_callback(gpointer user_data) {
    ElevationProcessData *data = (ElevationProcessData*)user_data;

    if (data->app_state->elevation_progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->app_state->elevation_progress_bar),
                                     data->current_progress);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(data->app_state->elevation_progress_bar),
                                 data->progress_text);
    }

    return FALSE; // 只執行一次
}

// 更新最終結果的回調函數（線程安全）
static gboolean update_result_callback(gpointer user_data) {
    ElevationProcessData *data = (ElevationProcessData*)user_data;
    AppState *state = data->app_state;

    // 更新狀態標籤
    if (data->error) {
        const char *error_message = data->error->message ? data->error->message : "未知錯誤";
        gtk_label_set_text(GTK_LABEL(state->status_label), error_message);

        if (state->altitude_text_buffer) {
            gtk_text_buffer_set_text(state->altitude_text_buffer, error_message, -1);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(state->status_label), "高程轉換完成");

        if (state->altitude_text_buffer) {
            gtk_text_buffer_set_text(state->altitude_text_buffer, data->result_text->str, -1);
        }
    }

    // 清理資源
    g_string_free(data->result_text, TRUE);
    if (data->error) {
        g_error_free(data->error);
    }
    g_free(data->input_path);
    g_free(data->sep_path);
    g_free(data);

    return FALSE; // 只執行一次
}

// 背景工作線程函數 - 實際執行高程轉換
static void* elevation_conversion_worker(void *user_data) {
    ElevationProcessData *data = (ElevationProcessData*)user_data;

    // 初始化進度
    data->result_text = g_string_new("");
    data->error = NULL;

    // 創建進度更新上下文
    typedef struct {
        ElevationProcessData *data;
    } ProgressContext;

    ProgressContext *ctx = g_new(ProgressContext, 1);
    ctx->data = data;

    // 進度更新回調函數 (線程安全)
    void progress_update_callback(double percentage, const char *message) {
        ctx->data->current_progress = percentage;
        strncpy(ctx->data->progress_text, message, sizeof(ctx->data->progress_text) - 1);

        // 通過 GTK 的異步機制通知主線程更新UI
        g_idle_add(update_progress_callback, ctx->data);
    }

    // 開始處理 - 通過回調初始化進度
    progress_update_callback(0.0, "準備處理...");

    // 調用高程轉換處理函數（使用回調版本）
    extern gboolean process_elevation_conversion_with_callback(const char*, const char*, GString*, GError**, void (*)(double, const char*));
    if (!process_elevation_conversion_with_callback(data->input_path, data->sep_path,
                                          data->result_text, &data->error,
                                          progress_update_callback)) {
        // 處理失敗 - 立即通知主線程
        g_idle_add(update_result_callback, data);
        g_free(ctx);
        return NULL;
    }

    // 處理成功 - 最終進度更新
    progress_update_callback(1.0, "處理完成");

    // 通知主線程處理完成并顯示結果
    g_idle_add(update_result_callback, data);

    // 清理資源
    g_free(ctx);

    return NULL;
}

// 執行高程轉換的回調函數（多線程版本）
void on_perform_conversion(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    // 檢查必要的文件是否都已選擇
    if (!state->selected_file_path || !state->selected_sep_path) {
        char *error_msg;
        if (!state->selected_file_path && !state->selected_sep_path) {
            error_msg = "請先選擇檔案和SEP檔案";
        } else if (!state->selected_file_path) {
            error_msg = "請先選擇檔案";
        } else {
            error_msg = "請先選擇SEP檔案";
        }

        gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);

        if (state->altitude_text_buffer) {
            gtk_text_buffer_set_text(state->altitude_text_buffer, error_msg, -1);
        }
        return;
    }

    // 快速檢查文件是否存在
    FILE *test_file = fopen(state->selected_file_path, "r");
    if (!test_file) {
        char *error_msg = g_strdup_printf("主要檔案不存在或無法讀取: %s", state->selected_file_path);
        gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);
        if (state->altitude_text_buffer) {
            gtk_text_buffer_set_text(state->altitude_text_buffer, error_msg, -1);
        }
        g_free(error_msg);
        return;
    }
    fclose(test_file);

    test_file = fopen(state->selected_sep_path, "r");
    if (!test_file) {
        char *error_msg = g_strdup_printf("SEP檔案不存在或無法讀取: %s", state->selected_sep_path);
        gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);
        if (state->altitude_text_buffer) {
            gtk_text_buffer_set_text(state->altitude_text_buffer, error_msg, -1);
        }
        g_free(error_msg);
        return;
    }
    fclose(test_file);

    // 準備多線程處理數據
    ElevationProcessData *process_data = g_new(ElevationProcessData, 1);
    process_data->app_state = state;
    process_data->result_text = g_string_new("");
    process_data->error = NULL;
    process_data->input_path = g_strdup(state->selected_file_path);
    process_data->sep_path = g_strdup(state->selected_sep_path);
    process_data->current_progress = 0.0;
    strcpy(process_data->progress_text, "準備處理...");

    // 更新按鈕狀態（禁用處理按鈕）
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "convert_button")), FALSE);

    // 設置狀態
    gtk_label_set_text(GTK_LABEL(state->status_label), "開始高程轉換...");

    // 重置進度條
    if (state->elevation_progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->elevation_progress_bar), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(state->elevation_progress_bar), "準備處理...");
    }

    // 創建工作線程
    GThread *worker_thread = g_thread_new("elevation_worker",
                                         elevation_conversion_worker,
                                         process_data);

    // 不等待線程結束，讓它在背景運行
    g_thread_unref(worker_thread);
}
