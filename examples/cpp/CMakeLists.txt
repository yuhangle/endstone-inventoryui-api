cmake_minimum_required(VERSION 3.15)
project(inventoryui_example CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch fmt (header-only, required on Arch Linux)
include(FetchContent)
find_package(fmt QUIET)
if (NOT fmt_FOUND)
    set(FMT_HEADER_ONLY ON CACHE BOOL "fmt header-only" FORCE)
    FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 11.2.0)
    FetchContent_MakeAvailable(fmt)
endif()

# Fetch Endstone SDK
include(FetchContent)
FetchContent_Declare(
    endstone
    GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
    GIT_TAG v0.11.0
)
FetchContent_MakeAvailable(endstone)

# Fetch the endstone-inventoryui plugin
include(FetchContent)
FetchContent_Declare(
    inventoryui
    GIT_REPOSITORY https://github.com/yuhangle/endstone-inventoryui.git
    GIT_TAG main
)
FetchContent_MakeAvailable(inventoryui)

# Build the example plugin
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
configure_file(include/version.h.in include/version.h)

endstone_add_plugin(${PROJECT_NAME} plugin.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    endstone_inventoryui
    fmt::fmt-header-only
)
