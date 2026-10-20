# ── libAntiREAPass (LLVM out-of-tree pass plugin) ─────────────────────────
# Builds as a shared library (.so / .dll) that clang loads via
#   -fpass-plugin=libAntiREAPass.so
# or that antirea-opt loads directly.

add_llvm_pass_plugin(AntiREAPass
  AntiREAPass.cpp
  MirageProfile.cpp
  DEPENDS
    intrinsics_gen
)

target_include_directories(AntiREAPass
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/runtime/include
)

target_link_libraries(AntiREAPass
  PRIVATE
    nlohmann_json
)

# Pass plugin must NOT pull in LLVM as a static dep — it is loaded into
# the clang/opt process which already has LLVM symbols.
# LLVM_LINK_COMPONENTS stays empty; symbols resolved at plugin load time.

set_target_properties(AntiREAPass PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
