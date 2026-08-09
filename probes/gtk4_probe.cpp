#include <gtk/gtk.h>

#include <iostream>

int main() {
  gtk_init();
  GtkSettings *settings = gtk_settings_get_default();
  if (settings == nullptr)
    return 2;
  gchar *icon_theme = nullptr;
  gboolean prefer_dark = FALSE;
  g_object_get(settings, "gtk-icon-theme-name", &icon_theme, "gtk-application-prefer-dark-theme", &prefer_dark,
               nullptr);
  std::cout << "startup icon=" << (icon_theme != nullptr ? icon_theme : "") << " dark=" << prefer_dark << '\n';
  g_object_set(settings, "gtk-application-prefer-dark-theme", !prefer_dark, nullptr);
  gboolean changed = prefer_dark;
  g_object_get(settings, "gtk-application-prefer-dark-theme", &changed, nullptr);
  g_free(icon_theme);
  std::cout << "live dark=" << changed << '\n';
  return changed == prefer_dark ? 3 : 0;
}
