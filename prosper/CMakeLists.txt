cmake_minimum_required(VERSION 3.20)
project(prosper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Agentic-first: binaries must run with no PATH/DLL setup so CI (ctest) is hermetic.
# On mingw, statically link the GCC/C++ runtimes.
if(MINGW OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# --- tools ---------------------------------------------------------------
# self_dump: SELF/ELF inspector and NID import extractor (M0).
add_executable(self_dump tools/self_dump/self_dump.cpp)

# --- core library (M1+) --------------------------------------------------
# Loader / HLE / kernel / gpu sources accrete here per the roadmap.
file(GLOB_RECURSE PROSPER_SRC CONFIGURE_DEPENDS src/*.cpp)
if(PROSPER_SRC)
  add_library(prosper_core STATIC ${PROSPER_SRC})
  target_include_directories(prosper_core PUBLIC src)
endif()

# --- tests (agentic-first: self-checking, exit-code = truth) -------------
enable_testing()
# Path to the (gitignored) local game dump; override with -DGAME_DUMP=...
set(GAME_DUMP "${CMAKE_SOURCE_DIR}/../PPSA24651-app0" CACHE PATH "Local PS5 game dump root")

if(TARGET prosper_core)
  add_executable(test_module tests/test_module.cpp)
  target_link_libraries(test_module PRIVATE prosper_core)
  add_test(NAME module_loads_eboot
           COMMAND test_module "${GAME_DUMP}/eboot.bin")

  add_executable(test_nid tests/test_nid.cpp)
  target_link_libraries(test_nid PRIVATE prosper_core)
  add_test(NAME nid_hash_matches_imports
           COMMAND test_nid "${GAME_DUMP}/eboot.bin")

  # M2: real memory mapping + import trap (Linux only).
  if(UNIX AND NOT APPLE)
    add_executable(test_trap_linux tests/test_trap_linux.cpp)
    target_link_libraries(test_trap_linux PRIVATE prosper_core)
    add_test(NAME trap_identifies_imports
             COMMAND test_trap_linux "${GAME_DUMP}/eboot.bin")

    add_executable(test_boot_linux tests/test_boot_linux.cpp)
    target_link_libraries(test_boot_linux PRIVATE prosper_core)
    add_test(NAME boot_reaches_first_syscall
             COMMAND test_boot_linux "${GAME_DUMP}/eboot.bin")
  endif()
endif()
