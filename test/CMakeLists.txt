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
