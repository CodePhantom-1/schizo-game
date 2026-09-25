/* Mini-storm — Act I quest + dialogue data from the canon story beats.
 * Two GLM-5.3-Flash authors; the deterministic gate (lint + kernel suite) decides.
 */

interface TableRowResult {
  table: string;
  rows_added: number;
  tags_used: string;
  open_questions: string[];
  notes: string;
  status: string;
}

interface WorkflowReport {
  conclusion: string;
  findings: { where: string; what: string; evidence: string; status: "verified" | "unconfirmed"; severity: "low" | "medium" | "high" }[];
  verified: string[];
  notCovered: string[];
}

const POLICY = [
  "CONTENT POLICY (a violation fails your work):",
  "- db/sources/notes.md is THE canon. CANON tag only for content restating notes/world-bible. Everything you author here is INVENTED glue (Act I quest/dialogue content is authored; the story beats it serves are canon) — tag INVENTED with source_ref citing the canon beat you serve (story.csv id, wb §x, notes L<n>).",
  "- Names of named people come ONLY from db/canon/people.csv (law_giver, the_prophet, the_warchief). Everyone else is a role, not a person (a gatekeeper, a debtor priestess, a caravan master) — do not invent named characters.",
  "- Quest rows MUST serve a canon story beat (db/canon/story.csv: opening/act_i/act_ii; game-design §6). The player is the prisoner: his Act I is arrival, survival, first contact among the groups.",
  "- Anachronism guard: bronze-age world. No coins, no iron, no camels, no clocks. Silver is weighed in grains; beer and bread are rations and wages.",
  "- CSV discipline: append rows; never edit the header or existing rows; quote fields containing commas; edit ONLY your one table file.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

artifact.board("tables", {
  title: "Act I data — quests and dialogues",
  key: "table",
  status: "status",
  columns: ["authoring", "done", "blocked"],
  detail: [
    { field: "rows_added", label: "rows" },
    { field: "tags_used", label: "tags" },
  ],
});

const TABLES: { name: string; brief: string }[] = [
  {
    name: "quests",
    brief: "~10 rows, act='I' (plus 2 for the opening). The prisoner's first steps as quest DEFINITIONS the kernel can accept/fail: survival on arrival (thirst, hunger, shelter); first work for wages; the gate and its tolls; first contact with a group (a cult courier? a rebel pamphlet is too late-era — a whispered meeting); a first small business (a caravan errand, a debt to collect); the temple's pull (an offering errand). kind ∈ history_arc|systemic|faction|emergent. giver = role text ('the gate sergeant', 'a temple steward') or a canon person id. deadline_days small integers or 0.",
  },
  {
    name: "dialogues",
    brief: "~14 rows: the lines that carry those beats — the prisoner's arrival (thecaptain's verdict, a guard's bark), the city's texture (a water-carrier, a market seller, a night watchman), the pull of the groups (a robed stranger who leaves no name, a southern accent asking about the Prophet). speaker = role text or canon person id. context = where/when the line plays. Keep the register austere and mythic — the world is breaking and everyone knows it. Tag INVENTED.",
  },
];

phase("Author Act I quests and dialogues in parallel");
const authored = await Promise.all(
  TABLES.map(async (t) => {
    const result = await agent(`author-${t.name}`).ask<TableRowResult>(
      `You are a content author for the schizo-game canon database (workspace root = repo root).

READ FIRST:
- db/canon/story.csv and db/canon/people.csv            (the canon beats and the only named people)
- db/schema/${t.name}.md and db/canon/${t.name}.csv     (your table — APPEND rows, keep the header)
- docs/game-design.md §6 (the story arc) and §8 (the world's register)
- DECISIONS.md (D-011..D-016 conventions)

YOUR TASK
${t.brief}

Then verify from the repo root: python3 tools/canon_lint.py must end 'LINT PASSED'.

${POLICY}

YOUR ONE FILE: db/canon/${t.name}.csv. Nothing else.

Return the TableRowResult.`,
    );
    const item: TableRowResult = { ...result, status: result.rows_added > 0 ? "done" : "blocked" };
    report(item, "tables");
    return result;
  }),
);

phase("Gate: validators + kernel suite");
const lint = await world.run("python3", ["tools/canon_lint.py"], { timeoutMs: 60000 });
const coverage = await world.run("python3", ["tools/coverage_check.py"], { timeoutMs: 60000 });
await world.run("cmake", ["-S", "kernel", "-B", "kernel/build", "-DCMAKE_BUILD_TYPE=Release"], { timeoutMs: 120000 });
await world.run("cmake", ["--build", "kernel/build", "-j", "4"], { timeoutMs: 300000 });
const test = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
const green = lint.exitCode === 0 && coverage.exitCode === 0 && test.exitCode === 0;

await artifact.markdown(
  "act1-data-report",
  [
    "# Act I quest + dialogue data",
    "",
    `**Gate:** ${green ? "GREEN" : "RED"} — lint ${lint.exitCode === 0 ? "green" : "FAILED"}, coverage ${coverage.exitCode === 0 ? "green" : "FAILED"}, kernel suite ${test.exitCode === 0 ? "passed" : "FAILED"}.`,
    "",
    ...TABLES.map((t) => {
      const a = authored.find((x) => x.table === t.name);
      return `- **${t.name}** — ${a?.rows_added ?? 0} rows (${a?.tags_used ?? ""})${a?.open_questions.length ? " — open: " + a.open_questions.join("; ") : ""}`;
    }),
  ].join("\n"),
  { title: "Act I data report", description: "Quest and dialogue rows from the canon beats, gated.", primary: true },
);

return {
  conclusion: green
    ? `Act I data landed: ${authored.reduce((n, r) => n + r.rows_added, 0)} rows across quests and dialogues, gated green (lint, coverage, kernel suite).`
    : `Act I data cycle ended RED — lint ${lint.exitCode}, coverage ${coverage.exitCode}, suite ${test.exitCode}.`,
  findings: authored.flatMap((r) => r.open_questions.map((q) => ({ where: "db/canon/", what: r.table + ": " + q, evidence: "author escalation", status: "unconfirmed" as const, severity: "low" as const }))),
  verified: ["tools/canon_lint.py", "tools/coverage_check.py", "kernel build + full ctest"],
  notCovered: ["the slice gate still decides whether this content ships in the vertical slice", "UE5 bring-up (editor compiling)"],
};
