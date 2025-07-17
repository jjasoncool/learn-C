#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callbacks.h"
#include "scan.h"
#include "angle_parser.h"
#include "max_finder.h"

// 進度更新資料結構
typedef struct {
    AppState *state;
    double progress;
    gchar *text;
} ProgressUpdateData;

// 異步處理資料結構
typedef struct {
    AppState *app_state;
    char *folder_path;
    char *output_file;
    AngleAnalysisResult result;
    int max_search_success;
    char *max_result_file_path;
} AsyncProcessData;

// 靜態函數聲明
static gboolean update_progress_ui(gpointer data);
static void progress_callback(int current, int total, const char *filename, void *user_data);
static gpointer angle_analysis_thread(gpointer data);
static gboolean angle_analysis_finished(gpointer data);
static void free_async_process_data(AsyncProcessData *data);

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

// 在主執行緒中更新進度 UI
static gboolean update_progress_ui(gpointer data) {
    ProgressUpdateData *update_data = (ProgressUpdateData *)data;
    AppState *state = update_data->state;

    // 更新進度條
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), update_data->progress);

    // 更新進度標籤
    gtk_label_set_text(GTK_LABEL(state->progress_label), update_data->text);

    g_free(update_data->text);
    return FALSE; // 只執行一次
}

// 進度回調函數（在工作執行緒中調用）
static void progress_callback(int current, int total, const char *filename, void *user_data) {
    AsyncProcessData *async_data = (AsyncProcessData *)user_data;
    AppState *state = async_data->app_state;

    // 檢查是否請求取消
    if (is_cancel_requested(state)) {
        return; // 不再發送進度更新
    }

    // 計算進度百分比
    double progress = (double)current / total;

    // 建立進度資訊字串
    gchar *progress_text = g_strdup_printf("處理檔案 %d/%d: %s", current, total, filename);

    // 建立進度更新資料
    ProgressUpdateData *update_data = g_new(ProgressUpdateData, 1);
    if (!update_data) {
        g_printerr("Error: Failed to allocate progress update data\n");
        g_free(progress_text);
        return;
    }

    update_data->state = state;
    update_data->progress = progress;
    update_data->text = progress_text;

    // 在主執行緒中更新 UI
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, update_progress_ui, update_data, g_free);
}

// 工作執行緒函數
static gpointer angle_analysis_thread(gpointer data) {
    AsyncProcessData *async_data = (AsyncProcessData *)data;

    // 檢查是否在開始前就請求取消
    if (is_cancel_requested(async_data->app_state)) {
        async_data->result.success = 0;
        async_data->result.error = g_strdup("操作已取消");
        g_idle_add(angle_analysis_finished, async_data);
        return NULL;
    }

    // 執行角度分析（帶進度回調）
    async_data->result = process_angle_files_with_progress(
        async_data->folder_path,
        async_data->output_file,
        progress_callback,
        async_data
    );

    // 檢查是否在分析過程中請求取消
    if (is_cancel_requested(async_data->app_state)) {
        // 清理已分配的結果
        free_angle_analysis_result(&async_data->result);
        async_data->result.success = 0;
        async_data->result.error = g_strdup("操作已取消");
        g_idle_add(angle_analysis_finished, async_data);
        return NULL;
    }

    // 如果角度分析成功，執行最大角度搜尋
    if (async_data->result.success) {
        gchar *result_file_path = g_build_filename(async_data->folder_path, "angle_analysis_result.txt", NULL);
        if (result_file_path) {
            async_data->max_result_file_path = g_build_filename(async_data->folder_path, "max_angle_result.txt", NULL);
            if (async_data->max_result_file_path) {
                // 再次檢查取消請求
                if (!is_cancel_requested(async_data->app_state)) {
                    async_data->max_search_success = find_max_angle_difference(result_file_path, async_data->max_result_file_path);
                } else {
                    async_data->max_search_success = 0;
                }
            } else {
                g_printerr("Error: Failed to build max result file path\n");
                async_data->max_search_success = 0;
            }
            g_free(result_file_path);
        } else {
            g_printerr("Error: Failed to build result file path\n");
            async_data->max_search_success = 0;
        }
    }

    // 在主執行緒中完成處理
    g_idle_add(angle_analysis_finished, async_data);
    return NULL;
}

// 異步處理完成回調（在主執行緒中執行）
static gboolean angle_analysis_finished(gpointer data) {
    AsyncProcessData *async_data = (AsyncProcessData *)data;
    AppState *state = async_data->app_state;
    AngleAnalysisResult *result = &async_data->result;

    // 還原處理狀態
    set_processing_state(state, FALSE);

    if (!result->success) {
        char *error_msg = g_strdup_printf("角度分析失敗: %s", result->error ? result->error : "未知錯誤");
        gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);
        gtk_text_buffer_set_text(state->text_buffer, error_msg, -1);
        g_free(error_msg);
        free_async_process_data(async_data);
        return FALSE;
    }

    // 格式化結果顯示
    GString *display_text = g_string_new("");
    g_string_append_printf(display_text, "角度分析結果:\n");
    g_string_append_printf(display_text, "===========================================\n");
    g_string_append_printf(display_text, "資料夾: %s\n\n", async_data->folder_path);

    if (result->count == 0) {
        g_string_append(display_text, "未找到有效的角度資料\n");
    } else {
        g_string_append_printf(display_text, "找到 %d 組角度資料:\n\n", result->count);

        for (int i = 0; i < result->count; i++) {
            AngleRange *range = &result->ranges[i];
            g_string_append_printf(display_text,
                "profile: %d\n"
                "  bin 範圍: %d ~ %d\n"
                "  角度範圍: %.2f ~ %.2f\n"
                "  角度差值: %.2f\n\n",
                range->first_num,
                range->min_second, range->max_second,
                range->min_third, range->max_third,
                range->angle_diff);
        }

        g_string_append_printf(display_text, "===========================================\n");
        g_string_append_printf(display_text, "結果已儲存至: angle_analysis_result.txt\n");
        g_string_append_printf(display_text, "檔案格式: profile bin angle\n");
    }

    // 處理最大角度結果
    if (async_data->max_search_success && async_data->max_result_file_path) {
        FILE *max_output_file = fopen(async_data->max_result_file_path, "r");
        if (max_output_file) {
            g_string_append_printf(display_text, "\n\n");
            g_string_append_printf(display_text, "===========================================\n");

            char line[256];
            while (fgets(line, sizeof(line), max_output_file)) {
                g_string_append(display_text, line);
            }
            fclose(max_output_file);

            g_string_append_printf(display_text, "\n結果已儲存至: max_angle_result.txt\n");
            gtk_label_set_text(GTK_LABEL(state->status_label), "角度分析和最大角度搜尋完成！");
        } else {
            gtk_label_set_text(GTK_LABEL(state->status_label), "角度分析完成，但無法讀取最大角度結果");
        }
    } else {
        gtk_label_set_text(GTK_LABEL(state->status_label), "角度分析完成，但最大角度搜尋失敗");
    }

    // 顯示完整結果
    gtk_text_buffer_set_text(state->text_buffer, display_text->str, -1);
    g_string_free(display_text, TRUE);

    // 清理資源
    free_async_process_data(async_data);
    return FALSE; // 只執行一次
}

// 清理異步處理資料
static void free_async_process_data(AsyncProcessData *data) {
    if (data) {
        g_free(data->folder_path);
        g_free(data->output_file);
        g_free(data->max_result_file_path);
        free_angle_analysis_result(&data->result);
        g_free(data);
    }
}

// 選擇資料夾的回調函數
void on_folder_selected(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;

    dialog = gtk_file_chooser_dialog_new("選擇資料夾",
                                         GTK_WINDOW(state->window),
                                         action,
                                         "_取消",
                                         GTK_RESPONSE_CANCEL,
                                         "_選擇",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *folder_path;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        folder_path = gtk_file_chooser_get_filename(chooser);
        if (!folder_path) {
            gtk_label_set_text(GTK_LABEL(state->status_label), "選擇資料夾失敗：無法獲取路徑");
            g_printerr("Error: gtk_file_chooser_get_filename failed\n");
            gtk_widget_destroy(dialog);
            return;
        }

        // 更新選中的資料夾路徑
        g_free(state->selected_folder_path);
        state->selected_folder_path = g_strdup(folder_path);

        // 更新界面顯示
        char *status_text = g_strdup_printf("已選擇資料夾: %s", folder_path);
        gtk_label_set_text(GTK_LABEL(state->status_label), status_text);
        g_free(status_text);

        // 在結果區域顯示選中的路徑
        char *result_text = g_strdup_printf("選中的資料夾: %s\n", folder_path);
        gtk_text_buffer_set_text(state->text_buffer, result_text, -1);
        g_free(result_text);

        g_free(folder_path);
    }

    gtk_widget_destroy(dialog);
}

// 處理檔案的回調函數
void on_process_files(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    if (!state->selected_folder_path) {
        gtk_label_set_text(GTK_LABEL(state->status_label), "請先選擇一個資料夾！");
        return;
    }

    // 如果正在處理，則忽略
    if (state->is_processing) {
        gtk_label_set_text(GTK_LABEL(state->status_label), "正在處理中，請等待...");
        return;
    }

    gtk_label_set_text(GTK_LABEL(state->status_label), "正在掃描 TXT 檔案...");

    // 使用檔案處理模組掃描 TXT 檔案
    ScanResult result = scan_txt_files(state->selected_folder_path);

    if (!result.success) {
        char *error_msg = g_strdup_printf("掃描失敗: %s", result.error ? result.error : "未知錯誤");
        gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);
        g_printerr("Scan error: %s\n", result.error ? result.error : "unknown");
        gtk_text_buffer_set_text(state->text_buffer, error_msg, -1);
        g_free(error_msg);
        free_scan_result(&result);
        return;
    }

    // 格式化結果，使用 GString 動態建構
    GString *display_text = g_string_new("");
    format_scan_result(&result, state->selected_folder_path, display_text);
    gtk_text_buffer_set_text(state->text_buffer, display_text->str, -1);
    g_string_free(display_text, TRUE);

    // 更新狀態標籤
    char *status_text;
    if (result.count > 0) {
        status_text = g_strdup_printf("掃描完成，找到 %d 個 TXT 檔案", result.count);
    } else {
        status_text = g_strdup("掃描完成，未找到 TXT 檔案");
    }
    gtk_label_set_text(GTK_LABEL(state->status_label), status_text);
    g_free(status_text);

    // 釋放記憶體
    free_scan_result(&result);
}

// 角度分析的回調函數
void on_analyze_angles(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    if (!state->selected_folder_path) {
        gtk_label_set_text(GTK_LABEL(state->status_label), "請先選擇一個資料夾！");
        return;
    }

    // 如果正在處理，則忽略
    if (state->is_processing) {
        gtk_label_set_text(GTK_LABEL(state->status_label), "正在處理中，請等待...");
        return;
    }

    // 重置取消標記
    set_cancel_requested(state, FALSE);

    // 設定處理狀態
    set_processing_state(state, TRUE);
    gtk_label_set_text(GTK_LABEL(state->status_label), "開始角度分析...");

    // 準備異步處理資料
    AsyncProcessData *async_data = g_new0(AsyncProcessData, 1);
    if (!async_data) {
        g_printerr("Error: Failed to allocate async process data\n");
        set_processing_state(state, FALSE);
        gtk_label_set_text(GTK_LABEL(state->status_label), "記憶體分配失敗");
        return;
    }

    async_data->app_state = state;
    async_data->folder_path = g_strdup(state->selected_folder_path);
    async_data->output_file = g_strdup("angle_analysis_result.txt");
    async_data->max_search_success = 0;
    async_data->max_result_file_path = NULL;

    // 檢查記憶體分配
    if (!async_data->folder_path || !async_data->output_file) {
        g_printerr("Error: Failed to allocate strings in async data\n");
        free_async_process_data(async_data);
        set_processing_state(state, FALSE);
        gtk_label_set_text(GTK_LABEL(state->status_label), "記憶體分配失敗");
        return;
    }

    // 啟動工作執行緒
    GThread *thread = g_thread_new("angle_analysis", angle_analysis_thread, async_data);
    if (!thread) {
        g_printerr("Error: Failed to create analysis thread\n");
        free_async_process_data(async_data);
        set_processing_state(state, FALSE);
        gtk_label_set_text(GTK_LABEL(state->status_label), "無法建立工作執行緒");
        return;
    }
    g_thread_unref(thread); // 讓執行緒自動清理
}

// 取消處理的回調函數
void on_cancel_processing(GtkWidget *widget, gpointer data) {
    (void)widget;  // 壓制警告
    AppState *state = (AppState *)data;

    if (!state->is_processing) {
        return; // 沒有正在進行的處理
    }

    // 設定取消請求
    set_cancel_requested(state, TRUE);
    gtk_label_set_text(GTK_LABEL(state->status_label), "正在取消處理...");
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
