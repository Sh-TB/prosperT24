cmake_minimum_required(VERSION 3.14)
project(ProsperDiagnostics VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Sanitizers
if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Source files
set(DIAGNOSTICS_SOURCES
    core/diagnostic_interface.hpp
    core/event_bus.hpp
    core/plugin_registry.hpp
    plugins/boot_timeline_plugin.hpp
    plugins/module_load_plugin.hpp
    plugins/import_resolution_plugin.hpp
    plugins/memory_map_plugin.hpp
    plugins/crash_context_plugin.hpp
    plugins/thread_activity_plugin.hpp
    plugins/file_access_plugin.hpp
    plugins/hle_call_stats_plugin.hpp
    plugins/performance_marker_plugin.hpp
    plugins/ai_report_generator_plugin.hpp
    plugins/boot_state_machine_plugin.hpp
    plugins/relocation_diagnostics_plugin.hpp
    plugins/crash_replay_snapshot_plugin.hpp
    plugins/hle_evidence_plugin.hpp
    plugins/event_correlation_engine.hpp
    plugins/memory_mapping_validator_plugin.hpp
    plugins/deterministic_diagnostics_mode.hpp
    plugins/runtime_init_trace_plugin.hpp
)

# Library target (header-only)
add_library(diagnostics_framework INTERFACE)
target_include_directories(diagnostics_framework INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
)

# Test suite
if(BUILD_TESTS)
    enable_testing()
    
    # Find Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    
    # For Windows: prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
    
    # Test executable
    add_executable(diagnostics_tests
        tests/diagnostics_test_suite.cpp
        ${DIAGNOSTICS_SOURCES}
    )
    
    target_link_libraries(diagnostics_tests PRIVATE
        gtest
        gtest_main
        pthread  # For std::mutex, std::thread support in tests
    )
    
    include(GoogleTest)
    gtest_discover_tests(diagnostics_tests)
endif()

# Example programs
if(BUILD_EXAMPLES)
    add_executable(minimal_example examples/minimal_example.cpp)
    target_link_libraries(minimal_example PRIVATE diagnostics_framework)
endif()

# Installation rules
install(TARGETS diagnostics_framework EXPORT diagnostics-framework-targets)
install(DIRECTORY core/ plugins/
    DESTINATION include/diagnostics_framework
        FILES_MATCHING PATTERN "*.hpp"
)

# Export targets
install(EXPORT diagnostics-framework-targets
    NAMESPACE DiagnosticsFramework::
    DESTINATION lib/cmake/diagnostics-framework
)

# Package configuration
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/diagnostics-framework-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/diagnostics-framework-config.cmake
    INSTALL_DESTINATION lib/cmake/diagnostics-framework
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/diagnostics-framework-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/diagnostics-framework-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/diagnostics-framework-config-version.cmake
    DESTINATION lib/cmake/diagnostics-framework
)

# Print summary
message(STATUS "")
message(STATUS "Prosper Diagnostics Framework ${PROJECT_VERSION}")
message(STATUS "=========================================")
message(STATUS "  Tests:           ${BUILD_TESTS}")
message(STATUS "  Examples:        ${BUILD_EXAMPLES}")
message(STATUS "  Sanitizers:      ${ENABLE_SANITIZERS}")
message(STATUS "  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
