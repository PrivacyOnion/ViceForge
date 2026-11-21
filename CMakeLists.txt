cmake_minimum_required(VERSION 3.24)
project(ViceForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_library(viceforge_core SHARED
    src/manifest.cpp
    src/vfs.cpp
    src/rage9.cpp
    src/loader.cpp
)
target_include_directories(viceforge_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(viceforge_core
    PRIVATE
        VICEFORGE_CORE_EXPORTS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)
if(MSVC)
    target_compile_options(viceforge_core PRIVATE /W4 /EHsc /utf-8)
endif()

add_library(viceforge_proxy SHARED
    src/proxy_bootstrap.cpp
)
target_link_libraries(viceforge_proxy PRIVATE viceforge_core)
target_compile_definitions(viceforge_proxy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(viceforge_proxy PRIVATE /W4 /EHsc /utf-8)
endif()

add_executable(vfcli
    cli/main.cpp
)
target_link_libraries(vfcli PRIVATE viceforge_core)
target_compile_definitions(vfcli PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(vfcli PRIVATE /W4 /EHsc /utf-8)
endif()

add_library(sample_mod SHARED
    examples/sample_mod/mod_main.cpp
)
target_include_directories(sample_mod PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(sample_mod PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(sample_mod PRIVATE /W4 /EHsc /utf-8)
endif()

add_library(sample_addon SHARED
    examples/sample_addon/mod_main.cpp
)
target_include_directories(sample_addon PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(sample_addon PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(sample_addon PRIVATE /W4 /EHsc /utf-8)
endif()

enable_testing()

function(viceforge_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE viceforge_core)
    target_compile_definitions(${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /EHsc /utf-8)
    endif()
    add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}> ${ARGN})
endfunction()

viceforge_add_test(test_manifest_resolver tests/test_manifest_resolver.cpp)
viceforge_add_test(test_vfs_registry tests/test_vfs_registry.cpp)
viceforge_add_test(test_rage9_containers tests/test_rage9_containers.cpp)
viceforge_add_test(test_mod_loader_e2e tests/test_mod_loader_e2e.cpp
    "$<TARGET_FILE:sample_mod>"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_mod/viceforge.mod.json"
    "$<TARGET_FILE:sample_addon>"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_addon/viceforge.mod.json"
)
viceforge_add_test(test_cli_e2e tests/test_cli_e2e.cpp
    "$<TARGET_FILE:vfcli>"
    "$<TARGET_FILE:sample_mod>"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_mod/viceforge.mod.json"
    "$<TARGET_FILE:sample_addon>"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_addon/viceforge.mod.json"
)
set_tests_properties(test_mod_loader_e2e PROPERTIES DEPENDS "sample_mod;sample_addon;viceforge_core")
set_tests_properties(test_cli_e2e PROPERTIES DEPENDS "vfcli;sample_mod;sample_addon;viceforge_core")
