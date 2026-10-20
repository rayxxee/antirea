cmake_minimum_required(VERSION 3.20)
project(AntiREA VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── LLVM discovery ────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "AntiREA: found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")
message(STATUS "AntiREA: LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

if(LLVM_PACKAGE_VERSION VERSION_LESS "19.0")
  message(FATAL_ERROR "AntiREA requires LLVM >= 19. Found: ${LLVM_PACKAGE_VERSION}")
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# ── Platform detection ────────────────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(MIRAGE_PLATFORM "linux")
  add_compile_definitions(MIRAGE_PLATFORM_LINUX=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(MIRAGE_PLATFORM "windows")
  add_compile_definitions(MIRAGE_PLATFORM_WINDOWS=1)
else()
  message(FATAL_ERROR "AntiREA: unsupported platform '${CMAKE_SYSTEM_NAME}'. Only Linux and Windows x86_64 are in scope.")
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
  message(WARNING "AntiREA: detected processor '${CMAKE_SYSTEM_PROCESSOR}'. Only x86_64 is in scope; proceed at your own risk.")
endif()

message(STATUS "AntiREA: platform=${MIRAGE_PLATFORM} arch=${CMAKE_SYSTEM_PROCESSOR}")

# ── Third-party: nlohmann/json (header-only, vendored) ────────────────────────
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
  ${CMAKE_SOURCE_DIR}/third_party/nlohmann
)

# ── Subprojects ────────────────────────────────────────────────────────────────
add_subdirectory(lib/runtime)         # libmirage_runtime
add_subdirectory(lib/pass)            # libAntiREAPass
add_subdirectory(tools/antirea-opt)   # antirea-opt CLI

# ── Tests ──────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

# ── Install rules ──────────────────────────────────────────────────────────────
install(TARGETS antirea-opt DESTINATION bin)
install(TARGETS AntiREAPass DESTINATION lib)
install(TARGETS mirage_runtime DESTINATION lib)
