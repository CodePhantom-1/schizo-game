// SimRuntime.Build.cs — binds the world kernel to Unreal.
// Consumes the kernel built by CMake with UE's own toolchain:
//   kernel/build-ue/libsim_core.a (Linux/Mac) or kernel/build-ue/sim_core.lib (Win64)
//   + kernel/include (the C API, sim/CApi.h).
// How to build kernel/build-ue: docs/kernel-build-for-ue.md
// (Linux: tools/build_kernel_for_ue.sh — UE's bundled clang, libc++, -fPIC).
// The kernel is the product's deterministic core (plan v2 §5); the engine renders it.
using UnrealBuildTool;

public class SimRuntime : ModuleRules
{
	public SimRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The kernel headers (repo root is five levels up from the module dir).
		PublicIncludePaths.Add(System.IO.Path.Combine(
			ModuleDirectory, "..", "..", "..", "..", "..", "kernel", "include"));

		// The CMake-built kernel static library (Release), built with UE's
		// toolchain — see docs/kernel-build-for-ue.md. Fully qualified: UBT warns
		// (slow deps) on unresolvable relative library paths.
		string KernelLib = Target.Platform == UnrealTargetPlatform.Win64 ? "sim_core.lib" : "libsim_core.a";
		string KernelPath = System.IO.Path.GetFullPath(System.IO.Path.Combine(
			ModuleDirectory, "..", "..", "..", "..", "..", "kernel", "build-ue", KernelLib));
		if (!System.IO.File.Exists(KernelPath))
		{
			// Not thrown: UBT also runs this file while generating project files,
			// before the kernel may exist. The link step fails on the path below.
			System.Console.WriteLine("SimRuntime: warning: kernel library missing at " + KernelPath +
				" - build it with UE's toolchain (docs/kernel-build-for-ue.md).");
		}
		PublicAdditionalLibraries.Add(KernelPath);

		// Engine types appear in the public headers (world/game-instance subsystems).
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
