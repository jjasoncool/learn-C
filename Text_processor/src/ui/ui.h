#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <gtk/gtk.h>
#include "callbacks.h"

/**
 * 應用程序啟動時的回調函數
 */
void activate(GtkApplication *app, gpointer user_data);

/**
 * 清理 AppState 的函數
 */
void free_app_state(AppState *state);

// Tab 模組聲明
void build_angle_analysis_tab(AppState *state, GtkNotebook *notebook);
void build_elevation_conversion_tab(AppState *state, GtkNotebook *notebook);
void build_data_conversion_tab(GtkNotebook *notebook);

#endif // UI_MAIN_H
