include(FetchContent)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9b
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(glfw imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw opengl32)

# --------------------------------------------------
# Engine: no GUI or device dependencies (usable headless)
# --------------------------------------------------
file(GLOB ENGINE_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp)
add_library(halcyon_engine STATIC ${ENGINE_SRC})
target_include_directories(halcyon_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(halcyon_engine PUBLIC NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
if (MSVC)
    target_compile_options(halcyon_engine PUBLIC /fp:fast /W3)
endif()

# --------------------------------------------------
# App
# --------------------------------------------------
file(GLOB HOST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/host/*.cpp)
file(GLOB UI_SRC   CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp)

add_executable(Halcyon WIN32
    main.cpp
    ${HOST_SRC}
    ${UI_SRC}
    ${PROJECT_SOURCE_DIR}/src/platform/RtMidi.cpp)
target_compile_definitions(Halcyon PRIVATE __WINDOWS_MM__)
target_link_libraries(Halcyon PRIVATE halcyon_engine imgui winmm)
set_target_properties(Halcyon PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Headless tool: render presets to WAV, benchmark, self-test
add_executable(halcyon_cli tools/cli.cpp tools/MiniaudioEncoder.cpp)
target_link_libraries(halcyon_cli PRIVATE halcyon_engine)
