#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "callbacks.h"

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    AppState *state = malloc(sizeof(AppState));
    if (!state) {
        fprintf(stderr, "Error: Failed to allocate AppState\n");
        return 1;
    }
    memset(state, 0, sizeof(AppState));  // 清零以避免垃圾
    state->is_processing = FALSE;  // 初始化處理狀態

    app = gtk_application_new("com.example.txtprocessor", G_APPLICATION_DEFAULT_FLAGS);
    if (!app) {
        fprintf(stderr, "Error: Failed to create GTK application\n");
        free_app_state(state);
        return 1;
    }

    g_signal_connect(app, "activate", G_CALLBACK(activate), state);
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // 清理
    free_app_state(state);
    g_object_unref(app);
    return status;
}
