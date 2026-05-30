# Phase 3, Wave 0 — the BLOCKING test infrastructure every later Phase 3 plan
# depends on. Three CTest units under the `phase3` name prefix:
#
#   phase3_save_roundtrip   CONF-01 fidelity gate (no structure/value loss)
#   phase3_save_unknownkey  unknown-key resilience (no crash on a schema-unknown key)
#   phase3_keyname_mapper   BTN-02 Qt key/modifier -> evdev KEY_* mapping
#
# The two save() round-trip units exercise the daemon's config serialization
# WITHOUT a live bus or polkit: they compile the daemon's config translation
# unit (config/config.cpp) directly and link ipcgull_static (for the non-inline
# base_property::notify_change + variant machinery the schema's
# ipcgull::property<std::string> member pulls in) plus libconfig (config++).
# This is the exact code path save() runs after the polkit gate
# (Configuration.cpp:125-127), so it is a faithful round-trip.

# ---------------------------------------------------------------------------
# Round-trip + unknown-key units (daemon-side, no Qt required)
# ---------------------------------------------------------------------------

# A writable scratch dir for fixture cfg files; passed to the test as a macro so
# nothing ever touches /etc/logid.cfg (T-3-00-04 = accept).
set(PHASE3_TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(PHASE3_CONFIG_TU ${CMAKE_SOURCE_DIR}/src/logid/config/config.cpp
                     ${CMAKE_SOURCE_DIR}/src/logid/util/log.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/phase3_log_stub.cpp)
set(PHASE3_CONFIG_INCLUDES
        ${CMAKE_SOURCE_DIR}/src/logid
        ${CMAKE_SOURCE_DIR}/src/ipcgull/src/include)

foreach(rt save_roundtrip save_unknownkey)
    add_executable(phase3_${rt}_test ${rt}_test.cpp ${PHASE3_CONFIG_TU})
    target_include_directories(phase3_${rt}_test PRIVATE ${PHASE3_CONFIG_INCLUDES})
    target_compile_definitions(phase3_${rt}_test PRIVATE
            BUILD_TMP_DIR="${PHASE3_TMP_DIR}")
    # ipcgull_static gives base_property::notify_change + variant (the schema's
    # ipcgull::property<std::string> Device::default_profile needs them);
    # config++ is libconfig.
    target_link_libraries(phase3_${rt}_test PRIVATE ipcgull_static config++)
    set_target_properties(phase3_${rt}_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON)
endforeach()

add_test(NAME phase3_save_roundtrip  COMMAND phase3_save_roundtrip_test)
add_test(NAME phase3_save_unknownkey COMMAND phase3_save_unknownkey_test)

# ---------------------------------------------------------------------------
# KeyNameMapper unit (GUI-side, QtTest) — added in Task 2 (BTN-02).
# Guarded so a Qt-less / -DBUILD_GUI=OFF build still builds the daemon round-trip
# units above. logiops-gui-lib is only defined when the root build entered
# src/logiops-gui (Qt6 found), so gate on that target existing AND the test
# source being present.
if(TARGET logiops-gui-lib AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/KeyNameMapperTest.cpp)
    find_package(Qt6 6.4 COMPONENTS Test QUIET)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_executable(phase3_keyname_mapper_test KeyNameMapperTest.cpp)
        target_link_libraries(phase3_keyname_mapper_test PRIVATE
                logiops-gui-lib Qt6::Test)
        set_target_properties(phase3_keyname_mapper_test PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON)
        add_test(NAME phase3_keyname_mapper COMMAND phase3_keyname_mapper_test)
    endif()
endif()

# ---------------------------------------------------------------------------
# DeviceController unit (GUI-side, QtTest) — Plan 01 (capability + DPI-bounds
# spine). Driven via the construction seams with no live bus (Phase 2
# DeviceModelTest precedent). Gated identically on the GUI lib existing.
# ---------------------------------------------------------------------------
if(TARGET logiops-gui-lib AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/DeviceControllerTest.cpp)
    find_package(Qt6 6.4 COMPONENTS Test QUIET)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_executable(phase3_device_controller_test DeviceControllerTest.cpp)
        target_link_libraries(phase3_device_controller_test PRIVATE
                logiops-gui-lib Qt6::Test)
        set_target_properties(phase3_device_controller_test PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON)
        add_test(NAME phase3_device_controller COMMAND phase3_device_controller_test)
    endif()
endif()
