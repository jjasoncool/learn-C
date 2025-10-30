// 数据转换标签页實現

#include <gtk/gtk.h>

// 構建資料轉換頁籤 (預留框架)
void build_data_conversion_tab(GtkNotebook *notebook) {
    GtkWidget *tab_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(tab_vbox), 20);

    GtkWidget *label = gtk_label_new("資料轉換功能\n(開發中...)");
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(tab_vbox), label, TRUE, TRUE, 0);

    GtkWidget *tab_label = gtk_label_new("資料轉換");
    gtk_notebook_append_page(notebook, tab_vbox, tab_label);
}
