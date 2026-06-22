cmake_minimum_required(VERSION 3.18.3)

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
pkg_check_modules(GSTLIBS REQUIRED gobject-2.0 glib-2.0 gstreamer-1.0)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(NL3 REQUIRED libnl-3.0 libnl-genl-3.0)

add_library(view src/view.c)
target_include_directories(view PUBLIC include ${GSTLIBS_INCLUDE_DIRS}
                                       ${NL3_INCLUDE_DIRS})
target_link_libraries(view PUBLIC PkgConfig::GTK ${GSTLIBS_LIBRARIES}
                                  ${NL3_LIBRARIES})

add_library(batman src/batman.c)
target_include_directories(batman PUBLIC include ${GSTLIBS_INCLUDE_DIRS}
                                         ${NL3_INCLUDE_DIRS})
target_link_libraries(batman PUBLIC PkgConfig::GTK ${GSTLIBS_LIBRARIES}
                                    ${NL3_LIBRARIES})

add_executable(meshvision ${CMAKE_SOURCE_DIR}/src/main.c)

target_include_directories(
  meshvision PRIVATE view batman ${GSTLIBS_INCLUDE_DIRS} ${NL3_INCLUDE_DIRS})

target_link_libraries(meshvision PRIVATE view batman ${GSTLIBS_LIBRARIES}
                                         PkgConfig::GTK ${NL3_LIBRARIES})
