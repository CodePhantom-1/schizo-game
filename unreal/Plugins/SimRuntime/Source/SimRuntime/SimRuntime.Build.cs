// SimRuntime.Build.cs — PLACEHOLDER (authored pre-editor; bring-up fixes expected).
// Consumes the kernel built by CMake: kernel/build/libsim_core.a + kernel/include.
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

		// The CMake-built kernel static library (Release). MUST be built with
		// tools/build_kernel_for_ue.sh, not a plain `cmake -S kernel -B kernel/build-ue`:
		// that uses the host g++/glibc, which fails to link into this module (ABI/symbol
		// mismatch against UBT's bundled clang+sysroot toolchain). See that script's
		// header comment for the full story.
		// Fully qualified: UBT warns (slow deps) on unresolvable relative library paths.
		PublicAdditionalLibraries.Add(System.IO.Path.Combine(
			ModuleDirectory, "..", "..", "..", "..", "..", "kernel", "build-ue", "libsim_core.a"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine"
		});
	}
}
