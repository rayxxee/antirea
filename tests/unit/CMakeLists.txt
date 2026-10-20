add_executable(test_runtime_phase0 test_runtime_phase0.cpp)

target_link_libraries(test_runtime_phase0 PRIVATE mirage_runtime)

target_include_directories(test_runtime_phase0 PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/runtime/include
)

add_test(NAME phase0_runtime_unit COMMAND test_runtime_phase0)
set_tests_properties(phase0_runtime_unit PROPERTIES
  PASS_REGULAR_EXPRESSION "0 failed"
)
