#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全域變數
GtkWidget *window;
GtkWidget *folder_button;
GtkWidget *status_label;
GtkWidget *result_text_view;
GtkTextBuffer *text_buffer;
char *selected_folder_path = NULL;

// 選擇資料夾的回調函數
static void on_folder_selected(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;

    dialog = gtk_file_chooser_dialog_new("選擇資料夾",
                                        GTK_WINDOW(window),
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

        // 更新選中的資料夾路徑
        if (selected_folder_path) {
            g_free(selected_folder_path);
        }
        selected_folder_path = g_strdup(folder_path);

        // 更新界面顯示
        char status_text[512];
        snprintf(status_text, sizeof(status_text), "已選擇資料夾: %s", folder_path);
        gtk_label_set_text(GTK_LABEL(status_label), status_text);

        // 在結果區域顯示選中的路徑
        char result_text[1024];
        snprintf(result_text, sizeof(result_text), "選中的資料夾: %s\n", folder_path);
        gtk_text_buffer_set_text(text_buffer, result_text, -1);

        g_free(folder_path);
    }

    gtk_widget_destroy(dialog);
}

// 處理檔案的回調函數（暫時只是佔位符）
static void on_process_files(GtkWidget *widget, gpointer data) {
    if (!selected_folder_path) {
        gtk_label_set_text(GTK_LABEL(status_label), "請先選擇一個資料夾！");
        return;
    }

    gtk_label_set_text(GTK_LABEL(status_label), "正在處理檔案...");

    // 暫時顯示一個佔位符消息
    char result_text[1024];
    snprintf(result_text, sizeof(result_text),
             "選中的資料夾: %s\n\n"
             "功能開發中...\n"
             "將來這裡會顯示：\n"
             "- 找到的 txt 檔案列表\n"
             "- 處理結果\n",
             selected_folder_path);
    gtk_text_buffer_set_text(text_buffer, result_text, -1);

    gtk_label_set_text(GTK_LABEL(status_label), "處理完成（功能開發中）");
}

// 應用程序啟動時的回調函數
static void activate(GtkApplication *app, gpointer user_data) {
    // 創建主窗口
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "TXT 檔案處理工具");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // 創建主容器（垂直盒子）
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 20);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

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
    folder_button = gtk_button_new_with_label("選擇資料夾");
    gtk_widget_set_size_request(folder_button, 120, 40);
    g_signal_connect(folder_button, "clicked", G_CALLBACK(on_folder_selected), NULL);
    gtk_box_pack_start(GTK_BOX(button_hbox), folder_button, FALSE, FALSE, 0);

    // 創建處理檔案按鈕
    GtkWidget *process_button = gtk_button_new_with_label("處理檔案");
    gtk_widget_set_size_request(process_button, 120, 40);
    g_signal_connect(process_button, "clicked", G_CALLBACK(on_process_files), NULL);
    gtk_box_pack_start(GTK_BOX(button_hbox), process_button, FALSE, FALSE, 0);

    // 創建狀態標籤
    status_label = gtk_label_new("請選擇一個包含 TXT 檔案的資料夾");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(main_vbox), status_label, FALSE, FALSE, 0);

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

    result_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(result_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(result_text_view), GTK_WRAP_WORD);
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result_text_view));

    gtk_container_add(GTK_CONTAINER(scrolled_window), result_text_view);
    gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);

    // 顯示所有元件
    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.txtprocessor", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // 清理記憶體
    if (selected_folder_path) {
        g_free(selected_folder_path);
    }

    g_object_unref(app);
    return status;
}
