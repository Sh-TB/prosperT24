# CMake generated Testfile for 
# Source directory: /mnt/c/Users/matti/repos/ps5ys/prosper
# Build directory: /mnt/c/Users/matti/repos/ps5ys/prosper/build-linux
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[module_loads_eboot]=] "/mnt/c/Users/matti/repos/ps5ys/prosper/build-linux/test_module" "/mnt/c/Users/matti/repos/ps5ys/prosper/../PPSA24651-app0/eboot.bin")
set_tests_properties([=[module_loads_eboot]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/matti/repos/ps5ys/prosper/CMakeLists.txt;38;add_test;/mnt/c/Users/matti/repos/ps5ys/prosper/CMakeLists.txt;0;")
add_test([=[trap_identifies_imports]=] "/mnt/c/Users/matti/repos/ps5ys/prosper/build-linux/test_trap_linux" "/mnt/c/Users/matti/repos/ps5ys/prosper/../PPSA24651-app0/eboot.bin")
set_tests_properties([=[trap_identifies_imports]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/matti/repos/ps5ys/prosper/CMakeLists.txt;45;add_test;/mnt/c/Users/matti/repos/ps5ys/prosper/CMakeLists.txt;0;")
add_test([=[boot_reaches_first_syscall]=] "/mnt/c/Users/matti/repos/ps5ys/prosper/build-linux/test_boot_linux" "/mnt/c/Users/matti/repos/ps5ys/prosper/../PPSA24651-app0/eboot.bin")
set_tests_properties([=[boot_reaches_first_syscall]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/Users/matti/repos/ps5ys/prosper/CMakeLists.txt;50;add_test;/mnt/c/Users/matti/repos/ps5ys/prosper/CMakeLists.txt;0;")
