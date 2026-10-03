// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "holonight/appearance.h"
#include "labwc_theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <holonight/config/appearance.h>
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (argc != 2 || !QDir().mkpath(QCoreApplication::arguments()[1])) {
    return 1;
  }
  const auto resolved = Holonight::resolveAppearance(HoloNight::Config::Appearance{});
  if (!resolved || !resolved.value.has_value()) {
    return 1;
  }
  const auto files = Holonight::Adapters::labwcTheme(Holonight::resolveSemanticAppearance(*resolved.value));
  for (auto it = files.begin(); it != files.end(); ++it) {
    QFile file(QCoreApplication::arguments()[1] + '/' + it.key());
    if (!file.open(QIODevice::WriteOnly) || file.write(it.value()) != it.value().size()) {
      return 1;
    }
  }
}
