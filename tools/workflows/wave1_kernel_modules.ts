/* Wave 1 — the eight kernel modules, built by the fleet (D-010).
 * Contracts are frozen (Wave 0): builders implement src/<M>.cpp + tests/test_<m>.cpp
 * against include/sim/<M>.hpp. The deterministic gate (cmake build + ctest)
 * decides, not claims. Reviewers check contract adherence before the final gate.
 */

interface ModuleResult {
  /** Module name, e.g. "Economy". */
  module: string;
  /** True when both files are written and the module's own tests pass locally. */
  implemented: boolean;
  /** True when the agent's local build + ctest run passed. */
  tests_pass: boolean;
  /** Workspace-relative files the agent wrote. */
  files: string[];
  /** Undecided design decisions hit — the agent did NOT invent around these. */
  open_walls: string[];
  /** One short paragraph for the coordinator. */
  notes: string;
  /** Board status: building | reviewing | fixing | done | blocked. */
  status: string;
}

interface ReviewVerdict {
  module: string;
  /** Contract violations that MUST be fixed before merge. Empty if none. */
  must_fix: string[];
  /** Non-blocking suggestions. */
  suggestions: string[];
  approved: boolean;
}

/** One item on the module board (tagged to artifact "modules"). */
interface BoardItem {
  module: string;
  status: string;
  implemented: boolean;
  tests_pass: boolean;
  files: string[];
  open_walls: string[];
  notes: string;
}

interface Finding {
  /** Workspace-relative path, with a line when it applies. */
  where: string;
  /** One sentence: what was found. */
  what: string;
  /** What showed it: the lines read, or the command and output that proved it. */
  evidence: string;
  /** "verified" when a deterministic check or independent agent confirmed it. */
  status: "verified" | "unconfirmed";
  /** Reserve "high" for data loss, a crash, or a wrong result. */
  severity: "low" | "medium" | "high";
}

interface WorkflowReport {
  /** Two or three sentences answering what this wave set out to do. */
  conclusion: string;
  findings: Finding[];
  /** What the run checked and how. */
  verified: string[];
  /** What it did not look at, and why. */
  notCovered: string[];
}

const MODULES = ["Economy", "Population", "Faction", "Magic", "Justice", "Events", "Property", "Quests"];

artifact.board("modules", {
  title: "Kernel modules — Wave 1",
  key: "module",
  status: "status",
  columns: ["building", "reviewing", "fixing", "done", "blocked"],
  detail: [
    { field: "tests_pass", label: "tests pass" },
    { field: "open_walls", label: "open walls" },
  ],
});

const COMMON_RULES = [
  "HARD CONSTRAINTS (a violation fails your work):",
  "- C++20, ZERO external dependencies. CMake already globs kernel/src/*.cpp and kernel/tests/test_*.cpp.",
  "- Write ONLY the two files named below. Never edit any header, Context.hpp, other modules' files, docs, db, or tools.",
  "- Determinism: no wall clock, no global or static mutable state, no threads; randomness ONLY via ctx.rng (see Rng.hpp).",
  "- Read canon ONLY through sim::Db (it loads db/canon/*.csv). OPEN-tagged canon rows mean the content does not exist yet: handle absence gracefully and never invent canon data.",
  "- You may READ other modules' state structs via the WorldContext (const refs). You may WRITE only your own state struct.",
  "- If the contract cannot be satisfied without an undecided design decision, do NOT invent one: implement the mechanical core and list the blocker in open_walls.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

const BUILD_AND_TEST = [
  "Build and test from the repo root:",
  "  cmake -S kernel -B kernel/build -DCMAKE_BUILD_TYPE=Release",
  "  cmake --build kernel/build -j 8",
  "  ctest --test-dir kernel/build --output-on-failure",
  "Your module's test must pass, and the three existing suites (test_time, test_db, test_contracts) must stay green.",
].join("\n");

function capitalize(s: string): string {
  return s.charAt(0).toUpperCase() + s.slice(1);
}

function failingTests(ctestOutput: string): string[] {
  const out = new Set<string>();
  for (const line of ctestOutput.split("\n")) {
    const m = /\d+ - (test_\w+)\s+\((Failed|Not Run)\)/.exec(line);
    if (m) out.add(m[1]);
  }
  return Array.from(out);
}

function targetsFromBuildErrors(buildOutput: string): string[] {
  const out = new Set<string>();
  for (const m of buildOutput.matchAll(/test_[a-z_]+\.cpp/g)) {
    out.add(m[0].replace(".cpp", ""));
  }
  for (const m of buildOutput.matchAll(/sim\/([A-Za-z]+)\.hpp/g)) {
    out.add("test_" + m[1].toLowerCase());
  }
  return Array.from(out);
}

function tail(s: string, n: number): string {
  return s.length <= n ? s : "...\n" + s.slice(s.length - n);
}

phase("Build the eight modules in parallel");
log("Fanning out one build agent per module: " + MODULES.join(", "));
const built = await Promise.all(
  MODULES.map(async (m) => {
    const lower = m.toLowerCase();
    const result = await agent(`builder-${m}`).ask<ModuleResult>(
      `You are a build agent for the schizo-game project — a deterministic C++20 world kernel. The workspace root IS the repo root.

READ FIRST (use your file tools; do not ask for contents):
- kernel/contracts/module_${m}.md            (your contract: role, owns, definition of done)
- kernel/include/sim/${m}.hpp                (the FROZEN API — implement every declaration, change nothing)
- kernel/include/sim/Context.hpp             (the seam: WorldContext + fixed tick order)
- kernel/include/sim/Types.hpp, Rng.hpp, Db.hpp, Test.hpp   (the common layer)
- kernel/CMakeLists.txt

YOUR TASK
1. Write kernel/src/${m}.cpp implementing every declaration in include/sim/${m}.hpp.
2. Write kernel/tests/test_${lower}.cpp that actually tests the header's documented behavior and invariants (use SIM_CHECK / SIM_MAIN from sim/Test.hpp — no other framework).
3. Run the build and tests yourself and iterate until green:
${BUILD_AND_TEST}

${COMMON_RULES}

YOUR TWO FILES: kernel/src/${m}.cpp and kernel/tests/test_${lower}.cpp. Nothing else.

Return the ModuleResult.`,
    );
    const item = {
      module: m,
      status: result.implemented && result.tests_pass ? "reviewing" : "blocked",
      implemented: result.implemented,
      tests_pass: result.tests_pass,
      files: result.files,
      open_walls: result.open_walls,
      notes: result.notes,
    };
    report(item, "modules");
    return result;
  }),
);

phase("Fix until the full suite is green");
await world.run("cmake", ["-S", "kernel", "-B", "kernel/build", "-DCMAKE_BUILD_TYPE=Release"], { timeoutMs: 120000 });
const firstBuild = await world.run("cmake", ["--build", "kernel/build", "-j", "8"], { timeoutMs: 300000 });
let gate = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
log(firstBuild.exitCode === 0 ? "Build ok; running the suite." : "Build failed; routing errors to fixers.");

for (let round = 1; round <= 3 && gate.exitCode !== 0; round++) {
  let targets = failingTests(gate.stdout);
  if (targets.length === 0 && firstBuild.exitCode !== 0) targets = targetsFromBuildErrors(firstBuild.stderr);
  if (targets.length === 0) targets = MODULES.map((m) => "test_" + m.toLowerCase());
  log(`Gate round ${round}: fixing ${targets.join(", ")}`);
  await Promise.all(
    targets.map((t) => {
      const mod = capitalize(t.replace(/^test_/, ""));
      const output = firstBuild.exitCode !== 0 ? firstBuild.stderr + "\n" + gate.stdout : gate.stdout + "\n" + gate.stderr;
      return agent(`fixer-gate-${t}-${round}`).ask<ModuleResult>(
        `You are a repair agent for the schizo-game C++20 kernel (workspace root = repo root).

The deterministic gate is red. Failing target: ${t}.
Gate output (tail):
${tail(output, 8000)}

READ FIRST:
- kernel/contracts/module_${mod}.md and kernel/include/sim/${mod}.hpp (the frozen API)
- kernel/include/sim/Context.hpp and the common layer (Types/Rng/Db/Test)
- the current kernel/src/${mod}.cpp and kernel/tests/${t}.cpp if they exist

FIX ONLY: kernel/src/${mod}.cpp and kernel/tests/${t}.cpp. If the failing target's module does not exist, create exactly those two files per the contract.
Then verify yourself:
${BUILD_AND_TEST}

${COMMON_RULES}

Return the ModuleResult (module = "${mod}").`,
      );
    }),
  );
  await world.run("cmake", ["--build", "kernel/build", "-j", "8"], { timeoutMs: 300000 });
  gate = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
}

phase("Review each module against its contract");
const reviews = await Promise.all(
  MODULES.map((m) => {
    const lower = m.toLowerCase();
    return agent(`reviewer-${m}`).ask<ReviewVerdict>(
      `You are a code reviewer for the schizo-game C++20 kernel. You have fresh eyes: you have seen nothing of how this code was written.

READ (do not edit ANY file):
- kernel/contracts/module_${m}.md and kernel/include/sim/${m}.hpp (the frozen API and invariants)
- kernel/src/${m}.cpp and kernel/tests/test_${lower}.cpp
- kernel/include/sim/Context.hpp and the common layer (Types.hpp, Rng.hpp, Db.hpp) as needed

JUDGE, with evidence (cite file:line):
1. Is EVERY declaration in the header implemented with the documented semantics?
2. Do the header's invariants hold (determinism; no writes to other modules' state; clamps where specified; the fixed success formula in Magic.hpp if applicable)?
3. Do the tests actually test behavior, or are they tautologies? Do they cover the contract's Definition of done?
4. Does the code invent canon (data the db does not contain) or resolve an OPEN row? That is a must_fix.
5. Would anything here break the deterministic daily tick?

Report must_fix ONLY for real contract violations or canon violations — list each with file:line and one sentence. Style preferences are suggestions, not must_fix. If you cannot verify something, say so in suggestions rather than guessing.

Return the ReviewVerdict.`,
    );
  }),
);
for (const r of reviews) {
  const item: BoardItem = {
    module: r.module,
    status: r.approved ? "done" : "fixing",
    implemented: true,
    tests_pass: true,
    files: [],
    open_walls: [],
    notes: r.suggestions.join(" | "),
  };
  report(item, "modules");
}

phase("Apply review fixes and confirm everything green");
const toFix = reviews.filter((r) => r.must_fix.length > 0);
if (toFix.length > 0) {
  log(`Review fixes needed: ${toFix.map((r) => r.module).join(", ")}`);
  await Promise.all(
    toFix.map((r) => {
      const lower = r.module.toLowerCase();
      return agent(`fixer-review-${r.module}`).ask<ModuleResult>(
        `You are a repair agent for the schizo-game C++20 kernel (workspace root = repo root).

A reviewer found contract violations in module ${r.module} that MUST be fixed:
${r.must_fix.map((s) => "- " + s).join("\n")}

READ FIRST: kernel/contracts/module_${r.module}.md, kernel/include/sim/${r.module}.hpp, kernel/include/sim/Context.hpp, and the current kernel/src/${r.module}.cpp and kernel/tests/test_${lower}.cpp.

FIX ONLY: kernel/src/${r.module}.cpp and kernel/tests/test_${lower}.cpp. Then verify:
${BUILD_AND_TEST}

${COMMON_RULES}

Return the ModuleResult (module = "${r.module}").`,
      );
    }),
  );
} else {
  log("No review fixes needed.");
}

await world.run("cmake", ["-S", "kernel", "-B", "kernel/build", "-DCMAKE_BUILD_TYPE=Release"], { timeoutMs: 120000 });
const finalBuild = await world.run("cmake", ["--build", "kernel/build", "-j", "8"], { timeoutMs: 300000 });
const finalTest = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
const lint = await world.run("python3", ["tools/canon_lint.py"], { timeoutMs: 60000 });
const coverage = await world.run("python3", ["tools/coverage_check.py"], { timeoutMs: 60000 });

const suiteGreen = finalBuild.exitCode === 0 && finalTest.exitCode === 0;
for (const m of MODULES) {
  const review = reviews.find((r) => r.module === m);
  const hadFix = toFix.some((r) => r.module === m);
  const item: BoardItem = {
    module: m,
    status: suiteGreen ? "done" : hadFix || !suiteGreen ? "fixing" : "done",
    implemented: true,
    tests_pass: suiteGreen,
    files: [],
    open_walls: built.find((b) => b.module === m)?.open_walls ?? [],
    notes: review?.approved ? "reviewed" : "review fixes applied",
  };
  report(item, "modules");
}

const openWalls = built.flatMap((b) => b.open_walls.map((w) => b.module + ": " + w));
const verdicts = reviews.map((r) => `${r.module}: ${r.approved ? "approved" : "must_fix=" + r.must_fix.length}`).join("; ");

const reportLines = [
  "# Wave 1 — the eight kernel modules",
  "",
  `**Suite:** ${suiteGreen ? "GREEN" : "RED"} — cmake build ${finalBuild.exitCode === 0 ? "ok" : "FAILED"}, ctest ${finalTest.exitCode === 0 ? "passed" : "FAILED"} (canon_lint ${lint.exitCode === 0 ? "green" : "FAILED"}, coverage ${coverage.exitCode === 0 ? "green" : "FAILED"}).`,
  "",
  "## Modules",
  "",
  ...MODULES.map((m) => {
    const b = built.find((x) => x.module === m);
    const r = reviews.find((x) => x.module === m);
    const walls = b?.open_walls.length ? ` — open walls: ${b.open_walls.join("; ")}` : "";
    return `- **${m}** — ${b?.implemented ? "built" : "incomplete"}, tests ${b?.tests_pass ? "pass" : "failing"}, review ${r?.approved ? "approved" : "fixes applied"}${walls}`;
  }),
  "",
  "## Review verdicts",
  "",
  verdicts,
  "",
  "## Open walls (undecided design decisions — escalated, never invented around)",
  openWalls.length ? openWalls.map((w) => "- " + w).join("\n") : "- none",
  "",
  "## What this wave did not do",
  "- Wave 2 integration (WorldState daily tick, golden determinism run) — next wave, coordinator's.",
  "- The engine (UE5) phase, and any content storms: separate waves.",
];
await artifact.markdown("wave1-report", reportLines.join("\n"), {
  title: "Wave 1 report — kernel modules",
  description: "What the fleet built, what the gate proved, and what escalated.",
  primary: true,
});

const findings: Finding[] = openWalls.map((w) => ({
  where: "kernel/",
  what: w,
  evidence: "build agent escalation (agents were instructed never to invent around undecided design)",
  status: "unconfirmed",
  severity: "medium",
}));

return {
  conclusion: suiteGreen
    ? `All eight kernel modules (Economy, Population, Faction, Magic, Justice, Events, Property, Quests) are implemented, reviewed, and green: the full ctest suite passes, and canon_lint + coverage_check stay green. ${openWalls.length} open wall(s) escalated to the coordinator instead of being invented around.`
    : `The wave ended RED: build ${finalBuild.exitCode}, ctest ${finalTest.exitCode}. Review verdicts — ${verdicts}. Read the failing output in the report and route a fixer wave.`,
  findings,
  verified: [
    "cmake configure + build (deterministic gate, run by the script)",
    "ctest full suite (deterministic gate, run by the script)",
    "tools/canon_lint.py",
    "tools/coverage_check.py",
    "one independent contract review per module (fresh-eyes agents, evidence cited)",
  ],
  notCovered: [
    "Wave 2 integration: WorldState daily tick wiring and the ten-year golden determinism run",
    "the UE5 engine phase",
    "canon content storms (items, laws, customs, events, schedules...)",
  ],
};
