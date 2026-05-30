# logiops test suite (Phase 1, Wave 0)
#
# A single dependency-free C++ unit for the HID++ bounds helper, registered with
# CTest. It is RED-by-design until Plan 04 (ACCESS-04) adds the free inline
# `logid::backend::hidpp::hasHidppHeader()` to backend/hidpp/Report.h.
#
# The unit is header-only: it includes Report.h via the daemon's include dir and
# does NOT link the `logid` target (no libconfig/udev/evdev needed).

add_executable(hidpp_bounds_test hidpp_bounds_test.cpp)
target_include_directories(hidpp_bounds_test PRIVATE ${CMAKE_SOURCE_DIR}/src/logid)
set_target_properties(hidpp_bounds_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)
add_test(NAME hidpp_bounds COMMAND hidpp_bounds_test)

# Phase 2, Wave 0: RED Qt-based units (device_model + battery_feature). These are
# guarded behind Qt6 so the suite still builds/passes where Qt6 is absent (the
# daemon-only path). They are RED-by-design until Plans 02/03 land the
# implementations they #include.
find_package(Qt6 6.4 COMPONENTS Core DBus Test QUIET)
if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)

    # DeviceModel unit — links the GUI lib for the model under test (Plan 03).
    add_executable(device_model_test DeviceModelTest.cpp)
    target_link_libraries(device_model_test PRIVATE logiops-gui-lib Qt6::Test)
    set_target_properties(device_model_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON)
    add_test(NAME device_model COMMAND device_model_test)
    add_test(NAME device_model_noflicker COMMAND device_model_test test_status_noflicker)

    # BatteryStatus decoder unit — header-includes the daemon backend (Plan 02),
    # no link to the full logid target (mirrors hidpp_bounds_test).
    add_executable(battery_feature_test BatteryStatusTest.cpp)
    target_include_directories(battery_feature_test PRIVATE ${CMAKE_SOURCE_DIR}/src/logid)
    target_link_libraries(battery_feature_test PRIVATE Qt6::Test)
    set_target_properties(battery_feature_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON)
    add_test(NAME battery_feature COMMAND battery_feature_test)
else()
    message(WARNING "Qt6 not found — skipping device_model/battery_feature units")
endif()
