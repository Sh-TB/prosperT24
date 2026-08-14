# Debug Intelligence Layer CMake Configuration
# Observer-only evidence management system for AI-assisted debugging

cmake_minimum_required(VERSION 3.14)
project(DebugIntelligence VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Find dependencies
find_package(Threads REQUIRED)

# Main library
add_library(debug_intelligence_lib
    # Headers only library - implementation in headers
)

target_include_directories(debug_intelligence_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# CLI executable
add_executable(debug-intel
    main.cpp
)

target_link_libraries(debug-intel
    PRIVATE
        debug_intelligence_lib
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

# Set output name
set_target_properties(debug-intel PROPERTIES
    OUTPUT_NAME debug-intel
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Test executable (requires Google Test)
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # Try to find GTest
    find_package(GTest QUIET)
    
    if(GTest_FOUND OR TARGET GTest::GTest)
        add_executable(test_debug_intelligence
            ../../tests/test_debug_intelligence.cpp
        )
        
        target_link_libraries(test_debug_intelligence
            PRIVATE
                debug_intelligence_lib
                GTest::gtest_main
                Threads::Threads
        )
        
        include(GoogleTest)
        gtest_discover_tests(test_debug_intelligence)
        
        message(STATUS "Debug Intelligence tests enabled")
    else()
        message(STATUS "GTest not found - tests disabled")
        message(STATUS "Install GTest or set BUILD_TESTS=OFF to suppress this message")
    endif()
endif()

# Installation rules
install(TARGETS debug-intel
    RUNTIME DESTINATION bin
)

install(DIRECTORY .
    DESTINATION include/debug_intelligence
    FILES_MATCHING PATTERN "*.hpp"
)

# Print configuration summary
message(STATUS "")
message(STATUS "Debug Intelligence Layer ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:        ${BUILD_TESTS}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
