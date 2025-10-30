#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/ui.h"
#include "callbacks.h"

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    AppState *state = malloc(sizeof(AppState));
    if (!state) {
        fprintf(stderr, "Error: Failed to allocate AppState\n");
        return 1;
    }

    // 初始化應用狀態
    init_app_state(state);

    app = gtk_application_new("com.example.txtprocessor", G_APPLICATION_DEFAULT_FLAGS);
    if (!app) {
        fprintf(stderr, "Error: Failed to create GTK application\n");
        cleanup_app_state(state);
        free(state);
        return 1;
    }

    g_signal_connect(app, "activate", G_CALLBACK(activate), state);
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // 清理
    cleanup_app_state(state);
    free(state);
    g_object_unref(app);
    return status;
}
