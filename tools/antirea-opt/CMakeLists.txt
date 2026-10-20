# ── antirea-opt CLI ──────────────────────────────────────────────────────────

set(LLVM_LINK_COMPONENTS
  Core
  BitReader
  BitWriter
  Passes
  Support
  Analysis
  TransformUtils
  IRReader
)

add_llvm_tool(antirea-opt
  main.cpp
  AntiREAOptCLI.cpp
)

target_include_directories(antirea-opt
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/pass
    ${CMAKE_SOURCE_DIR}/lib/runtime/include
)

target_link_libraries(antirea-opt
  PRIVATE
    AntiREAPass
    mirage_runtime
    nlohmann_json
)

set_target_properties(antirea-opt PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
