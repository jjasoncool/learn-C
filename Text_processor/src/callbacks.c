#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "callbacks.h"
#include "scan.h"
#include "angle_parser.h"
#include "max_finder.h"
#include "elevation_processing.h"

// 延遲捲動用的數據結構
typedef struct {
    GtkTextView *text_view;
    GtkTextBuffer *buffer;
} ScrollData;

// 延遲捲動的回調函數
static gboolean delayed_scroll_to_end(gpointer user_data) {
    ScrollData *ctx = (ScrollData *)user_data;

    if (ctx->text_view && GTK_IS_TEXT_VIEW(ctx->text_view)) {
        // 確保視圖未被銷毀
        GtkTextIter scroll_iter;
        gtk_text_buffer_get_end_iter(ctx->buffer, &scroll_iter);
        gtk_text_view_scroll_to_iter(ctx->text_view, &scroll_iter, 0.0, TRUE, 0.0, 1.0);

        // 確保變化生效
        gtk_widget_queue_draw(GTK_WIDGET(ctx->text_view));
    }

    g_free(ctx);
    return FALSE; // 只執行一次
}

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

        // 同時控制高程轉換的停止按鈕
        GtkWidget *stop_button = GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "elevation_stop_button"));
        if (stop_button) {
            gtk_widget_set_sensitive(stop_button, TRUE);
        } else {
            // 如果找不到全域存儲的按鈕，嘗試從當前的notebook頁籤中找到
            if (state->notebook) {
                GtkWidget *current_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(state->notebook),
                                                                   gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook)));
                if (current_page) {
                    stop_button = GTK_WIDGET(g_object_get_data(G_OBJECT(current_page), "stop_button"));
                    if (stop_button) {
                        gtk_widget_set_sensitive(stop_button, TRUE);
                        // 同時將其儲存到window級別供下次使用
                        g_object_set_data(G_OBJECT(state->window), "elevation_stop_button", stop_button);
                    }
                }
            }
        }
    } else {
        gtk_widget_hide(state->progress_container);
        gtk_widget_hide(state->cancel_button);
        // 重置取消標記
        set_cancel_requested(state, FALSE);

        // 同時控制高程轉換的停止按鈕
        GtkWidget *stop_button = GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "elevation_stop_button"));
        if (stop_button) {
            gtk_widget_set_sensitive(stop_button, FALSE);
        }
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
// 潮汐資料格式描述結構 - 支援自訂格式
typedef struct {
    char delimiter;           // 欄位分隔符
    int datetime_delimiters;  // datetime欄位結束的分隔符數量
    int numeric_fields;       // 數值欄位數量
} TideFormat;

// 預設格式定義（當前使用的格式：datetime/tide/longitude/latitude/ProcessedDepth/col6/col7）
const TideFormat CURRENT_TIDE_FORMAT = {
    .delimiter = '/',
    .datetime_delimiters = 4,
    .numeric_fields = 6
};

// 解析Tide數據行 —— 高速版（零配置 + 指標走訪 + strtod）
// 通用版本：支援自訂格式
// 說明：避免 g_strdup 與 sscanf，改用指標掃描與 strtod，顯著減少每行開銷。
//
// 格式限制：
// - datetime 必須有指定數量的分隔符結束
// - 數值欄位必須是有效的浮點數
// - 使用指定字符作為欄位分隔符
//
// 效能特點：
// - 零動態配置：無 malloc/free 呼叫
// - 指標走訪：直接在原字串操作
// - strtod 解析：高效浮點數轉換
gboolean parse_tide_data_row_ex(const char *line, TideDataRow *row,
                               const TideFormat *format) {
    if (!line || !row || !format) return FALSE;

    const char *p = line;

    // 1) 跳過前導空白
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;

    // 2) 找出 datetime 的結束位置（第 datetime_delimiters 個 delimiter）
    const char *q = p;
    int delimiter_count = 0;
    for (; *q; ++q) {
        if (*q == format->delimiter) {
            ++delimiter_count;
            if (delimiter_count == format->datetime_delimiters) break;
        }
    }
    if (delimiter_count != format->datetime_delimiters || *q != format->delimiter) {
        return FALSE; // 格式不含足夠的分隔符來結束 datetime
    }

    // 3) 複製 datetime（不配置臨時字串）
    size_t dt_len = (size_t)(q - p);
    if (dt_len == 0 || dt_len >= sizeof(row->datetime)) {
        return FALSE; // datetime 太長或為空
    }
    memcpy(row->datetime, p, dt_len);
    row->datetime[dt_len] = '\0';

    // 4) 依序解析指定數量的數值欄位
    p = q + 1; // 跳過 datetime 結束的分隔符

    char *end = NULL;

    // 根據數值欄位數量動態解析
    // 注意：目前實作假設欄位順序固定為 tide/longitude/latitude/processed_depth/col6/col7
    // 如果需要支援不同欄位順序，可以進一步擴展 TideFormat 結構
    if (format->numeric_fields >= 1) {
        row->tide = strtod(p, &end);
        if (end == p || (format->numeric_fields > 1 && *end != format->delimiter)) return FALSE;
        p = end + 1;
    }

    if (format->numeric_fields >= 2) {
        row->longitude = strtod(p, &end);
        if (end == p || (format->numeric_fields > 2 && *end != format->delimiter)) return FALSE;
        p = end + 1;
    }

    if (format->numeric_fields >= 3) {
        row->latitude = strtod(p, &end);
        if (end == p || (format->numeric_fields > 3 && *end != format->delimiter)) return FALSE;
        p = end + 1;
    }

    if (format->numeric_fields >= 4) {
        row->processed_depth = strtod(p, &end);
        if (end == p || (format->numeric_fields > 4 && *end != format->delimiter)) return FALSE;
        p = end + 1;
    }

    if (format->numeric_fields >= 5) {
        row->col6 = strtod(p, &end);
        if (end == p || (format->numeric_fields > 5 && *end != format->delimiter)) return FALSE;
        p = end + 1;
    }

    if (format->numeric_fields >= 6) {
        row->col7 = strtod(p, &end);
        if (end == p) return FALSE;
    }

    // 至此成功；尾端可能有換行或其他字元，無需特別處理
    return TRUE;
}

// 解析Tide數據行 —— 高速版（零配置 + 指標走訪 + strtod）
// 向後相容版本：使用預設格式
// 格式：datetime/tide/longitude/latitude/ProcessedDepth/col6/col7
// 說明：避免 g_strdup 與 sscanf，改用指標掃描與 strtod，顯著減少每行開銷。
gboolean parse_tide_data_row(const char *line, TideDataRow *row) {
    return parse_tide_data_row_ex(line, row, &CURRENT_TIDE_FORMAT);
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
                // 高程數據格式分析 - 簡化驗證結果顯示
                g_string_append_printf(display_text, "檔案格式: 高程數據 (7欄)\n");
                g_string_append_printf(display_text, "期望格式: datetime/tide/經度/緯度/ProcessedDepth/col6/col7\n\n");

                // 簡化格式驗證邏輯
                int valid_lines = 0;
                int total_data_lines = 0;
                int filtered_lines = 0;

                g_string_append_printf(display_text, "格式驗證結果:\n");
                g_string_append_printf(display_text, "================\n");

                // 分析每一行並統計格式驗證結果
                for (int i = 0; i < analysis_result->line_count; i++) {
                    if (g_strstrip(g_strdup(analysis_result->lines[i]))[0] == '\0') {
                        continue; // 跳過空行
                    }

                    total_data_lines++;

                    // 試著解析7欄數據
                    TideDataRow test_row;
                    if (parse_tide_data_row(analysis_result->lines[i], &test_row)) {
                        valid_lines++;

                        // 檢查過濾條件
                        if (test_row.col6 == 0.0 || test_row.col7 == 0.0) {
                            filtered_lines++;
                        }
                    }
                }

                // 顯示簡化的驗證結果
                if (total_data_lines == 0) {
                    g_string_append_printf(display_text, "❌ 無有效數據行\n");
                } else {
                    double valid_percentage = (double)valid_lines / total_data_lines * 100.0;
                    g_string_append_printf(display_text, "✅ 格式相符: %d/%d 行 (%.1f%%)\n",
                                         valid_lines, total_data_lines, valid_percentage);

                    if (filtered_lines > 0) {
                        g_string_append_printf(display_text, "⚠️  將被過濾: %d 行 (col6或col7為0)\n", filtered_lines);
                    }

                    int invalid_lines = total_data_lines - valid_lines;
                    if (invalid_lines > 0) {
                        g_string_append_printf(display_text, "❌ 格式不符: %d 行\n", invalid_lines);
                    }
                }

                g_string_append_printf(display_text, "\n前 %d 行樣本內容:\n", analysis_result->line_count);
                g_string_append_printf(display_text, "===========================\n");

                // 顯示前幾行的樣本內容
                for (int i = 0; i < MIN(analysis_result->line_count, 3); i++) {
                    g_string_append_printf(display_text, "第 %d 行: %s\n", i + 1, analysis_result->lines[i]);
                }

                if (analysis_result->line_count > 3) {
                    g_string_append_printf(display_text, "... (還有 %d 行)\n", analysis_result->line_count - 3);
                }

                g_string_append_printf(display_text, "\n===========================\n");
                g_string_append_printf(display_text, "總共分析了 %d 行 • 有效數據行: %d\n",
                                     analysis_result->line_count, valid_lines);

                // 添加詳細的格式建議
                g_string_append_printf(display_text, "\n📋 格式檢查清單:\n");
                if (valid_lines < total_data_lines * 0.8) { // 如果有效行少於80%
                    g_string_append_printf(display_text, "❌ 使用 '/' 作為分隔符\n");
                    g_string_append_printf(display_text, "❌ 確保每行有7個字段\n");
                } else {
                    g_string_append_printf(display_text, "✅ 使用 '/' 作為分隔符\n");
                    g_string_append_printf(display_text, "✅ 每行有7個字段\n");
                }
                g_string_append_printf(display_text, "ℹ️ 數值字段應有適當精確度\n");
                if (filtered_lines > 0) {
                    g_string_append_printf(display_text, "⚠️ col6與col7為0的行會被過濾\n");
                } else {
                    g_string_append_printf(display_text, "✅ 數據行不會被過濾\n");
                }

                g_string_append_printf(display_text, "\n🔧 常見問題修復:\n");
                g_string_append_printf(display_text, "• 檢查是否有額外空格或隱藏字符\n");
                g_string_append_printf(display_text, "• 確保datetime格式正確 (YYYY/MM/DD/HH:MM:SS.mmm)\n");
                g_string_append_printf(display_text, "• 確認數值字段沒有非數字字符\n");
                g_string_append_printf(display_text, "• 使用UTF-8編碼保存文件\n");

            } else {
                // 角度數據格式分析 - 保持原有的詳細顯示
                g_string_append_printf(display_text, "檔案格式: 角度數據 (變動欄位)\n\n");

                g_string_append_printf(display_text, "前 %d 行內容:\n", analysis_result->line_count);
                g_string_append_printf(display_text, "==================\n");

                for (int i = 0; i < analysis_result->line_count; i++) {
                    g_string_append_printf(display_text, "第 %d 行: %s\n", i + 1, analysis_result->lines[i]);
                }

                if (analysis_result->line_count == 0) {
                    g_string_append(display_text, "(檔案是空的)\n");
                }

                g_string_append_printf(display_text, "==================\n");
                g_string_append_printf(display_text, "總共分析了 %d 行\n", analysis_result->line_count);
            }

    // 更新界面 - 根據當前活動標籤頁選擇正確的文本緩衝區和視圖
    GtkTextBuffer *target_buffer = state->text_buffer;  // 預設使用角度分析的緩衝區
    GtkTextView *target_view = GTK_TEXT_VIEW(state->result_text_view);  // 預設使用角度分析的視圖

    // 檢查當前活動標籤頁
    if (state->notebook) {
        int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook));
        // 如果是高程轉換標籤頁（索引 1），使用高程緩衝區和視圖
        if (current_page == 1 && state->altitude_text_buffer && state->altitude_text_view) {
            target_buffer = state->altitude_text_buffer;
            target_view = GTK_TEXT_VIEW(state->altitude_text_view);
        }
    }

    // 儲存選擇的檔案路徑（用於高程轉換）
    g_free(state->selected_file_path);
    state->selected_file_path = g_strdup(filename);

    gtk_text_buffer_set_text(target_buffer, display_text->str, -1);
    g_string_free(display_text, TRUE);

    // 使用延遲捲動確保文本渲染完成後再捲動
    if (target_view) {
        ScrollData *scroll_data = g_new(ScrollData, 1);
        scroll_data->text_view = target_view;
        scroll_data->buffer = target_buffer;

        // 使用 g_idle_add 延遲捲動，確保文本完全渲染後再捲動
        g_idle_add((GSourceFunc)delayed_scroll_to_end, scroll_data);
    }

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

        // 在結果區域顯示SEP檔案確認訊息 (追加到現有文字後)
        GString *confirm_text = g_string_new("\n");
        g_string_append_printf(confirm_text, "SEP檔案確認:\n");
        g_string_append_printf(confirm_text, "================\n");
        g_string_append_printf(confirm_text, "SEP檔案已選擇\n");
        g_string_append_printf(confirm_text, "檔案路徑: %s\n", filename);
        g_string_append_printf(confirm_text, "\n此SEP檔案將用於高程轉換的地理空間插值處理。\n");

        // 根據當前活動標籤頁選擇正確的緩衝區和視圖
        GtkTextBuffer *target_buffer = state->text_buffer;
        GtkTextView *target_view = GTK_TEXT_VIEW(state->result_text_view);  // 修正為正確的成員名稱

        if (state->notebook) {
            int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook));
            // 如果是高程轉換標籤頁（索引 1），使用高程緩衝區和視圖
            if (current_page == 1 && state->altitude_text_buffer && state->altitude_text_view) {
                target_buffer = state->altitude_text_buffer;
                target_view = GTK_TEXT_VIEW(state->altitude_text_view);
            }
        }

        // 獲取現有文字並追加新訊息
        GtkTextIter start_iter, end_iter;
        gtk_text_buffer_get_start_iter(target_buffer, &start_iter);
        gtk_text_buffer_get_end_iter(target_buffer, &end_iter);
        char *existing_text = gtk_text_buffer_get_text(target_buffer, &start_iter, &end_iter, FALSE);

        GString *new_text = g_string_new(existing_text ? existing_text : "");
        g_string_append(new_text, confirm_text->str);

        gtk_text_buffer_set_text(target_buffer, new_text->str, -1);

        g_free(existing_text);
        g_string_free(confirm_text, TRUE);
        g_string_free(new_text, TRUE);

        // 使用延遲捲動確保文本渲染完成後再捲動
        if (target_view) {
            // 創建一個包含視圖引用的結構，供延遲函數使用
            ScrollData *scroll_data = g_new(ScrollData, 1);
            scroll_data->text_view = target_view;
            scroll_data->buffer = target_buffer;

            // 使用 g_idle_add 延遲捲動，確保文本完全渲染後再捲動
            g_idle_add((GSourceFunc)delayed_scroll_to_end, scroll_data);
        }

        char *status_text = g_strdup_printf("SEP檔案已確認: %s", filename);
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
        // 正確設置進度：將百分比轉為0.0-1.0範圍
        double fraction = data->current_progress / 100.0;
        fraction = CLAMP(fraction, 0.0, 1.0); // 確保範圍正確

        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->app_state->elevation_progress_bar), fraction);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(data->app_state->elevation_progress_bar), data->progress_text);
    }

    return FALSE; // 只執行一次
}

// 更新最終結果的回調函數（線程安全）
static gboolean update_result_callback(gpointer user_data) {
    ElevationProcessData *data = (ElevationProcessData*)user_data;
    AppState *state = data->app_state;

    // 恢復按鈕狀態 - 重新啟用執行按鈕，禁用停止按鈕
    GtkWidget *convert_button = GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "convert_button"));
    if (convert_button) {
        gtk_widget_set_sensitive(convert_button, TRUE);
    }

    GtkWidget *stop_button = GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "elevation_stop_button"));
    if (stop_button) {
        gtk_widget_set_sensitive(stop_button, FALSE);
    }

    // 重置處理狀態
    state->is_processing = FALSE;
    set_cancel_requested(state, FALSE);

    // 更新狀態標籤
    if (data->error) {
        const char *error_message = data->error->message ? data->error->message : "未知錯誤";
        gtk_label_set_text(GTK_LABEL(state->status_label), error_message);

        if (state->altitude_text_buffer) {
            // 檢查是否為取消錯誤
            if (g_error_matches(data->error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                // 取消情況：不清除結果框，只追加取消訊息
                GtkTextIter end_iter;
                gtk_text_buffer_get_end_iter(state->altitude_text_buffer, &end_iter);
                gtk_text_buffer_insert(state->altitude_text_buffer, &end_iter,
                                      "\n\n⚠️ 處理已被用戶取消", -1);

                // 更新進度條顯示取消狀態
                if (state->elevation_progress_bar) {
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->elevation_progress_bar), 0.0);
                    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(state->elevation_progress_bar), "處理已取消");
                }

                // 使用延遲捲動確保文本渲染完成後再捲動（模仿檔案選擇的捲動方式）
                if (state->altitude_text_view) {
                    ScrollData *scroll_data = g_new(ScrollData, 1);
                    scroll_data->text_view = GTK_TEXT_VIEW(state->altitude_text_view);
                    scroll_data->buffer = state->altitude_text_buffer;

                    // 使用 g_idle_add 延遲捲動，確保文本完全渲染後再捲動
                    g_idle_add((GSourceFunc)delayed_scroll_to_end, scroll_data);
                }
            } else {
                // 其他錯誤：追加錯誤訊息，而不是清除內容
                GtkTextIter end_iter;
                gtk_text_buffer_get_end_iter(state->altitude_text_buffer, &end_iter);
                gtk_text_buffer_insert(state->altitude_text_buffer, &end_iter,
                                      g_strdup_printf("\n\n❌ 錯誤：%s", error_message), -1);

                // 對於其他錯誤也捲動到底部
                if (state->altitude_text_view) {
                    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(state->altitude_text_view), &end_iter, 0.0, TRUE, 0.0, 1.0);
                }
            }
        }
    } else {
        // 成功完成：顯示完整結果
        gtk_label_set_text(GTK_LABEL(state->status_label), "高程轉換完成");

        if (state->altitude_text_buffer) {
            gtk_text_buffer_set_text(state->altitude_text_buffer, data->result_text->str, -1);
        }

        // 處理完成時維持進度條在100%
        if (state->elevation_progress_bar) {
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->elevation_progress_bar), 1.0);
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(state->elevation_progress_bar), "處理完成");
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
    // 添加取消檢查的包裝函數 - 直接在進度回調中強制終止處理

    void progress_callback_with_cancel(double percentage, const char *message) {
        // 先檢查取消請求 - 如果請求取消，立即設定錯誤（這會讓函數立即返回，並結束處理）
        if (is_cancel_requested(data->app_state)) {
            // 如果請求取消，設定錯誤，中斷當前處理
            g_print("[CANCEL] 強力取消：設定錯誤，中斷處理循環\n");
            g_set_error(&data->error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "操作已取消");
            return; // 不執行進度更新，讓錯誤向上傳播
        }

        // 正常執行進度更新
        progress_update_callback(percentage, message);
    }


    if (!process_elevation_conversion_with_callback(data->input_path, data->sep_path,
                                          data->result_text, &data->error,
                                          progress_callback_with_cancel)) {
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

    // 設定處理中狀態
    state->is_processing = TRUE;

    // 更新按鈕狀態（禁用處理按鈕，啟用停止按鈕）
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "convert_button")), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(state->window), "elevation_stop_button")), TRUE);

    // 設置狀態
    gtk_label_set_text(GTK_LABEL(state->status_label), "開始高程轉換...");

    // 在結果框內新增開始處理訊息
    if (state->altitude_text_buffer) {
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(state->altitude_text_buffer, &end_iter);
        gtk_text_buffer_insert(state->altitude_text_buffer, &end_iter,
                              "\n\n🚀 開始高程轉換處理...\n", -1);

        // 自動捲動到新訊息
        if (state->altitude_text_view) {
            ScrollData *scroll_data = g_new(ScrollData, 1);
            scroll_data->text_view = GTK_TEXT_VIEW(state->altitude_text_view);
            scroll_data->buffer = state->altitude_text_buffer;
            g_idle_add((GSourceFunc)delayed_scroll_to_end, scroll_data);
        }
    }

    // 重置進度條
    if (state->elevation_progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->elevation_progress_bar), 0.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(state->elevation_progress_bar), "準備處理...");
    }

    // 創建工作線程 - 支援強力取消
    GThread *worker_thread = g_thread_new("elevation_worker",
                                         elevation_conversion_worker,
                                         process_data);

    // 不等待線程結束，讓它在背景運行
    g_thread_unref(worker_thread);

    // 儲存線程參考以便強力取消
    g_object_set_data(G_OBJECT(state->window), "elevation_worker_thread", worker_thread);
}

// 取消處理的回調函數 (為高程轉換專用)
void on_cancel_processing(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    g_print("[DEBUG] on_cancel_processing called, is_processing=%d\n", state->is_processing);

    if (!state->is_processing) {
        g_print("[DEBUG] on_cancel_processing: not processing, return\n");
        return; // 沒有正在進行的處理
    }

    // 設定取消請求 (讓背景線程檢測到並自動終止)
    set_cancel_requested(state, TRUE);
    gtk_label_set_text(GTK_LABEL(state->status_label), "正在取消處理...");
    g_print("[DEBUG] on_cancel_processing: cancel requested set to TRUE\n");

    // 基本實現：只設定取消信號，讓背景線程自動檢查
    // 不使用 g_thread_join 避免阻塞UI線程
    // 線程會在下次檢查取消請求時自動終止並清理資源
}
