cmake_minimum_required(VERSION 3.10)
project(goc)

include(CTest)

set(CMAKE_CXX_STANDARD 17)

# Compiler executable
add_executable(goc 
    main.cpp
    logger.cpp
    lexer.cpp
    parser.cpp
    codegen.cpp
)

# Virtual Machine executable
add_executable(vm 
    vm_main.cpp
    logger.cpp
    vm.cpp
)

if(BUILD_TESTING)
    add_test(
        NAME goc_feature_tests
        COMMAND ${CMAKE_COMMAND} -E env
            GOC_COMPILER=$<TARGET_FILE:goc>
            GOC_VM=$<TARGET_FILE:vm>
            bash ${CMAKE_CURRENT_SOURCE_DIR}/../tests/run_feature_tests.sh
    )
endif()
