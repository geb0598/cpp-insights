cmake_minimum_required(VERSION 3.16)
project(cpp-insights VERSION 1.0.0 LANGUAGES CXX)

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