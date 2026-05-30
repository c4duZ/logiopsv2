# logiops-gui — OPTIONAL Qt 6 + QML client target (Phase 2)
#
# This subdirectory is only entered when the root build found Qt6 (the root
# CMakeLists.txt guards `add_subdirectory(src/logiops-gui)` behind a QUIET
# find_package + BUILD_GUI option). The daemon therefore builds with no Qt
# dependency at all when Qt6 is absent or -DBUILD_GUI=OFF is passed.
#
# Wave 0 scaffold only: a static `logiops-gui-lib` that test/DeviceModelTest
# links against. It carries no real sources yet — Plan 03 adds DeviceModel.{h,cpp}
# (the QAbstractListModel device model) and Plan 04 adds main.cpp + the QML module
# + the `logiops-gui` executable.
project(logiops-gui)

# Qt meta-object / resource compilers (the model is a QObject with Q_OBJECT).
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Match the daemon's language standard exactly (CLAUDE.md: C++20 throughout).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static library so test/DeviceModelTest can link the model without pulling in
# main.cpp / QGuiApplication. Plan 03 appends DeviceModel.cpp via target_sources.
add_library(logiops-gui-lib STATIC)
target_sources(logiops-gui-lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/placeholder.cpp)
target_link_libraries(logiops-gui-lib PUBLIC Qt6::Core Qt6::DBus)
target_include_directories(logiops-gui-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Plan 04 adds the `logiops-gui` executable target here:
#   add_executable(logiops-gui main.cpp ...)  +  qt_add_qml_module(...)
#   target_link_libraries(logiops-gui PRIVATE logiops-gui-lib Qt6::Quick Qt6::Qml Qt6::Svg)
# (deferred — Wave 0 ships the lib only).
