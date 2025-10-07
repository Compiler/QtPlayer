cmake_minimum_required(VERSION 3.16)

project(QtPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appQtPlayer
    main.cpp
)

qt_add_qml_module(appQtPlayer
    URI QtPlayer
    VERSION 1.0
    QML_FILES
        Main.qml
        RESOURCES Assets/256x256_test.png
)


find_package(PkgConfig REQUIRED)
set(PKG_CONFIG_EXECUTABLE "/opt/homebrew/bin/pkg-config" CACHE FILEPATH "Force brew pkg-config")

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)
target_include_directories(appQtPlayer PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(appQtPlayer PRIVATE ${OpenCV_LIBS})


message(STATUS "Using pkg-config: ${PKG_CONFIG_EXECUTABLE}")
message(STATUS "Env PATH seen by CMake: $ENV{PATH}")
execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --version
                OUTPUT_VARIABLE _pcver OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "pkg-config version: ${_pcver}")
execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable pc_path pkg-config
                OUTPUT_VARIABLE _pcpath OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "pkg-config pc_path: ${_pcpath}")
execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --modversion libavformat
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _v ERROR_VARIABLE _e
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
message(STATUS "libavformat probe rc=${_rc} ver='${_v}' err='${_e}'")

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavformat
  libavcodec
  libavutil
  libswscale
  libswresample
)

target_include_directories(appQtPlayer PRIVATE
  ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(appQtPlayer PRIVATE
  PkgConfig::FFMPEG
)

target_compile_definitions(appQtPlayer PRIVATE
  __STDC_CONSTANT_MACROS
  __STDC_LIMIT_MACROS
)

set_target_properties(appQtPlayer PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appQtPlayer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appQtPlayer
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS appQtPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

