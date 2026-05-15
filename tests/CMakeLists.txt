add_executable(test_ez_regex test_ez_regex.cpp)
target_link_libraries(test_ez_regex PRIVATE ezregex)
target_include_directories(test_ez_regex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME ezregex_tests COMMAND test_ez_regex)
