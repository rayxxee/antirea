add_executable(test_profile_schema test_profile_schema.cpp)

target_link_libraries(test_profile_schema
  PRIVATE
    AntiREAPass   # contains MirageProfile
    nlohmann_json
)

target_include_directories(test_profile_schema PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/pass
)

add_test(NAME phase0_profile_schema COMMAND test_profile_schema)
set_tests_properties(phase0_profile_schema PROPERTIES
  PASS_REGULAR_EXPRESSION "0 failed"
)
