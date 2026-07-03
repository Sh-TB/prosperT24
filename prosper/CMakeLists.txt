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
  target_compile_definitions(prosper_core PRIVATE PROSPER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  find_package(Threads REQUIRED)
  target_link_libraries(prosper_core PUBLIC Threads::Threads)
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

  add_executable(test_hle_registered tests/test_hle_registered.cpp)
  target_link_libraries(test_hle_registered PRIVATE prosper_core)
  add_test(NAME hle_functions_registered COMMAND test_hle_registered)

  # M2+: real memory mapping, trap, boot, and the boot-trace debug tool (Linux only).
  if(UNIX AND NOT APPLE)
    add_executable(boot_trace tools/boot_trace/boot_trace.cpp)
    target_link_libraries(boot_trace PRIVATE prosper_core)

    add_executable(test_trap_linux tests/test_trap_linux.cpp)
    target_link_libraries(test_trap_linux PRIVATE prosper_core)
    add_test(NAME trap_identifies_imports
             COMMAND test_trap_linux "${GAME_DUMP}")

    add_executable(test_boot_linux tests/test_boot_linux.cpp)
    target_link_libraries(test_boot_linux PRIVATE prosper_core)
    add_test(NAME boot_reaches_first_syscall
             COMMAND test_boot_linux "${GAME_DUMP}")

    # Regression test for the real setjmp/longjmp impl (x86-64 asm; used by the GC's root scan).
    add_executable(test_setjmp tests/test_setjmp.cpp)
    target_link_libraries(test_setjmp PRIVATE prosper_core)
    add_test(NAME setjmp_saves_and_restores COMMAND test_setjmp)
  endif()
endif()
