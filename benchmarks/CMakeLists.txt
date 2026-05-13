include(FetchContent)

message(STATUS "Fetching nanobench v4.3.11 ...")
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG        v4.3.11
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nanobench)

add_executable(bench_ezregex bench_ezregex.cpp)
target_link_libraries(bench_ezregex PRIVATE ezregex nanobench)
