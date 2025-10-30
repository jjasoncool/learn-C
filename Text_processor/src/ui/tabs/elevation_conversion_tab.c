// 高程转换标签页實現

#include <gtk/gtk.h>
#include "../../../include/callbacks.h"

// 構建高程轉換頁籤
void build_elevation_conversion_tab(AppState *state, GtkNotebook *notebook) {
    GtkWidget *tab_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(tab_vbox), 20);

    // 創建標題標籤
    GtkWidget *title_label = gtk_label_new("高程轉換工具");
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size='large' weight='bold'>高程轉換工具</span>");
    gtk_box_pack_start(GTK_BOX(tab_vbox), title_label, FALSE, FALSE, 0);

    // 創建分隔線
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(tab_vbox), separator, FALSE, FALSE, 0);

    // 創建按鈕區域
    GtkWidget *button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(tab_vbox), button_hbox, FALSE, FALSE, 0);

    // 創建選擇檔案按鈕
    GtkWidget *select_file_button = gtk_button_new_with_label("選擇檔案");
    gtk_widget_set_size_request(select_file_button, 120, 40);
    g_signal_connect(select_file_button, "clicked", G_CALLBACK(on_select_file), state);
    gtk_box_pack_start(GTK_BOX(button_hbox), select_file_button, FALSE, FALSE, 0);

    // 創建選擇SEP檔案按鈕
    GtkWidget *select_sep_button = gtk_button_new_with_label("選擇SEP檔案");
    gtk_widget_set_size_request(select_sep_button, 120, 40);
    g_signal_connect(select_sep_button, "clicked", G_CALLBACK(on_select_sep_file), state);
    gtk_box_pack_start(GTK_BOX(button_hbox), select_sep_button, FALSE, FALSE, 0);

    // 創建執行轉換按鈕
    GtkWidget *convert_button = gtk_button_new_with_label("執行轉換");
    gtk_widget_set_size_request(convert_button, 100, 40);
    g_signal_connect(convert_button, "clicked", G_CALLBACK(on_perform_conversion), state);
    gtk_box_pack_start(GTK_BOX(button_hbox), convert_button, FALSE, FALSE, 10);  // 多一些間距

    // 創建狀態標籤
    GtkWidget *status_label = gtk_label_new("請選擇要轉換的檔案和 SEP 檔案");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(tab_vbox), status_label, FALSE, FALSE, 0);

    // 添加進度條
    state->elevation_progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(state->elevation_progress_bar), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(state->elevation_progress_bar), "");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->elevation_progress_bar), 0.0);
    gtk_box_pack_start(GTK_BOX(tab_vbox), state->elevation_progress_bar, FALSE, FALSE, 5);

    // 創建結果顯示區域
    GtkWidget *result_label = gtk_label_new("結果:");
    gtk_label_set_xalign(GTK_LABEL(result_label), 0.0);
    gtk_label_set_markup(GTK_LABEL(result_label), "<span weight='bold'>結果:</span>");
    gtk_box_pack_start(GTK_BOX(tab_vbox), result_label, FALSE, FALSE, 0);

    // 創建可滾動的文字顯示區域
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 300);  // 保持原有的高度

    GtkWidget *result_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(result_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(result_text_view), GTK_WRAP_WORD);

    // 設置等寬字體（現代方式，使用CSS提供器）
    GtkCssProvider *provider = gtk_css_provider_new();
    const gchar *css = "textview { font-family: Monospace; font-size: 10pt; }";
    gtk_css_provider_load_from_data(provider, css, -1, NULL);

    GtkStyleContext *context = gtk_widget_get_style_context(result_text_view);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);

    // 設置state的高程轉換文本緩衝區
    state->altitude_text_view = result_text_view;
    state->altitude_text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result_text_view));

    gtk_container_add(GTK_CONTAINER(scrolled_window), result_text_view);
    gtk_box_pack_start(GTK_BOX(tab_vbox), scrolled_window, TRUE, TRUE, 0);

    // 創建頁籤標籤
    GtkWidget *tab_label = gtk_label_new("高程轉換");
    gtk_notebook_append_page(notebook, tab_vbox, tab_label);
}
