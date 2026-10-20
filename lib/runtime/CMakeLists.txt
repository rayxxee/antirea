# ── libmirage_runtime ──────────────────────────────────────────────────────────
# Static runtime library: inline-syscall sensors, ChaCha20/XOR, FNV-1a/CRC32.
# Dual platform: linux/ and windows/ directories provide the platform-specific
# implementations behind the common include/mirage/runtime.h interface.

set(RUNTIME_COMMON_SOURCES
  env_poll.c   # combined poll + Sensor D stub + ChaCha20/XOR stubs
)

set(RUNTIME_LINUX_SOURCES
  linux/sensor_a_tracer.c
  linux/sensor_b_hook_scan.c
  linux/sensor_c_self_checksum.c
)

set(RUNTIME_WINDOWS_SOURCES
  windows/sensor_a_tracer.c
  windows/sensor_b_hook_scan.c
  windows/sensor_c_self_checksum.c
)

if(MIRAGE_PLATFORM STREQUAL "linux")
  set(RUNTIME_PLATFORM_SOURCES ${RUNTIME_LINUX_SOURCES})
else()
  set(RUNTIME_PLATFORM_SOURCES ${RUNTIME_WINDOWS_SOURCES})
endif()

add_library(mirage_runtime STATIC
  ${RUNTIME_COMMON_SOURCES}
  ${RUNTIME_PLATFORM_SOURCES}
)

target_include_directories(mirage_runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/${MIRAGE_PLATFORM}
)

# Runtime must NOT pull in libc for the sensor implementations —
# that's the whole point of inline syscalls / PEB walks.
# Use compile options to catch accidental libc linkage in CI.
if(MIRAGE_PLATFORM STREQUAL "linux")
  target_compile_options(mirage_runtime PRIVATE
    -ffreestanding
    -fno-builtin
    -Wall -Wextra
  )
endif()

set_target_properties(mirage_runtime PROPERTIES
  OUTPUT_NAME "mirage_runtime"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
