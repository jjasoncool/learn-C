#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>

// 應用狀態結構
typedef struct {
    GtkWidget *window;
    GtkWidget *folder_button;
    GtkWidget *cancel_button;
    GtkWidget *status_label;
    GtkWidget *result_text_view;
    GtkTextBuffer *text_buffer;
    GtkWidget *progress_bar;
    GtkWidget *progress_label;
    GtkWidget *progress_container;
    char *selected_folder_path;
    gboolean is_processing;     // 處理狀態標記
    gboolean cancel_requested;  // 取消請求標記
    GMutex cancel_mutex;        // 保護取消標記的互斥鎖
} AppState;

/**
 * 選擇資料夾的回調函數
 */
void on_folder_selected(GtkWidget *widget, gpointer data);

/**
 * 處理檔案的回調函數
 */
void on_process_files(GtkWidget *widget, gpointer data);

/**
 * 角度分析的回調函數
 */
void on_analyze_angles(GtkWidget *widget, gpointer data);

/**
 * 取消處理的回調函數
 */
void on_cancel_processing(GtkWidget *widget, gpointer data);

/**
 * 設定處理狀態
 */
void set_processing_state(AppState *state, gboolean processing);

/**
 * 檢查是否請求取消
 */
gboolean is_cancel_requested(AppState *state);

/**
 * 設定取消請求
 */
void set_cancel_requested(AppState *state, gboolean cancel);

/**
 * 初始化應用狀態
 */
void init_app_state(AppState *state);

/**
 * 清理應用狀態
 */
void cleanup_app_state(AppState *state);

#endif // CALLBACKS_H
