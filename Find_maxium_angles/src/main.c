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
    char *selected_folder_path;
} AppState;

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

    // 創建狀態標籤
    state->status_label = gtk_label_new("請選擇一個包含 TXT 檔案的資料夾");
    gtk_label_set_xalign(GTK_LABEL(state->status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->status_label, FALSE, FALSE, 0);

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

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    AppState *state = malloc(sizeof(AppState));
    if (!state) {
        fprintf(stderr, "Error: Failed to allocate AppState\n");
        return 1;
    }
    memset(state, 0, sizeof(AppState));  // 清零以避免垃圾

    app = gtk_application_new("com.example.txtprocessor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), state);
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // 清理
    free_app_state(state);
    g_object_unref(app);
    return status;
}
