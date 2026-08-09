#include "holonight/gtk3_palette.h"

#include <gtk/gtk.h>

#include <array>
#include <cmath>
#include <iostream>

namespace {

Holonight::Adapters::Snapshot fixture() {
  Holonight::Adapters::Snapshot result;
  result.contract_version = Holonight::Adapters::kContractVersion;
  for (std::size_t index = 0; index < result.colors.size(); ++index) {
    result.colors[index] = {static_cast<std::uint8_t>(index + 10), static_cast<std::uint8_t>(index + 40),
                            static_cast<std::uint8_t>(index + 70), static_cast<std::uint8_t>(index + 100)};
  }
  return result;
}

void parsingError(GtkCssProvider *, GtkCssSection *, GError *error, gpointer data) {
  *static_cast<bool *>(data) = true;
  std::cerr << "GTK 3 CSS diagnostic: " << error->message << '\n';
}

bool close(double actual, std::uint8_t expected) {
  return std::abs(actual - (static_cast<double>(expected) / 255.0)) < 0.0001;
}

bool lookup(GtkStyleContext *context, const char *name, Holonight::Adapters::Color expected) {
  GdkRGBA actual{};
  return gtk_style_context_lookup_color(context, name, &actual) && close(actual.red, expected.red) &&
         close(actual.green, expected.green) && close(actual.blue, expected.blue) &&
         close(actual.alpha, expected.alpha);
}

} // namespace

int main(int argc, char **argv) {
  gtk_init(&argc, &argv);
  GtkSettings *settings = gtk_settings_get_default();
  if (settings == nullptr) {
    return 2;
  }

  const auto snapshot = fixture();
  const std::string css = Holonight::Adapters::generateGtk3PaletteCss(snapshot);
  GtkCssProvider *provider = gtk_css_provider_new();
  bool parse_error = false;
  g_signal_connect(provider, "parsing-error", G_CALLBACK(parsingError), &parse_error);
  gtk_css_provider_load_from_data(provider, css.c_str(), static_cast<gssize>(css.size()), nullptr);
  if (parse_error) {
    g_object_unref(provider);
    return 3;
  }
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  GtkWidget *button = gtk_button_new();
  GtkStyleContext *context = gtk_widget_get_style_context(button);
  constexpr std::array indices{2U, 13U, 8U, 15U, 19U, 21U, 22U, 23U};
  for (const auto index : indices) {
    const std::string name = "holonight_" + std::string(Holonight::Adapters::kColorRoleNames[index]);
    if (!lookup(context, name.c_str(), snapshot.colors[index])) {
      std::cerr << "GTK 3 failed to observe " << name << '\n';
      gtk_widget_destroy(button);
      g_object_unref(provider);
      return 4;
    }
  }
  gtk_widget_destroy(button);
  g_object_unref(provider);
  return 0;
}
