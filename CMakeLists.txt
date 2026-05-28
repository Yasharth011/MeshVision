cmake_minimum_required(VERSION 3.28.3)

project(
  Mesh
  VERSION 0.1
  LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_C_STANDARD_IMPLICIT_DIRECTORIES})
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(
  GSTLIBS
  REQUIRED
  gobject-2.0
  glib-2.0
  gstreamer-1.0)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk4)

add_executable(main ${CMAKE_SOURCE_DIR}/src/main.c)

target_include_directories(main PRIVATE ${GSTLIBS_INCLUDE_DIRS})

target_link_libraries(main PRIVATE ${GSTLIBS_LIBRARIES} PkgConfig::GTK)
