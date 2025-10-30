#ifndef UI_H
#define UI_H

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

#endif // UI_H
