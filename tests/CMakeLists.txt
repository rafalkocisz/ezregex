add_executable(test_ezregex test_ezregex.cpp)
target_link_libraries(test_ezregex PRIVATE ezregex)
target_include_directories(test_ezregex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME ezregex_tests COMMAND test_ezregex)
