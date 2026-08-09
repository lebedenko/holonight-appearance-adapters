cmake_minimum_required(VERSION 3.25)
project(HoloNightAppearanceAdapters VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_GLIB REQUIRED IMPORTED_TARGET json-glib-1.0)

add_library(holonight_appearance_consumer STATIC
  src/consumer.cpp
  src/tier1_projection.cpp
)
target_include_directories(holonight_appearance_consumer PUBLIC include)
target_link_libraries(holonight_appearance_consumer PUBLIC PkgConfig::JSON_GLIB)

set(HOLONIGHT_QT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../holonight-qt/build/tests/package-install-test/destdir/usr" CACHE PATH
    "Build or install prefix containing HolonightQtConfig.cmake")
set(HOLONIGHT_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../holonight-config/build/tests/consumer-prefix" CACHE PATH
    "Build or install prefix containing HoloNightConfigConfig.cmake")
list(PREPEND CMAKE_PREFIX_PATH "${HOLONIGHT_QT_DIR}" "${HOLONIGHT_CONFIG_DIR}")
find_package(HolonightQt CONFIG REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui)

add_library(holonight_appearance_qt_bridge STATIC src/qt_bridge.cpp)
target_include_directories(holonight_appearance_qt_bridge PUBLIC include)
target_link_libraries(holonight_appearance_qt_bridge PUBLIC HolonightQt::Theme Qt6::Core Qt6::Gui)

option(BUILD_GTK_PROBES "Build isolated GTK Tier 1 probe binaries" ON)
if(BUILD_GTK_PROBES)
  pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
  pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
  add_executable(holonight_gtk3_probe probes/gtk3_probe.cpp)
  target_link_libraries(holonight_gtk3_probe PRIVATE PkgConfig::GTK3)
  add_executable(holonight_gtk4_probe probes/gtk4_probe.cpp)
  target_link_libraries(holonight_gtk4_probe PRIVATE PkgConfig::GTK4)
endif()

if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  add_executable(consumer_tests tests/consumer_tests.cpp)
  target_link_libraries(consumer_tests PRIVATE holonight_appearance_consumer GTest::gtest_main)
  add_test(NAME consumer_tests COMMAND consumer_tests)

  add_executable(qt_bridge_tests tests/qt_bridge_tests.cpp)
  target_link_libraries(qt_bridge_tests PRIVATE holonight_appearance_qt_bridge
                                                holonight_appearance_consumer GTest::gtest_main)
  add_test(NAME qt_bridge_tests COMMAND qt_bridge_tests)

  add_test(NAME no_mutations COMMAND ${CMAKE_COMMAND} -P
           ${CMAKE_CURRENT_SOURCE_DIR}/tests/no_mutations.cmake)
  if(BUILD_GTK_PROBES)
    find_program(XVFB_RUN xvfb-run)
    find_program(DBUS_RUN_SESSION dbus-run-session)
    if(XVFB_RUN AND DBUS_RUN_SESSION)
      add_test(NAME gtk3_isolated COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_gtk_probe.sh
               ${DBUS_RUN_SESSION} ${XVFB_RUN} $<TARGET_FILE:holonight_gtk3_probe>)
      add_test(NAME gtk4_isolated COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_gtk_probe.sh
               ${DBUS_RUN_SESSION} ${XVFB_RUN} $<TARGET_FILE:holonight_gtk4_probe>)
    endif()
  endif()
endif()

find_program(CLANG_FORMAT clang-format)
find_program(CLANG_TIDY clang-tidy)
file(GLOB_RECURSE FORMAT_FILES CONFIGURE_DEPENDS include/*.h src/*.cpp tests/*.cpp probes/*.cpp)
if(CLANG_FORMAT)
  add_custom_target(format COMMAND ${CLANG_FORMAT} -i ${FORMAT_FILES})
  add_custom_target(format-check COMMAND ${CLANG_FORMAT} --dry-run --Werror ${FORMAT_FILES})
endif()
if(CLANG_TIDY)
  set(TIDY_DB_DIR ${CMAKE_BINARY_DIR}/tidy)
  file(MAKE_DIRECTORY ${TIDY_DB_DIR})
  file(WRITE ${CMAKE_BINARY_DIR}/prepare_tidy.cmake
    "file(READ \"${CMAKE_BINARY_DIR}/compile_commands.json\" db)\n"
    "string(REPLACE \"-mno-direct-extern-access\" \"\" db \"\${db}\")\n"
    "file(WRITE \"${TIDY_DB_DIR}/compile_commands.json\" \"\${db}\")\n")
  add_custom_target(tidy
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/prepare_tidy.cmake
    COMMAND ${CLANG_TIDY} -p ${TIDY_DB_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/consumer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tier1_projection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qt_bridge.cpp)
endif()

# CTV-005 targets intentionally have no install() rules pending CTV-007 acceptance.
