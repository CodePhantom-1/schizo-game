/* Wave 3 — scenario suites: multi-system proof of the kernel before the
 * engine binds to it (plan v2 §5, wave 3). One GLM-5.3-Flash agent per
 * scenario; the deterministic gate decides.
 */

interface ScenarioResult {
  /** Scenario name. */
  scenario: string;
  /** Workspace-relative test file written. */
  file: string;
  /** True when the full local suite passed. */
  suite_pass: boolean;
  /** What the scenario proves, one sentence. */
  proves: string;
  /** Escalations — never invented around. */
  open_walls: string[];
  notes: string;
  status: string;
}

interface WorkflowReport {
  conclusion: string;
  findings: { where: string; what: string; evidence: string; status: "verified" | "unconfirmed"; severity: "low" | "medium" | "high" }[];
  verified: string[];
  notCovered: string[];
}

artifact.board("scenarios", {
  title: "Wave 3 — scenario suites",
  key: "scenario",
  status: "status",
  columns: ["writing", "fixing", "done", "blocked"],
  detail: [{ field: "proves", label: "proves" }],
});

const RULES = [
  "HARD CONSTRAINTS:",
  "- Write ONLY your one test file. Never edit headers, sources, other tests, docs, db, or tools.",
  "- C++20, zero deps; use sim/Test.hpp (SIM_CHECK / SIM_MAIN); a WorldState is created with w.init(\"../db/canon\", seed) — ctest runs from kernel/.",
  "- Determinism: fixed seeds; assert exact values where the contracts guarantee them, computed-from-canon values where they don't (never hardcode content numbers you didn't compute from db/canon).",
  "- Do not weaken or edit any existing test. If the kernel looks wrong, say so in open_walls instead of papering over it.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

const SCENARIOS = [
  {
    name: "year-under-drought",
    brief: "kernel/tests/test_scenario_drought.cpp — advance a full 360-day year at drought 0 and record the City of the Moon grain price curve; then a second world at drought 3 for the same year. Prove: the drought-3 curve is strictly dearer than the drought-0 curve at every month's end, and both worlds are individually deterministic (re-run one twice, identical prices).",
  },
  {
    name: "oath-lifecycle",
    brief: "kernel/tests/test_scenario_oaths.cpp — through the C++ API (Faction.hpp): add standing to reach each tier (Stranger/Known/Trusted/Sworn/Oath-bound), swear an oath, prove the one-oath rule refuses a second unbroken oath, break the oath, prove the curse flag sets, standing drops by the documented amount, and the swearer may swear again. Then verify the whole lifecycle is deterministic across two identical seeds.",
  },
  {
    name: "debts-and-deadlines",
    brief: "kernel/tests/test_scenario_debts.cpp — through Property.hpp and Quests.hpp: issue a loan at 20%, advance days, prove accrued interest follows the documented formula; repay part and prove the balance; repay fully and prove it closes. Then accept a quest with a deadline, advance past it, prove tick_quests fails it exactly once and it lands in failed_list; a quest with no deadline never fails. Steward report lines must appear as assets earn.",
  },
];

phase("Write the three scenario suites in parallel");
log("Scenarios: " + SCENARIOS.map((s) => s.name).join(", "));
const built = await Promise.all(
  SCENARIOS.map(async (s) => {
    const result = await agent(`scenario-${s.name}`).ask<ScenarioResult>(
      `You are a test engineer for the schizo-game C++20 world kernel (workspace root = repo root). The kernel is complete and green; your scenarios harden it before the engine binds to it.

READ FIRST:
- kernel/include/sim/World.hpp and Context.hpp (how a world is created and ticked)
- the module headers your scenario touches (Economy.hpp / Faction.hpp / Property.hpp / Quests.hpp)
- an existing test for style: kernel/tests/test_world.cpp and kernel/tests/test_economy.cpp
- kernel/contracts/*.md as needed

YOUR TASK
${s.brief}

Then run the full suite yourself and iterate until green:
  cmake -S kernel -B kernel/build -DCMAKE_BUILD_TYPE=Release
  cmake --build kernel/build -j 4
  ctest --test-dir kernel/build --output-on-failure
(use -j 4 for the build: another long compile shares this machine)

${RULES}

Return the ScenarioResult.`,
    );
    const item: ScenarioResult = { ...result, status: result.suite_pass ? "done" : "blocked" };
    report(item, "scenarios");
    return result;
  }),
);

phase("Gate the full suite");
await world.run("cmake", ["-S", "kernel", "-B", "kernel/build", "-DCMAKE_BUILD_TYPE=Release"], { timeoutMs: 120000 });
await world.run("cmake", ["--build", "kernel/build", "-j", "4"], { timeoutMs: 300000 });
let gate = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
for (let round = 1; round <= 2 && gate.exitCode !== 0; round++) {
  log(`Gate round ${round}: routing failures to repair`);
  await agent(`gate-fixer-${round}`).ask<string>(
    `You are a repair agent for the schizo-game C++20 kernel (workspace root = repo root). The full suite is red after new scenario tests landed. Output (tail):\n${gate.stdout.slice(-6000)}\n\nFix the failing scenario test(s) to be correct per the frozen contracts (kernel/include/sim/*.hpp, kernel/contracts/*.md) — never weaken guarantees, never edit other tests. Then verify: cmake --build kernel/build -j 4 && ctest --test-dir kernel/build --output-on-failure. Return a one-paragraph summary.`,
  );
  await world.run("cmake", ["--build", "kernel/build", "-j", "4"], { timeoutMs: 300000 });
  gate = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
}
const green = gate.exitCode === 0;
const lint = await world.run("python3", ["tools/canon_lint.py"], { timeoutMs: 60000 });

await artifact.markdown(
  "wave3-report",
  [
    "# Wave 3 — scenario suites",
    "",
    `**Gate:** ${green ? "GREEN" : "RED"} — full suite ${gate.exitCode === 0 ? "passed" : "FAILED"}; canon_lint ${lint.exitCode === 0 ? "green" : "FAILED"}.`,
    "",
    ...SCENARIOS.map((s) => {
      const b = built.find((x) => x.scenario === s.name);
      return `- **${s.name}** — ${b?.file ?? "missing"}: ${b?.proves ?? "?"}${b?.open_walls.length ? " — open walls: " + b.open_walls.join("; ") : ""}`;
    }),
  ].join("\n"),
  { title: "Wave 3 report — scenario suites", description: "What the scenarios prove and the gate result.", primary: true },
);

return {
  conclusion: green
    ? "All three scenario suites landed and the full kernel suite is green: the drought curve, the oath lifecycle and the debts-and-deadlines machinery are proven across multiple systems before engine bring-up."
    : `Wave 3 ended RED (ctest exit ${gate.exitCode}); read the report and route a fix wave.`,
  findings: built.flatMap((b) => b.open_walls.map((w) => ({ where: "kernel/", what: b.scenario + ": " + w, evidence: "scenario agent escalation", status: "unconfirmed" as const, severity: "low" as const }))),
  verified: ["cmake build + full ctest (deterministic gate, run by the script)", "tools/canon_lint.py"],
  notCovered: ["UE5 engine bring-up (install still running)", "the C API surface is tested by kernel/tests/test_capi.cpp, not re-tested here"],
};
