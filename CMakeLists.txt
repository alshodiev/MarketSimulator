cmake_minimum_required(VERSION 3.16)
project(MarketReplaySimulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Options ---
option(ENABLE_TESTING "Enable building tests" ON)
option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
option(BUILD_GUI_VERSION "Enable building the GUI version" ON)

# --- Compiler Flags ---
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(COMMON_CXX_FLAGS "-Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS}")
    if(ENABLE_SANITIZERS)
        set(SANITIZE_FLAGS "-fsanitize=address,undefined,thread -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZE_FLAGS}")
    endif()
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    if(ENABLE_SANITIZERS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    endif()
endif()

# --- Include Directories ---
include_directories(include)

# --- Dependencies ---
include(FetchContent)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# Dear ImGui
FetchContent_Declare(
    imgui_fc_decl # Changed name to avoid conflict if we don't use MakeAvailable
    SOURCE_DIR extern/imgui # Store it in a known subdirectory if you prefer more control
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4 
)
FetchContent_GetProperties(imgui_fc_decl) # Populates imgui_fc_decl_SOURCE_DIR and imgui_fc_decl_POPULATED
if(NOT imgui_fc_decl_POPULATED)
    FetchContent_Populate(imgui_fc_decl)
    message(STATUS "imgui source dir: ${imgui_fc_decl_SOURCE_DIR}")
endif()
# We will compile ImGui sources directly, so FetchContent_MakeAvailable(imgui) is not strictly needed
# unless other parts of ImGui's CMakeLists are essential (like setting include dirs for its own target)
# For direct source compilation, we mainly need imgui_fc_decl_SOURCE_DIR.
# Let's still call MakeAvailable as it's generally good practice, and we only skip linking its 'imgui' target.
FetchContent_MakeAvailable(imgui_fc_decl) # This will run ImGui's CMake and define 'imgui' target, but we won't link it.

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)


# --- Library for Core Components ---
file(GLOB_RECURSE CORE_SRC_FILES
    src/core/*.cpp
    src/io/*.cpp
    src/utils/*.cpp
)
add_library(market_replay_core ${CORE_SRC_FILES})
target_include_directories(market_replay_core PUBLIC include)
target_link_libraries(market_replay_core 
    PUBLIC spdlog::spdlog 
    PUBLIC Threads::Threads # Making Threads PUBLIC for GUI thread usage
)

# --- Strategy Implementations ---
file(GLOB_RECURSE STRATEGY_SRC_FILES src/strategy/*.cpp)

# --- Executable (CLI version) ---
add_executable(market_replay_sim_cli src/main.cpp ${STRATEGY_SRC_FILES})
target_link_libraries(market_replay_sim_cli PRIVATE market_replay_core) # Gets spdlog & Threads via market_replay_core

# --- Executable (GUI version) ---
if(BUILD_GUI_VERSION)
    message(STATUS "Building GUI version of the simulator.")
    set(GUI_MAIN_SRC src/gui_main.cpp)

    # ImGui backend files
    set(IMGUI_BACKEND_FILES
        ${imgui_fc_decl_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_fc_decl_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    # ImGui core source files
    set(IMGUI_CORE_SOURCES
        ${imgui_fc_decl_SOURCE_DIR}/imgui.cpp
        ${imgui_fc_decl_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_fc_decl_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_fc_decl_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_fc_decl_SOURCE_DIR}/imgui_demo.cpp # Needed for ImGui::ShowDemoWindow()
    )

    add_executable(market_replay_sim_gui 
        ${GUI_MAIN_SRC}
        ${IMGUI_BACKEND_FILES}
        ${IMGUI_CORE_SOURCES}   # <-- Compile ImGui sources directly
        ${STRATEGY_SRC_FILES}
    )

    target_include_directories(market_replay_sim_gui PRIVATE
        ${imgui_fc_decl_SOURCE_DIR}          # For imgui.h
        ${imgui_fc_decl_SOURCE_DIR}/backends # For imgui_impl_*.h
        # glfw includes are propagated by linking the glfw target
    )

    target_link_libraries(market_replay_sim_gui 
        PRIVATE 
        market_replay_core  
        # imgui               # <--- REMOVED this link, as sources are compiled in
        glfw               
        # Threads::Threads    # Transitive from market_replay_core (now public)
        # spdlog::spdlog    # Transitive from market_replay_core (public)
    )

    if(APPLE)
        target_link_libraries(market_replay_sim_gui PRIVATE "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(market_replay_sim_gui PRIVATE opengl32)
    else() 
        find_package(OpenGL REQUIRED)
        target_link_libraries(market_replay_sim_gui PRIVATE OpenGL::GL)
    endif()
else()
    message(STATUS "GUI version build is disabled (BUILD_GUI_VERSION=OFF).")
endif()


# --- Testing (Catch2) ---
if(ENABLE_TESTING)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()

# --- Output ---
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "Testing enabled: ${ENABLE_TESTING}")
message(STATUS "Sanitizers enabled: ${ENABLE_SANITIZERS}")
message(STATUS "Build GUI version: ${BUILD_GUI_VERSION}")