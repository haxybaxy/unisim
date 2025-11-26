#include "ui/main_window.hpp"
#include <gtk/gtk.h>

static void activate(GtkApplication* app, gpointer user_data) {
    unisim::MainWindow* window = new unisim::MainWindow(app);
    window->show();
}

int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("com.unisim", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}

