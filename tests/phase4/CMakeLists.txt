# Phase 4, Wave 0 — the test infrastructure every later Phase 4 plan verifies
# against. Mirrors tests/phase3/CMakeLists.txt. Two CTest units under the
# `phase4` name prefix:
#
#   phase4_gesture_math    GEST-02/03 daemon-side interval/axis accounting
#                          (RED until Plan 01 fixes IntervalGesture default +
#                          accounting). Self-contained math unit — reimplements
#                          ONLY the boundary-crossing accounting under test, so it
#                          links nothing beyond the standard library and never
#                          pulls Action::makeAction / InputDevice / Device / a bus.
#                          It is daemon-side (no Qt) and encodes the exact contract
#                          Plan 01 must satisfy.
#
#   phase4_gesture_model   GEST-01/04 GUI recording-subclass scaffold for the
#                          two-step mode-switch + the preview sentence. RED now
#                          (won't link until Plan 02 lands GestureModel); the
#                          EXISTS guard below keeps the daemon math unit buildable
#                          regardless of GestureModel's absence.

# ---------------------------------------------------------------------------
# A writable scratch dir for any fixture files; passed to the units as a macro so
# nothing ever touches /etc/logid.cfg (T-04-00-01 = mitigate). Mirrors phase3.
# ---------------------------------------------------------------------------
set(PHASE4_TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(PHASE4_DAEMON_INCLUDES
        ${CMAKE_SOURCE_DIR}/src/logid
        ${CMAKE_SOURCE_DIR}/src/ipcgull/src/include)

# ---------------------------------------------------------------------------
# gesture_math unit (daemon-side, no Qt, no bus). The GEST-02/03 accounting
# harness. SELF-CONTAINED: the test file reimplements only the interval
# boundary-crossing + axis-remainder accounting under test, so it needs no link
# to Action::makeAction / InputDevice / Device. If a future plan instead compiles
# the real IntervalGesture.cpp / AxisGesture.cpp TUs, add util/log.cpp +
# phase4_log_stub.cpp here and link `ipcgull_static config++` (see phase3).
# ---------------------------------------------------------------------------
add_executable(phase4_gesture_math_test gesture_math_test.cpp)
target_include_directories(phase4_gesture_math_test PRIVATE ${PHASE4_DAEMON_INCLUDES})
target_compile_definitions(phase4_gesture_math_test PRIVATE
        BUILD_TMP_DIR="${PHASE4_TMP_DIR}")
set_target_properties(phase4_gesture_math_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)

add_test(NAME phase4_gesture_math COMMAND phase4_gesture_math_test)

# ---------------------------------------------------------------------------
# GestureModel unit (GUI-side, QtTest) — Plan 02 (GEST-01 / GEST-04). Drives the
# two-step mode-switch (SetGesture then per-mode .Gesture.<mode> param call) +
# preview-sentence logic via a RecordingGestureModel subclass with NO live bus
# (mirrors tests/phase3 ButtonsModelTest). RED now — GestureModel does not exist
# until Plan 02, so this unit won't link yet; the EXISTS guard keeps the daemon
# math unit buildable. Gated exactly like phase3: the GUI lib target must exist
# AND the source must be present AND Qt6 Test must be found.
# ---------------------------------------------------------------------------
if(TARGET logiops-gui-lib AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/GestureModelTest.cpp)
    find_package(Qt6 6.4 COMPONENTS Test QUIET)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_executable(phase4_gesture_model_test GestureModelTest.cpp)
        target_link_libraries(phase4_gesture_model_test PRIVATE
                logiops-gui-lib Qt6::Test)
        set_target_properties(phase4_gesture_model_test PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON)
        add_test(NAME phase4_gesture_model COMMAND phase4_gesture_model_test)
    endif()
endif()
