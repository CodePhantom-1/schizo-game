cmake_minimum_required(VERSION 3.16)

# When configured into kernel/build-ue (the UE bring-up build directory named
# by every UE track's build recipe), switch to UE5's own bundled clang+libc++
# toolchain *before* project(). UE links this static lib into its own shared
# modules (UnrealEditor-SimRuntime.so): mixing libstdc++ object code (this
# repo's default toolchain) into UE's libc++ link produces undefined-symbol
# and "recompile with -fPIC" errors (ABI + relocation mismatch) — proven by
# hand when Wave-3 (UE-2, player body) first linked. No effect on the plain
# gate build (any other build dir keeps the system default compiler).
set(_using_ue_clang OFF)
if(CMAKE_BINARY_DIR MATCHES "/kernel/build-ue$" AND NOT DEFINED CMAKE_CXX_COMPILER)
    file(GLOB _ue_clang_candidates
        "$ENV{HOME}/UnrealEngine/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/*/x86_64-unknown-linux-gnu")
    if(_ue_clang_candidates)
        list(GET _ue_clang_candidates 0 _ue_toolchain)
        if(EXISTS "${_ue_toolchain}/bin/clang++")
            set(CMAKE_C_COMPILER "${_ue_toolchain}/bin/clang" CACHE STRING "" FORCE)
            set(CMAKE_CXX_COMPILER "${_ue_toolchain}/bin/clang++" CACHE STRING "" FORCE)
            set(_ue_sysroot_flags "--target=x86_64-unknown-linux-gnu --sysroot=${_ue_toolchain} -stdlib=libc++")
            set(CMAKE_CXX_FLAGS_INIT "${_ue_sysroot_flags}")
            set(CMAKE_EXE_LINKER_FLAGS_INIT "${_ue_sysroot_flags}")
            set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_ue_sysroot_flags}")
            set(_using_ue_clang ON)
        endif()
    endif()
endif()

project(sim_kernel CXX)

if(_using_ue_clang)
    # -lc++abi must land at the END of the link line (after the objects/libs
    # that need it) — link_libraries() appends there for every target defined
    # below, unlike CMAKE_EXE_LINKER_FLAGS which lands before the objects.
    link_libraries(c++abi)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# UE5's editor links this static lib into a shared module (UnrealEditor-SchizoGame.so);
# ld.lld refuses non-PIC relocations there. Harmless for the plain ctest gate build.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(sim_core STATIC ${SIM_SOURCES})
target_include_directories(sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()

file(GLOB SIM_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp)
foreach(test_file ${SIM_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE sim_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
