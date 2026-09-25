cmake_minimum_required(VERSION 3.16)
project(sim_kernel CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(sim_core STATIC ${SIM_SOURCES})
target_include_directories(sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# UE links this static lib into UnrealEditor-SimRuntime.so (SimRuntime.Build.cs);
# a non-PIC archive fails at link with "recompile with -fPIC" under UE's clang/lld
# toolchain. Harmless for the plain CLI test binaries too.
set_target_properties(sim_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

enable_testing()

file(GLOB SIM_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp)
foreach(test_file ${SIM_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE sim_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
