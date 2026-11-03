cmake_minimum_required(VERSION 3.16)
# Releases are tagged v<VERSION>; release.yml refuses a tag that does not match.
project(cpp-insights VERSION 0.1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------
# cpp-insights-core (header only)
# -----------------------------------------------
add_library(cpp-insights-core
    STATIC
        src/insights/client.cpp
        src/insights/connection.cpp
        src/insights/pipe_transport.cpp
        src/insights/reporter.cpp
        src/insights/server.cpp
)

target_include_directories(cpp-insights-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(cpp-insights-core PUBLIC cxx_std_17)

# -----------------------------------------------
# cpp-insights-gpu (static library)
# -----------------------------------------------
if (WIN32)
    add_library(cpp-insights-gpu STATIC
        src/insights/gpu/gpu_profiler.cpp
        src/insights/gpu/d3d11_gpu_profiler_backend.cpp
    )

    target_include_directories(cpp-insights-gpu
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(cpp-insights-gpu
        PUBLIC cpp-insights-core
        PRIVATE d3d11
    )
else()
    message(STATUS "cpp-insights-gpu: Windows only, skipping on this platform")
endif()

# -----------------------------------------------
# cpp-insights-viewer 
# -----------------------------------------------

option(INSIGHTS_BUILD_VIEWER "Build viewer" ON)
if(INSIGHTS_BUILD_VIEWER)
    add_subdirectory(viewer)
endif()

# -----------------------------------------------
# Install / package
# -----------------------------------------------
# The SDK holds Debug and Release libraries side by side, so both configurations
# must be built before installing or packaging:
#   cpack --config build/debug/CPackConfig.cmake -C "Debug;Release"
install(DIRECTORY include/insights DESTINATION include COMPONENT sdk)
install(FILES LICENSE DESTINATION . COMPONENT sdk)

set(INSIGHTS_SDK_TARGETS cpp-insights-core)
if (TARGET cpp-insights-gpu)
    list(APPEND INSIGHTS_SDK_TARGETS cpp-insights-gpu)
endif()

foreach(config Debug Release)
    install(TARGETS ${INSIGHTS_SDK_TARGETS}
        ARCHIVE
            DESTINATION    lib/${config}
            CONFIGURATIONS ${config}
            COMPONENT      sdk
    )
endforeach()

# Debug objects are compiled with /Zi and reference their compiler PDB, which sits
# next to the .lib. Without it, consumers get LNK4099 and cannot step into the SDK.
foreach(target ${INSIGHTS_SDK_TARGETS})
    install(FILES "$<TARGET_FILE_DIR:${target}>/${target}.pdb"
        DESTINATION    lib/Debug
        CONFIGURATIONS Debug
        COMPONENT      sdk
    )
endforeach()

set(CPACK_GENERATOR                               ZIP)
set(CPACK_PACKAGE_FILE_NAME                       "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_ARCHIVE_COMPONENT_INSTALL               ON)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY    ON)
set(CPACK_COMPONENTS_ALL                          sdk)
if(INSIGHTS_BUILD_VIEWER)
    list(APPEND CPACK_COMPONENTS_ALL viewer)
endif()
include(CPack)

# -----------------------------------------------
# Tests
# -----------------------------------------------
option(INSIGHTS_BUILD_TESTS "Build tests" OFF)
if(INSIGHTS_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    set(CMAKE_CTEST_ARGUMENTS "--no-compress-output")

    set(TEST_SOURCES
        tests/unit/test_archive.cpp
        tests/unit/test_pipe_transport.cpp
        tests/unit/test_registry.cpp
        tests/unit/test_reporter.cpp
        tests/unit/test_scope_profiler.cpp
        tests/unit/test_session.cpp
    )

    add_executable(insights-test ${TEST_SOURCES})

    target_include_directories(insights-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_compile_definitions(insights-test
        PRIVATE
            INSIGHTS
    )

    target_link_libraries(insights-test
        PRIVATE
            cpp-insights-core
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(insights-test)

    # gtest_discover_tests runs every test in its own process, which hides state
    # leaking between tests. Also run the whole suite in one shuffled process.
    add_test(NAME insights-test.single-process
             COMMAND insights-test --gtest_shuffle --gtest_repeat=10)

    add_subdirectory(tests/integration/game_stub)
    add_subdirectory(tests/integration/gpu_stub)
endif()