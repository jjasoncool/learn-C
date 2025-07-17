#include <gtk/gtk.h>
#include "ui.h"
#include "callbacks.h"

// 應用程序啟動時的回調函數
void activate(GtkApplication *app, gpointer user_data) {
    AppState *state = (AppState *)user_data;

    if (!state) {
        g_printerr("Error: activate called with NULL state\n");
        return;
    }

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
void free_app_state(AppState *state) {
    if (state) {
        g_free(state->selected_folder_path);
        free(state);
    }
}
