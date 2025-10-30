#include <gtk/gtk.h>
#include "ui.h"
#include "callbacks.h"

// Tab 模組宣告
void build_angle_analysis_tab(AppState *state, GtkNotebook *notebook);
void build_file_processing_tab(AppState *state, GtkNotebook *notebook);
void build_data_conversion_tab(GtkNotebook *notebook);

// 靜態函數聲明
static void build_ui(AppState *state);

// 構建用戶界面
static void build_ui(AppState *state) {
    if (!state || !state->window) {
        g_printerr("Error: build_ui called with invalid state\n");
        return;
    }

    // 創建主容器（垂直盒子）
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 20);
    gtk_container_add(GTK_CONTAINER(state->window), main_vbox);

    // 創建標題標籤
    GtkWidget *title_label = gtk_label_new("TXT 檔案處理工具");
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size='x-large' weight='bold'>TXT 檔案處理工具</span>");
    gtk_box_pack_start(GTK_BOX(main_vbox), title_label, FALSE, FALSE, 0);

    // 創建分隔線
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_vbox), separator, FALSE, FALSE, 0);

    // 創建筆記本容器
    state->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), state->notebook, TRUE, TRUE, 0);

    // 構建各個頁籤
    build_angle_analysis_tab(state, GTK_NOTEBOOK(state->notebook));
    build_elevation_conversion_tab(state, GTK_NOTEBOOK(state->notebook));
    build_data_conversion_tab(GTK_NOTEBOOK(state->notebook));
}

// 應用程序啟動時的回調函數
void activate(GtkApplication *app, gpointer user_data) {
    AppState *state = (AppState *)user_data;

    if (!state) {
        g_printerr("Error: activate called with NULL state\n");
        return;
    }

    // 創建主窗口
    state->window = gtk_application_window_new(app);
    if (!state->window) {
        g_printerr("Error: Failed to create application window\n");
        return;
    }

    gtk_window_set_title(GTK_WINDOW(state->window), "TXT 檔案處理工具");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(state->window), GTK_WIN_POS_CENTER);

    // 構建界面
    build_ui(state);

    // 顯示所有控件
    gtk_widget_show_all(state->window);

    // 隱藏進度容器和取消按鈕（因為 show_all 會顯示所有控件）
    gtk_widget_hide(state->progress_container);
    gtk_widget_hide(state->cancel_button);
}

// 清理 AppState 的函數
void free_app_state(AppState *state) {
    if (state) {
        g_free(state->selected_folder_path);
        free(state);
    }
}
