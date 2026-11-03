// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include <csignal>
#include <cstdio>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t reloads = 0;
void reload(int) { reloads = 1; }
}  // namespace
int main() {
  std::signal(SIGHUP, reload);
  std::puts("ready");
  std::fflush(stdout);
  for (;;) {
    if (reloads != 0) {
      reloads = 0;
      std::puts("reload");
      std::fflush(stdout);
    }
    usleep(1000);
  }
}
