add_executable(fuzz_ezregex fuzz_ezregex.cpp)
target_link_libraries(fuzz_ezregex PRIVATE ezregex)

# -fsanitize=fuzzer adds the fuzzer driver (replaces main) and coverage
# instrumentation.  ASan + UBSan are already applied to the whole build by
# the root CMakeLists when EZREGEX_BUILD_FUZZ=ON.
target_compile_options(fuzz_ezregex PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_ezregex    PRIVATE -fsanitize=fuzzer)
