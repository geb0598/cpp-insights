include(FetchContent)

# -----------------------------------------------
# ImGui
# -----------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC d3d11 dxgi)

# -----------------------------------------------
# ImPlot
# -----------------------------------------------
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16 
)
FetchContent_MakeAvailable(implot)

add_library(implot STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
)

target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})

target_link_libraries(implot PUBLIC imgui)

# -----------------------------------------------
# cpp-insights-viewer
# -----------------------------------------------
add_executable(cpp-insights-viewer WIN32
    src/main.cpp
    src/flame_panel.cpp
    src/realtime_panel.cpp
    src/stack_panel.cpp
    src/timeline_panel.cpp
    src/toolbar_panel.cpp
    src/viewer.cpp
    data/cpp-insights.rc
)

target_include_directories(cpp-insights-viewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cpp-insights-viewer
    PRIVATE
        cpp-insights-core
        implot
        imgui
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/JetBrainsMonoNerdFont-Regular.ttf")
    add_custom_command(TARGET cpp-insights-viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/JetBrainsMonoNerdFont-Regular.ttf"
            "$<TARGET_FILE_DIR:cpp-insights-viewer>/JetBrainsMonoNerdFont-Regular.ttf"
    )
endif()

# The viewer ships as a Release build only. The font's license (OFL.txt) must
# accompany it on redistribution.
install(TARGETS cpp-insights-viewer
    RUNTIME
        DESTINATION    .
        CONFIGURATIONS Release
        COMPONENT      viewer
)

install(
    FILES
        data/JetBrainsMonoNerdFont-Regular.ttf
        ${CMAKE_SOURCE_DIR}/LICENSE
        ${CMAKE_SOURCE_DIR}/OFL.txt
    DESTINATION    .
    CONFIGURATIONS Release
    COMPONENT      viewer
)
