#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include "angle_parser.h" // 為了 AngleAnalysisResult

// 高程數據結構，用於文件解析
typedef struct {
    char datetime[50];      // 日期時間字串
    double tide;           // 潮位
    double longitude;      // 經度
    double latitude;       // 緯度
    double processed_depth; // 處理深度
    double col6, col7;     // 第6、7欄數值
} TideDataRow;

// 應用狀態結構
typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;        // 主頁籤容器
    GtkWidget *folder_button;
    GtkWidget *cancel_button;
    GtkWidget *status_label;
    GtkWidget *result_text_view;
    GtkTextBuffer *text_buffer;         // 角度分析的文字緩衝區
    GtkWidget *altitude_text_view;      // 高程轉換的文字視圖
    GtkTextBuffer *altitude_text_buffer; // 高程轉換的文字緩衝區
    GtkWidget *elevation_progress_bar;  // 高程轉換專用進度條
    GtkWidget *progress_bar;
    GtkWidget *progress_label;
    GtkWidget *progress_container;
    char *selected_folder_path;
    char *selected_file_path;   // 高程轉換選擇的檔案路徑
    char *selected_sep_path;    // 高程轉換選擇的SEP檔案路徑
    gboolean is_processing;     // 處理狀態標記
    gboolean cancel_requested;  // 取消請求標記
    GMutex cancel_mutex;        // 保護取消標記的互斥鎖
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

// 高程轉換相關回調函數

/**
 * 選擇檔案的回調函數
 */
void on_select_file(GtkWidget *widget, gpointer data);

/**
 * 選擇SEP檔案的回調函數
 */
void on_select_sep_file(GtkWidget *widget, gpointer data);

/**
 * 執行高程轉換的回調函數
 */
void on_perform_conversion(GtkWidget *widget, gpointer data);

/**
 * 解析Tide數據行的工具函數
 */
gboolean parse_tide_data_row(const char *line, TideDataRow *row);

#endif // CALLBACKS_H
