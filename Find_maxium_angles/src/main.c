#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_processor.h"

// 應用狀態結構
typedef struct {
    GtkWidget *window;
    GtkWidget *folder_button;
    GtkWidget *status_label;
    GtkWidget *result_text_view;
    GtkTextBuffer *text_buffer;
    GtkWidget *progress_bar;
    GtkWidget *progress_label;
    GtkWidget *progress_container;
    char *selected_folder_path;
    gboolean is_processing;  // 處理狀態標記
} AppState;

// 異步處理資料結構
typedef struct {
    AppState *app_state;
    char *folder_path;
    char *output_file;
    AngleAnalysisResult result;
    int max_search_success;
    char *max_result_file_path;
} AsyncProcessData;

// 函數聲明
static void on_folder_selected(GtkWidget *widget, gpointer data);
static void on_process_files(GtkWidget *widget, gpointer data);
static void on_analyze_angles(GtkWidget *widget, gpointer data);
static void progress_callback(int current, int total, const char *filename, void *user_data);
static gpointer angle_analysis_thread(gpointer data);
static gboolean angle_analysis_finished(gpointer data);
static void set_processing_state(AppState *state, gboolean processing);
static void free_async_process_data(AsyncProcessData *data);

// 選擇資料夾的回調函數
static void on_folder_selected(GtkWidget *widget, gpointer data) {
    (void)widget;  // 暫時壓警告，但之後修根源
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
static void on_process_files(GtkWidget *widget, gpointer data) {
    (void)widget;  // 暫時壓警告
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
    // 假設 format_scan_result 修改為用 GString
    format_scan_result_gstring(&result, state->selected_folder_path, display_text);
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
static void on_analyze_angles(GtkWidget *widget, gpointer data) {
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

    // 設定處理狀態
    set_processing_state(state, TRUE);
    gtk_label_set_text(GTK_LABEL(state->status_label), "開始角度分析...");

    // 準備異步處理資料
    AsyncProcessData *async_data = g_new0(AsyncProcessData, 1);
    async_data->app_state = state;
    async_data->folder_path = g_strdup(state->selected_folder_path);
    async_data->output_file = g_strdup("angle_analysis_result.txt");
    async_data->max_search_success = 0;
    async_data->max_result_file_path = NULL;

    // 啟動工作執行緒
    GThread *thread = g_thread_new("angle_analysis", angle_analysis_thread, async_data);
    g_thread_unref(thread); // 讓執行緒自動清理
}

// 應用程序啟動時的回調函數
static void activate(GtkApplication *app, gpointer user_data) {
    AppState *state = (AppState *)user_data;

    // 創建主窗口
    state->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state->window), "TXT 檔案處理工具");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(state->window), GTK_WIN_POS_CENTER);

    // 創建主容器（垂直盒子）
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 20);
    gtk_container_add(GTK_CONTAINER(state->window), main_vbox);

    // 創建標題標籤
    GtkWidget *title_label = gtk_label_new("TXT 檔案處理工具");
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size='x-large' weight='bold'>TXT 檔案處理工具</span>");
    gtk_box_pack_start(GTK_BOX(main_vbox), title_label, FALSE, FALSE, 0);

    // 創建分隔線
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_vbox), separator1, FALSE, FALSE, 0);

    // 創建按鈕區域
    GtkWidget *button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), button_hbox, FALSE, FALSE, 0);

    // 創建選擇資料夾按鈕
    state->folder_button = gtk_button_new_with_label("選擇資料夾");
    gtk_widget_set_size_request(state->folder_button, 120, 40);
    g_signal_connect(state->folder_button, "clicked", G_CALLBACK(on_folder_selected), state);
    gtk_box_pack_start(GTK_BOX(button_hbox), state->folder_button, FALSE, FALSE, 0);

    // 創建處理檔案按鈕
    GtkWidget *process_button = gtk_button_new_with_label("掃描 TXT 檔案");
    gtk_widget_set_size_request(process_button, 120, 40);
    g_signal_connect(process_button, "clicked", G_CALLBACK(on_process_files), state);
    gtk_box_pack_start(GTK_BOX(button_hbox), process_button, FALSE, FALSE, 0);

    // 創建角度分析按鈕（包含最大角度查找）
    GtkWidget *angle_button = gtk_button_new_with_label("分析角度並找最大值");
    gtk_widget_set_size_request(angle_button, 150, 40);
    g_signal_connect(angle_button, "clicked", G_CALLBACK(on_analyze_angles), state);
    gtk_box_pack_start(GTK_BOX(button_hbox), angle_button, FALSE, FALSE, 0);

    // 創建狀態標籤
    state->status_label = gtk_label_new("請選擇一個包含 TXT 檔案的資料夾");
    gtk_label_set_xalign(GTK_LABEL(state->status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->status_label, FALSE, FALSE, 0);

    // 創建進度條容器（初始隱藏）
    state->progress_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->progress_container, FALSE, FALSE, 0);

    // 創建進度標籤
    state->progress_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(state->progress_label), 0.0);
    gtk_box_pack_start(GTK_BOX(state->progress_container), state->progress_label, FALSE, FALSE, 0);

    // 創建進度條
    state->progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(state->progress_bar), TRUE);
    gtk_box_pack_start(GTK_BOX(state->progress_container), state->progress_bar, FALSE, FALSE, 0);

    // 初始隱藏進度容器
    gtk_widget_hide(state->progress_container);

    // 創建另一個分隔線
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_vbox), separator2, FALSE, FALSE, 0);

    // 創建結果顯示區域
    GtkWidget *result_label = gtk_label_new("結果:");
    gtk_label_set_xalign(GTK_LABEL(result_label), 0.0);
    gtk_label_set_markup(GTK_LABEL(result_label), "<span weight='bold'>結果:</span>");
    gtk_box_pack_start(GTK_BOX(main_vbox), result_label, FALSE, FALSE, 0);

    // 創建可滾動的文字顯示區域
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 300);

    state->result_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->result_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->result_text_view), GTK_WRAP_WORD);
    state->text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->result_text_view));

    gtk_container_add(GTK_CONTAINER(scrolled_window), state->result_text_view);
    gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);

    // 顯示所有元件
    gtk_widget_show_all(state->window);
}

// 清理 AppState 的函數
static void free_app_state(AppState *state) {
    if (state) {
        g_free(state->selected_folder_path);
        free(state);
    }
}

// 進度更新資料結構
typedef struct {
    AppState *state;
    double progress;
    gchar *text;
} ProgressUpdateData;

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

    // 計算進度百分比
    double progress = (double)current / total;

    // 建立進度資訊字串
    gchar *progress_text = g_strdup_printf("處理檔案 %d/%d: %s", current, total, filename);

    // 建立進度更新資料
    ProgressUpdateData *update_data = g_new(ProgressUpdateData, 1);
    update_data->state = state;
    update_data->progress = progress;
    update_data->text = progress_text;

    // 在主執行緒中更新 UI
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, update_progress_ui, update_data, g_free);
}

// 設定處理狀態
static void set_processing_state(AppState *state, gboolean processing) {
    state->is_processing = processing;

    // 控制按鈕是否可用
    gtk_widget_set_sensitive(state->folder_button, !processing);

    // 顯示或隱藏進度條
    if (processing) {
        gtk_widget_show(state->progress_container);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), 0.0);
        gtk_label_set_text(GTK_LABEL(state->progress_label), "準備開始處理...");
    } else {
        gtk_widget_hide(state->progress_container);
    }
}

// 工作執行緒函數
static gpointer angle_analysis_thread(gpointer data) {
    AsyncProcessData *async_data = (AsyncProcessData *)data;

    // 執行角度分析（帶進度回調）
    async_data->result = process_angle_files_with_progress(
        async_data->folder_path,
        async_data->output_file,
        progress_callback,
        async_data
    );

    // 如果角度分析成功，執行最大角度搜尋
    if (async_data->result.success) {
        gchar *result_file_path = g_build_filename(async_data->folder_path, "angle_analysis_result.txt", NULL);
        async_data->max_result_file_path = g_build_filename(async_data->folder_path, "max_angle_result.txt", NULL);

        async_data->max_search_success = find_max_angle_difference(result_file_path, async_data->max_result_file_path);
        g_free(result_file_path);
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
        char *error_msg = g_strdup_printf("Angle analysis failed: %s", result->error ? result->error : "Unknown error");
        gtk_label_set_text(GTK_LABEL(state->status_label), error_msg);
        gtk_text_buffer_set_text(state->text_buffer, error_msg, -1);
        g_free(error_msg);
        free_async_process_data(async_data);
        return FALSE;
    }

    // 格式化結果顯示
    GString *display_text = g_string_new("");
    g_string_append_printf(display_text, "Angle Analysis Results:\n");
    g_string_append_printf(display_text, "===========================================\n");
    g_string_append_printf(display_text, "Folder: %s\n\n", async_data->folder_path);

    if (result->count == 0) {
        g_string_append(display_text, "No valid angle data found\n");
    } else {
        g_string_append_printf(display_text, "Found %d angle data groups:\n\n", result->count);

        for (int i = 0; i < result->count; i++) {
            AngleRange *range = &result->ranges[i];
            g_string_append_printf(display_text,
                "profile: %d\n"
                "  bin range: %d ~ %d\n"
                "  angle range: %.2f ~ %.2f\n"
                "  angle difference: %.2f\n\n",
                range->first_num,
                range->min_second, range->max_second,
                range->min_third, range->max_third,
                range->angle_diff);
        }

        g_string_append_printf(display_text, "===========================================\n");
        g_string_append_printf(display_text, "Results saved to: angle_analysis_result.txt\n");
        g_string_append_printf(display_text, "File format: profile bin angle\n");
    }

    // 處理最大角度結果
    if (async_data->max_search_success && async_data->max_result_file_path) {
        FILE *max_output_file = fopen(async_data->max_result_file_path, "r");
        if (max_output_file) {
            g_string_append_printf(display_text, "\n\n");
            g_string_append_printf(display_text, "===========================================\n");
            g_string_append_printf(display_text, "Maximum Angle Difference Analysis:\n");
            g_string_append_printf(display_text, "===========================================\n");

            char line[256];
            while (fgets(line, sizeof(line), max_output_file)) {
                g_string_append(display_text, line);
            }
            fclose(max_output_file);

            g_string_append_printf(display_text, "\nResults also saved to: max_angle_result.txt\n");
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

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    AppState *state = malloc(sizeof(AppState));
    if (!state) {
        fprintf(stderr, "Error: Failed to allocate AppState\n");
        return 1;
    }
    memset(state, 0, sizeof(AppState));  // 清零以避免垃圾
    state->is_processing = FALSE;  // 初始化處理狀態

    app = gtk_application_new("com.example.txtprocessor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), state);
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // 清理
    free_app_state(state);
    g_object_unref(app);
    return status;
}
