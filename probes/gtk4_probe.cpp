#include "holonight/gtk4_palette.h"

#include <array>
#include <cmath>
#include <gtk/gtk.h>
#include <iostream>

namespace {

Holonight::Adapters::Snapshot fixture() {
  Holonight::Adapters::Snapshot result;
  result.contract_version = Holonight::Adapters::kContractVersion;
  for (std::size_t index = 0; index < result.colors.size(); ++index) {
    result.colors.at(index) = {
        .red = static_cast<std::uint8_t>(index + 10),
        .green = static_cast<std::uint8_t>(index + 40),
        .blue = static_cast<std::uint8_t>(index + 70),
        .alpha = static_cast<std::uint8_t>(index + 100),
    };
  }
  return result;
}

void parsingError([[maybe_unused]] GtkCssProvider* provider, [[maybe_unused]] GtkCssSection* section,
                  const GError* error, gpointer data) {
  *static_cast<bool*>(data) = true;
  std::cerr << "GTK 4 CSS diagnostic: " << error->message << '\n';
}

bool close(float actual, std::uint8_t expected) {
  return std::abs(actual - (static_cast<float>(expected) / 255.0F)) < 0.0001F;
}

// Named CSS colors remain observable through GtkStyleContext across GTK 4.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
bool lookup(GtkStyleContext* context, const char* name, Holonight::Adapters::Color expected) {
  GdkRGBA actual{};
  return gtk_style_context_lookup_color(context, name, &actual) != 0 && close(actual.red, expected.red) &&
         close(actual.green, expected.green) && close(actual.blue, expected.blue) &&
         close(actual.alpha, expected.alpha);
}

#pragma GCC diagnostic pop

}  // namespace

int main() {
  gtk_init();
  GdkDisplay* display = gdk_display_get_default();
  if (display == nullptr) {
    return 2;
  }

  const auto snapshot = fixture();
  const std::string css = Holonight::Adapters::generateGtk4PaletteCss(snapshot);
  GtkCssProvider* provider = gtk_css_provider_new();
  bool parse_error = false;
  g_signal_connect(provider, "parsing-error", G_CALLBACK(parsingError), &parse_error);
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_css_provider_load_from_string(provider, css.c_str());
#else
  gtk_css_provider_load_from_data(provider, css.c_str(), static_cast<gssize>(css.size()));
#endif
  if (parse_error) {
    g_object_unref(provider);
    return 3;
  }
  gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  GtkWidget* button = gtk_button_new();
  g_object_ref_sink(button);
  // This probe verifies arbitrary generated named colors, beyond widget foreground.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  GtkStyleContext* context = gtk_widget_get_style_context(button);
#pragma GCC diagnostic pop
  constexpr std::array indices{2U, 13U, 8U, 15U, 19U, 21U, 22U, 23U};
  for (const auto index : indices) {
    const std::string name = "holonight_" + std::string(Holonight::Adapters::kColorRoleNames.at(index));
    if (!lookup(context, name.c_str(), snapshot.colors.at(index))) {
      std::cerr << "GTK 4 failed to observe " << name << '\n';
      g_object_unref(button);
      g_object_unref(provider);
      return 4;
    }
  }
  g_object_unref(button);
  g_object_unref(provider);
  return 0;
}
