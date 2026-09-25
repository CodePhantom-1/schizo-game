#!/bin/bash
# build_kernel_for_ue.sh — configures and builds kernel/build-ue with the SAME
# clang + sysroot UBT itself uses (Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/
# Linux_x64/v26_clang-20.1.8-rockylinux8), instead of the host's system g++.
#
# WHY: the plain `cmake -S kernel -B kernel/build-ue` (host g++, host glibc)
# produces a libsim_core.a that fails to link into SimRuntime's .so:
#   - host libstdc++ vtables (basic_ifstream/ostringstream) undefined against
#     UE's bundled toolchain sysroot;
#   - host glibc versioned symbols (__isoc23_strtol, etc., glibc 2.38+) don't
#     exist in the toolchain's bundled Rocky Linux 8 sysroot.
# Building with UE's own clang+sysroot (and -stdlib=libc++, matching what UBT
# links UE modules with) avoids both: same compiler, same libc, same libc++.
#
# Usage: tools/build_kernel_for_ue.sh   (run from the repo root)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TC="$HOME/UnrealEngine/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v26_clang-20.1.8-rockylinux8/x86_64-unknown-linux-gnu"

if [ ! -x "$TC/bin/clang++" ]; then
  echo "UE toolchain clang++ not found at $TC/bin/clang++ — falling back to host cmake config (may fail to link into UBT)." >&2
  cmake -S "$ROOT/kernel" -B "$ROOT/kernel/build-ue" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/kernel/build-ue" -j4
  exit 0
fi

cmake -S "$ROOT/kernel" -B "$ROOT/kernel/build-ue" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_CXX_COMPILER="$TC/bin/clang++" \
  -DCMAKE_CXX_FLAGS="--sysroot=$TC --target=x86_64-unknown-linux-gnu -stdlib=libc++" \
  -DCMAKE_EXE_LINKER_FLAGS="--sysroot=$TC --target=x86_64-unknown-linux-gnu -stdlib=libc++ -lc++abi -fuse-ld=lld"
cmake --build "$ROOT/kernel/build-ue" -j4
