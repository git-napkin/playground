cmake_minimum_required(VERSION 3.16)
project(pluginplayground C CXX OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_ARCHITECTURES "arm64e")

set(CODESIGN_IDENTITY "-" CACHE STRING "Codesign identity (default: ad-hoc)")
set(INSTALL_PREFIX "/opt/pluginplayground" CACHE PATH "Installation prefix")

function(syphon_codesign target)
  add_custom_command(TARGET ${target} POST_BUILD
        COMMAND codesign -f -s "${CODESIGN_IDENTITY}"
                --entitlements "${CMAKE_SOURCE_DIR}/Master.entitlements"
                $<TARGET_FILE:${target}>
        COMMENT "codesigning ${target} (identity: ${CODESIGN_IDENTITY})"
    )
endfunction()

set(SYPHON_SOURCES
    syphon/envbuf.c
    syphon/tweak_utils.c
)

set(SYPHON_INCLUDE_PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/syphon>
    $<INSTALL_INTERFACE:include/syphon>
)

# ── libfangs_hook: Frida-Gum posix_spawn hook (injected into launchd) ────
add_library(fangs_hook SHARED
    syphon/fangs_hook.c
    ${SYPHON_SOURCES}
)
target_include_directories(fangs_hook PRIVATE ${SYPHON_INCLUDE_PRIVATE})
target_link_libraries(fangs_hook PRIVATE
    "-framework CoreFoundation"
)

# ── libplayground_opener: per-process tweak loader ────────────────────────
add_library(playground_opener SHARED
    syphon/playground_opener.c
    ${SYPHON_SOURCES}
)
target_include_directories(playground_opener PRIVATE ${SYPHON_INCLUDE_PRIVATE})
target_link_libraries(playground_opener PRIVATE
    "-framework Foundation"
    "-framework CoreFoundation"
)

# ── Privilege grant tool (launchd shellcode injector) ─────────────────────

add_executable(grant grant/main.m)
target_include_directories(grant PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/grant")
target_link_libraries(grant PRIVATE
    "-framework Foundation"
)
target_compile_options(grant PRIVATE -fobjc-arc)
syphon_codesign(grant)

# ── Configurator ──────────────────────────────────────────────────────────

option(BUILD_CONFIGURATOR "Build the Configurator App" ON)

if(BUILD_CONFIGURATOR)
    include(FetchContent)
    FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG        v1.16.1
    )
    set(SLINT_STYLE "native" CACHE STRING "Slint widget style")
    set(SLINT_FEATURE_STYLE_NATIVE ON CACHE BOOL "Enable native widget style")
    set(SLINT_FETCH_CONTENT ON CACHE BOOL "" FORCE)

    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.build-cargo")
    if(EXISTS "${CMAKE_BINARY_DIR}/cargo")
        if(NOT IS_SYMLINK "${CMAKE_BINARY_DIR}/cargo")
            file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/cargo")
            file(CREATE_LINK "${CMAKE_SOURCE_DIR}/.build-cargo" "${CMAKE_BINARY_DIR}/cargo" SYMBOLIC)
        endif()
    else()
        file(CREATE_LINK "${CMAKE_SOURCE_DIR}/.build-cargo" "${CMAKE_BINARY_DIR}/cargo" SYMBOLIC)
    endif()
    FetchContent_MakeAvailable(Slint)

    add_executable(configurator
        configurator/main.cpp
        configurator/controller.cpp
        configurator/dmanager.cpp
        configurator/options.cpp
        configurator/tweaks.cpp
        configurator/configurator.slint
    )
    slint_target_sources(configurator configurator/configurator.slint)
    set_property(TARGET configurator PROPERTY SLINT_EMBED_RESOURCES embed-files)
    set_target_properties(configurator PROPERTIES
        CXX_STANDARD 17
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pluginplayground.configurator"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_BUNDLE_NAME "Plugin Playground Configurator"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/configurator/Info.plist.in"
        BUILD_RPATH "@executable_path/."
        INSTALL_RPATH "@executable_path/."
    )
    target_link_libraries(configurator PRIVATE Slint::Slint "-framework CoreFoundation")

    add_custom_command(TARGET configurator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_BINARY_DIR}/_deps/slint-build/api/cpp/libslint_cpp.dylib"
            "$<TARGET_FILE_DIR:configurator>"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/configurator/gearbox.icns"
            "$<TARGET_BUNDLE_CONTENT_DIR:configurator>/Resources/gearbox.icns"
        COMMENT "Copying libslint_cpp.dylib + gearbox.icns into configurator.app bundle"
    )

    install(TARGETS configurator
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION Applications
    )
endif()

# ── Install ──────────────────────────────────────────────────────────────
install(TARGETS grant RUNTIME DESTINATION bin)
install(TARGETS fangs_hook playground_opener LIBRARY DESTINATION lib)
