cmake_minimum_required(VERSION 3.20)
project(ezregex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EZREGEX_BUILD_TESTS      "Build unit tests"                      ON)
option(EZREGEX_BUILD_BENCHMARKS "Build benchmarks"                      OFF)
option(EZREGEX_SANITIZE         "Build with ASan (+ UBSan on GCC/Clang)" OFF)
option(EZREGEX_BUILD_FUZZ       "Build libFuzzer target (Clang only)"   OFF)

if(EZREGEX_BUILD_FUZZ AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "EZREGEX_BUILD_FUZZ requires Clang. "
        "Re-run cmake with -DCMAKE_CXX_COMPILER=clang++")
endif()

# Sanitizer / fuzzer flags must be set before add_library / add_executable so
# that every target in this directory tree picks them up.
if(EZREGEX_SANITIZE)
    if(MSVC)
        # MSVC supports ASan only (no UBSan).
        # /RTC1 (default in Debug) and /INCREMENTAL (default linker) are both
        # incompatible with ASan — remove them.
        add_compile_options(/fsanitize=address /Zi)
        foreach(flag CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG)
            string(REPLACE "/RTC1" "" ${flag} "${${flag}}")
        endforeach()
        add_link_options(/INCREMENTAL:NO)
    else()
        # GCC / Clang: full ASan + UBSan.
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    endif()
elseif(EZREGEX_BUILD_FUZZ)
    # Fuzz build implies ASan + UBSan; handled separately so EZREGEX_SANITIZE
    # is not required as a prerequisite.
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(ezregex ez_regex.cpp)
target_include_directories(ezregex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(EZREGEX_BUILD_TESTS)
    target_compile_definitions(ezregex PRIVATE EZ_REGEX_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(EZREGEX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(EZREGEX_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif()
