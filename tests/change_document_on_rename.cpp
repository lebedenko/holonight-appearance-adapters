// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>

namespace {
void changeDocument(const char* destination, int result) {
  static bool changed = false;
  const auto* state = std::getenv("CA_TEST_STATE_PATH");
  const auto* appearance = std::getenv("CA_TEST_APPEARANCE_PATH");
  if (result != 0 || changed || state == nullptr || appearance == nullptr || std::strcmp(destination, state) != 0) {
    return;
  }
  changed = true;
  if (const auto* target = std::getenv("CA_TEST_RETARGET")) {
    std::filesystem::remove(appearance);
    std::filesystem::create_symlink(target, appearance);
    return;
  }
  std::ofstream file{appearance};
  file << "version=2\n[theme]\nscheme='holonight-light' # external writer\n";
}
template <typename Function>
Function next(const char* name) {
  // POSIX defines dlsym's conversion to the interposed function type.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
}
}  // namespace
// Parameter names match the libc declarations for these interposed symbols.
extern "C" int rename(const char* __old, const char* __new) noexcept {
  const auto result = next<int (*)(const char*, const char*)>("rename")(__old, __new);
  changeDocument(__new, result);
  return result;
}
extern "C" int renameat(int __oldfd, const char* __old, int __newfd, const char* __new) noexcept {
  const auto result = next<int (*)(int, const char*, int, const char*)>("renameat")(__oldfd, __old, __newfd, __new);
  changeDocument(__new, result);
  return result;
}
extern "C" int renameat2(int __oldfd, const char* __old, int __newfd, const char* __new,
                         unsigned int __flags) noexcept {
  const auto result = next<int (*)(int, const char*, int, const char*, unsigned int)>("renameat2")(
      __oldfd, __old, __newfd, __new, __flags);
  changeDocument(__new, result);
  return result;
}
