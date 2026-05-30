cmake_minimum_required(VERSION 3.12)

set(CMAKE_INSTALL_PREFIX /usr)

project(logiops)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_NONE "${CMAKE_CXX_FLAGS_NONE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall -Wextra")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_USER_BUS "Uses user bus" OFF)

find_package(Git)

# Set version number (ipcgull is vendored in-tree, not a submodule)
if(EXISTS ${PROJECT_SOURCE_DIR}/.git AND GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
         OUTPUT_VARIABLE LOGIOPS_VERSION
         RESULT_VARIABLE LOGIOPS_VERSION_RET
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
         ERROR_QUIET)
    if(NOT LOGIOPS_VERSION_RET EQUAL "0")
        execute_process(COMMAND ${GIT_EXECUTABLE}
            rev-parse --short HEAD
            OUTPUT_VARIABLE LOGIOPS_COMMIT_HASH)
        set(LOGIOPS_VERSION git-${LOGIOPS_COMMIT_HASH})
    endif()

    string(REGEX REPLACE "\n$" "" LOGIOPS_VERSION ${LOGIOPS_VERSION})
elseif(EXISTS ${PROJECT_SOURCE_DIR}/version.txt)
    file(READ version.txt LOGIOPS_VERSION)
    string(REGEX REPLACE "\n$" "" LOGIOPS_VERSION ${LOGIOPS_VERSION})

endif()

IF(NOT EXISTS ${PROJECT_SOURCE_DIR}/src/ipcgull)
    message(FATAL_ERROR "Missing src/ipcgull (vendored in-tree)")
endif()

if(NOT LOGIOPS_VERSION)
    set(LOGIOPS_VERSION "null")
endif()
message("LogiOps Version Number: ${LOGIOPS_VERSION}")

add_definitions( -DLOGIOPS_VERSION="${LOGIOPS_VERSION}")

if(USE_USER_BUS)
    add_definitions(-DUSE_USER_BUS)
endif()

add_subdirectory(src/ipcgull)
add_subdirectory(src/logid)

# Optional Qt 6 GUI client (logiops-gui). The daemon above does NOT depend on
# Qt; the GUI is guarded so a host without Qt6 (or -DBUILD_GUI=OFF) still builds
# the daemon and runs the test suite. find_package is QUIET (never REQUIRED) and
# a missing Qt6 only WARNs — this is the load-bearing "GUI is optional" contract.
option(BUILD_GUI "Build the Qt6 logiops-gui client" ON)
if(BUILD_GUI)
    find_package(Qt6 6.4 COMPONENTS Core Gui Qml Quick DBus Svg Test QUIET)
    if(Qt6_FOUND)
        add_subdirectory(src/logiops-gui)
    else()
        message(WARNING "Qt6 not found — skipping logiops-gui (daemon still builds)")
    endif()
endif()

option(BUILD_TESTING "Build the logiops test suite" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
