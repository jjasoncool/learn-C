#define _GNU_SOURCE  // 啟用 GNU 擴展，例如 getline 函數
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "callbacks.h"
#include "scan.h"

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
