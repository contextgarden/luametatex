# cross compilation experiment

set(CMAKE_SYSTEM_NAME ARM64)
set(TOOLCHAIN_PREFIX  aarch64-linux-gnu)
set(CMAKE_C_COMPILER  ${TOOLCHAIN_PREFIX}-gcc)

add_definitions(-DLUA_USE_LINUX=1)
add_definitions(-DLUA_USE_MKSTEMP=1)

# set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc")
