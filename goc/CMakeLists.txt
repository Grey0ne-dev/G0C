cmake_minimum_required(VERSION 3.10)
project(goc)

set(CMAKE_CXX_STANDARD 17)

# Compiler executable
add_executable(goc 
    main.cpp
    lexer.cpp
    parser.cpp
    codegen.cpp
)

# Virtual Machine executable
add_executable(vm 
    vm_main.cpp
    vm.cpp
)

