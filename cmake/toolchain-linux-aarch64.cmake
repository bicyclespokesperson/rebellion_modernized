# Cross toolchain for the native build on aarch64 Linux (clang-19 with the Ubuntu cross packages (gcc-aarch64-linux-gnu libc/libstdc++), no sysroot; docker/Dockerfile.native).
# Used by tools/build_native.sh --arch arm64. Run the result with qemu-aarch64 -L /usr/aarch64-linux-gnu.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER clang-19)
set(CMAKE_CXX_COMPILER clang++-19)
set(triple aarch64-linux-gnu)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(gccdir /usr/lib/gcc-cross/aarch64-linux-gnu/13)
set(CMAKE_C_FLAGS_INIT "--gcc-install-dir=${gccdir}")
set(CMAKE_CXX_FLAGS_INIT "--gcc-install-dir=${gccdir}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
