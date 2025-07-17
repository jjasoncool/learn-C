#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>

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
 * 設定處理狀態
 */
void set_processing_state(AppState *state, gboolean processing);

#endif // CALLBACKS_H
