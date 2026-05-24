cmake_minimum_required(VERSION 3.15)
project(inventoryui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(INVENTORYUI_BUILD_AS_STATIC
        "Build as static library for embedding in another plugin" OFF
)

# ---------------------------------------------------------------------------
# fmt — try system package first, fall back to header-only FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)

find_package(fmt QUIET)
if (NOT fmt_FOUND)
    set(FMT_HEADER_ONLY ON CACHE BOOL "fmt header-only" FORCE)
    FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 11.2.0
    )
    FetchContent_MakeAvailable(fmt)
endif()

# ---------------------------------------------------------------------------
# Endstone SDK
# ---------------------------------------------------------------------------
FetchContent_Declare(
        endstone
        GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
        GIT_TAG v0.11.0
)
FetchContent_MakeAvailable(endstone)

# When included as a subdirectory for header-only use (e.g. via FetchContent)
# and NOT building as static, only expose the header interface.
if (NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT INVENTORYUI_BUILD_AS_STATIC)
    add_library(endstone_inventoryui INTERFACE)
    target_include_directories(endstone_inventoryui INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(endstone_inventoryui INTERFACE
            endstone::endstone
            fmt::fmt-header-only
    )
    return()
endif ()

# ---------------------------------------------------------------------------
# Rust bedrock-ffi — fetch from GitHub and build via Cargo
# ---------------------------------------------------------------------------
include(ExternalProject)

# 1. 动态适配跨平台 Rust 静态库命名规则
if (WIN32)
    set(RUST_LIB_NAME "bedrock_ffi.lib")
else()
    set(RUST_LIB_NAME "libbedrock_ffi.a")
endif()

# 2. 动态适配跨平台系统级链接库
if (NOT WIN32)
    # Linux 平台需要的底层库
    set(SYSTEM_LIBS dl pthread rt m)
else()
    # Windows 平台下 Rust 静态库(std)必须依赖的底层系统库
    set(SYSTEM_LIBS ws2_32 ntdll userenv)
endif()


ExternalProject_Add(bedrock_ffi
        GIT_REPOSITORY https://github.com/yuhangle/bedrock-protocol-rs.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
        UPDATE_DISCONNECTED TRUE
        BUILD_IN_SOURCE      TRUE
        CONFIGURE_COMMAND    ""
        BUILD_COMMAND        cargo build --release -p bedrock-ffi
        INSTALL_COMMAND      ""
        BUILD_BYPRODUCTS     <SOURCE_DIR>/target/release/${RUST_LIB_NAME}
)

ExternalProject_Get_property(bedrock_ffi SOURCE_DIR)
set(RUST_FFI_LIB "${SOURCE_DIR}/target/release/${RUST_LIB_NAME}")

# ---------------------------------------------------------------------------
# Shared sources for both static and shared builds
# ---------------------------------------------------------------------------

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
configure_file(include/version.h.in include/version.h)

set(PLUGIN_SOURCES
        src/plugin.cpp
        src/inventoryui_init.cpp
        src/packets.cpp
        src/menu.cpp
        src/inventory.cpp
        src/listener.cpp
        src/item_registry.cpp
)

# ---------------------------------------------------------------------------
# Static library mode — for embedding in another plugin
# ---------------------------------------------------------------------------

if (INVENTORYUI_BUILD_AS_STATIC)

    add_library(endstone_inventoryui STATIC ${PLUGIN_SOURCES})
    target_include_directories(endstone_inventoryui
            PUBLIC  include
            PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include"
    )
    target_link_libraries(endstone_inventoryui
            PUBLIC  endstone::endstone fmt::fmt-header-only
            PRIVATE "${RUST_FFI_LIB}" ${SYSTEM_LIBS}
    )
    target_compile_definitions(endstone_inventoryui PRIVATE INVENTORYUI_AS_STATIC)
    add_dependencies(endstone_inventoryui bedrock_ffi)

    # ---------------------------------------------------------------------------
    # Shared library mode — standalone Endstone plugin
    # ---------------------------------------------------------------------------

else()

    endstone_add_plugin(${PROJECT_NAME} ${PLUGIN_SOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE
            include
            "${CMAKE_CURRENT_BINARY_DIR}/include"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
            fmt::fmt-header-only
            "${RUST_FFI_LIB}" ${SYSTEM_LIBS}
    )
    add_dependencies(${PROJECT_NAME} bedrock_ffi)

endif ()
