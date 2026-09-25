# Building the kernel for Unreal (`kernel/build-ue`)

`SimRuntime.Build.cs` links the world kernel as a static library from `kernel/build-ue/`:

| Platform | File it links |
|---|---|
| Linux (editor and game) | `kernel/build-ue/libsim_core.a` |
| Win64 | `kernel/build-ue/sim_core.lib` |

If the file is missing, UBT prints `SimRuntime: warning: kernel library missing at …` and then the link fails on that path. `kernel/build-ue/` is git-ignored (`kernel/build*/`), so every machine builds its own. Rebuild it whenever `kernel/src` changes, before building the editor.

This is a separate build from `kernel/build/`, which is the host-toolchain build that runs the kernel tests (`ctest`). Don't point the plugin at `kernel/build/`.

## Linux: UE's clang, libc++ and -fPIC

Unreal on Linux compiles with **its own bundled clang** against **its own libc++**, and in editor (modular) builds it links every module as a shared object. The kernel library has to match all three:

- **UE's bundled clang.** It is at `Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/<version>/x86_64-unknown-linux-gnu/bin/clang++`. If you set up the engine with `LINUX_MULTIARCH_ROOT`, it is under that folder instead.
- **libc++, not libstdc++.** Use `-nostdinc++ -isystem Engine/Source/ThirdParty/Unix/LibCxx/include/c++/v1`. A kernel built with the system gcc and libstdc++ fails to link into the editor with undefined `std::__cxx11::…` symbols.
- **-fPIC.** Without it, the linker refuses to put the static library's objects into `libUnrealEditor-SimRuntime.so`.

`tools/build_kernel_for_ue.sh` runs this build:

```sh
UE_ROOT=/path/to/UnrealEngine tools/build_kernel_for_ue.sh          # incremental
UE_ROOT=/path/to/UnrealEngine tools/build_kernel_for_ue.sh --clean  # after changing compilers
```

The script configures CMake with the following, then builds only the `sim_core` target:

- `CMAKE_CXX_COMPILER` set to UE's clang++
- `--target=x86_64-unknown-linux-gnu --sysroot=<toolchain> -fPIC -nostdinc++` plus the libc++ include paths
- `CMAKE_POSITION_INDEPENDENT_CODE=ON`
- `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`, so CMake's compiler check doesn't try to link an executable

The kernel tests are not built here. They belong to the host `kernel/build`.

You can override these paths:

| Variable | Default |
|---|---|
| `UE_TOOLCHAIN` | `$LINUX_MULTIARCH_ROOT/x86_64-unknown-linux-gnu`, else the newest toolchain under `UE_ROOT` |
| `UE_LIBCXX` | `$UE_ROOT/Engine/Source/ThirdParty/Unix/LibCxx` |
| `KERNEL_UE_BUILD_DIR` | `kernel/build-ue` |

The equivalent by hand:

```sh
TC=$UE_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/<version>/x86_64-unknown-linux-gnu
CXX_LIB=$UE_ROOT/Engine/Source/ThirdParty/Unix/LibCxx
cmake -S kernel -B kernel/build-ue -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=$TC/bin/clang++ \
  -DCMAKE_CXX_FLAGS="--target=x86_64-unknown-linux-gnu --sysroot=$TC -fPIC -nostdinc++ -isystem $CXX_LIB/include -isystem $CXX_LIB/include/c++/v1" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
cmake --build kernel/build-ue --target sim_core
```

**If your working `build-ue` came from different flags, keep those.** This recipe is reconstructed from the engine's toolchain layout. Nobody recorded the exact invocation that produced the designer's first working `build-ue`. If yours differs, write it here.

## Win64: MSVC

Run this from an *x64 Native Tools Command Prompt for VS 2022*. That is the same MSVC that UE uses.

```bat
cmake -S kernel -B kernel/build-ue -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build kernel/build-ue --target sim_core
```

Ninja is a single-config generator, so this writes `kernel/build-ue/sim_core.lib` where Build.cs looks for it. The Visual Studio generator would write `kernel/build-ue/Release/sim_core.lib` instead. CMake's Release default is the `/MD` runtime, which matches UE.

## Known limitation: the kernel is linked into each dependent module (audit U12)

`PublicAdditionalLibraries` passes the library on to every module that depends on SimRuntime, including `SchizoGame`. In editor builds, each module's `.so` or `.dll` gets its own copy of whatever kernel code it calls. That means game code that calls the C API directly on `GetSimHandle()` runs a second copy of the kernel against the same `SimWorld`.

This is harmless today because the kernel has no mutable globals. It becomes a problem if the kernel ever gains static state, such as caches, registries or RNG singletons. The fix is to route game calls through exported `SIMRUNTIME_API` wrappers in SimRuntime, or to build the kernel as a shared library. That is deferred until wave-3 shows which C API calls the game module actually makes.
