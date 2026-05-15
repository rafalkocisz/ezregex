add_executable(fuzz_ez_regex fuzz_ez_regex.cpp)
target_link_libraries(fuzz_ez_regex PRIVATE ezregex)

# -fsanitize=fuzzer adds the fuzzer driver (replaces main) and coverage
# instrumentation.  ASan + UBSan are already applied to the whole build by
# the root CMakeLists when EZREGEX_BUILD_FUZZ=ON.
target_compile_options(fuzz_ez_regex PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_ez_regex    PRIVATE -fsanitize=fuzzer)
