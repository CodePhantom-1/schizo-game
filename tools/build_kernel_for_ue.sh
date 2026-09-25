#!/usr/bin/env bash
# build_kernel_for_ue.sh — builds kernel/build-ue/libsim_core.a for the Linux
# editor/game build (SimRuntime.Build.cs links it). See docs/kernel-build-for-ue.md.
#
# Why not a plain `cmake -B kernel/build-ue`: Unreal on Linux compiles with its
# OWN bundled clang against its OWN libc++, and links modules as shared
# objects. A kernel built with the system gcc/libstdc++ fails to link
# (undefined std::__cxx11 symbols); one built without -fPIC cannot go into a
# shared object. So this script uses UE's clang, UE's libc++ headers, and -fPIC.
#
# Usage:
#   UE_ROOT=/path/to/UnrealEngine tools/build_kernel_for_ue.sh [--clean]
# UE_ROOT is the folder that contains Engine/. Optional overrides:
#   UE_TOOLCHAIN  the toolchain dir (…/Linux_x64/<version>/x86_64-unknown-linux-gnu);
#                 defaults to $LINUX_MULTIARCH_ROOT/x86_64-unknown-linux-gnu, else the
#                 newest one under $UE_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64.
#   UE_LIBCXX     UE's libc++ root; defaults to $UE_ROOT/Engine/Source/ThirdParty/Unix/LibCxx,
#                 or the toolchain itself (UE 5.8 ships libc++ inside the clang SDK).
#   KERNEL_UE_BUILD_DIR  output dir (default kernel/build-ue, where SimRuntime.Build.cs looks).
# --clean deletes kernel/build-ue first (needed when the compiler changes).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${KERNEL_UE_BUILD_DIR:-$REPO/kernel/build-ue}"

: "${UE_ROOT:?set UE_ROOT to the Unreal Engine root (the folder containing Engine/)}"

if [[ -n "${UE_TOOLCHAIN:-}" ]]; then
	TOOLCHAIN="$UE_TOOLCHAIN"
elif [[ -n "${LINUX_MULTIARCH_ROOT:-}" ]]; then
	TOOLCHAIN="${LINUX_MULTIARCH_ROOT%/}/x86_64-unknown-linux-gnu"
else
	SDK_BASE="$UE_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64"
	TOOLCHAIN="$(ls -d "$SDK_BASE"/*/x86_64-unknown-linux-gnu 2>/dev/null | sort | tail -n 1 || true)"
fi
LIBCXX="${UE_LIBCXX:-$UE_ROOT/Engine/Source/ThirdParty/Unix/LibCxx}"
# UE 5.8 moved libc++ into the toolchain SDK (…/x86_64-unknown-linux-gnu/include/c++/v1).
if [[ -z "${UE_LIBCXX:-}" && ! -d "$LIBCXX/include/c++/v1" && -d "$TOOLCHAIN/include/c++/v1" ]]; then
	LIBCXX="$TOOLCHAIN"
fi

CXX="$TOOLCHAIN/bin/clang++"
if [[ -z "$TOOLCHAIN" || ! -x "$CXX" ]]; then
	echo "build_kernel_for_ue: UE's bundled clang not found (looked for '$CXX')." >&2
	echo "  Set UE_TOOLCHAIN to …/Linux_x64/<version>/x86_64-unknown-linux-gnu." >&2
	exit 1
fi
if [[ ! -d "$LIBCXX/include/c++/v1" ]]; then
	echo "build_kernel_for_ue: UE's libc++ headers not found at $LIBCXX/include/c++/v1." >&2
	echo "  Set UE_LIBCXX to UE's Engine/Source/ThirdParty/Unix/LibCxx." >&2
	exit 1
fi

if [[ "${1:-}" == "--clean" ]]; then
	rm -rf "$BUILD"
fi

# The same target, sysroot and C++ library the UE Linux toolchain uses.
CXXFLAGS="--target=x86_64-unknown-linux-gnu --sysroot=$TOOLCHAIN -fPIC -nostdinc++ -isystem $LIBCXX/include -isystem $LIBCXX/include/c++/v1"

echo "build_kernel_for_ue: clang   $CXX"
echo "build_kernel_for_ue: libc++  $LIBCXX"

# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY: CMake's compiler check must not
# try to LINK an executable (there is no libc++ on the link line — UE links
# its own libc++ into the editor). Only the library target is built; the
# kernel tests are for the host toolchain build (kernel/build).
cmake -S "$REPO/kernel" -B "$BUILD" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_CXX_COMPILER="$CXX" \
	-DCMAKE_CXX_FLAGS="$CXXFLAGS" \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
cmake --build "$BUILD" --target sim_core --parallel

echo "build_kernel_for_ue: built $BUILD/libsim_core.a"
