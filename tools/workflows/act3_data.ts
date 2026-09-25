/* Mini-storm — Act III quest + dialogue data: the weakening and the breaking.
 * Story beats: story.csv:act_iii (horizon: raids and rebellion; the Empire on
 * its last pillars — the player's work against the Empire and the deity's
 * worship bears fruit). Two GLM-5.3-Flash authors; the gate decides.
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

artifact.board("tables", {
  title: "Act III data — the breaking",
  key: "table",
  status: "status",
  columns: ["authoring", "done", "blocked"],
  detail: [
    { field: "rows_added", label: "rows" },
    { field: "tags_used", label: "tags" },
  ],
});

const POLICY = [
  "CONTENT POLICY (a violation fails your work):",
  "- db/sources/notes.md is THE canon. INVENTED glue only (tag INVENTED) with source_ref citing the canon beat served (story.csv:act_iii, wb §x, notes L<n>). CANON rows only if restating notes verbatim.",
  "- The clock does not bend: the Empire's fall is coming and no quest averts it. Act III is the weakening — the player's work bears fruit, the city feels it.",
  "- The Brotherhood stays hidden (wb §2): no names, no doctrine, no door. The Prophet does not appear again (his one Act II speech stands).",
  "- Named people ONLY from db/canon/people.csv; everyone else is a role.",
  "- Register: the darkest yet — war-rationing, conscription notices, refugees, emptied granaries, rites that cost more as the drought peaks — but never spectacle, never gore-liturgy.",
  "- Anachronism guard: no coins, iron, camels, clocks. Silver in grains. Seasons per D-015. Ranks per D-015. NO named festivals (calendar.csv festival_days is OPEN — descriptive reaping/rains language only).",
  "- CSV discipline: append rows; never edit the header or existing rows; quote fields with commas; edit ONLY your one table file.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

const TABLES: { name: string; brief: string }[] = [
  {
    name: "quests",
    brief: "~10 rows, act='act_iii'. The breaking as work: war-contracts (the city's guard buying spears at any price — the D-011 grain-anchor economy biting); the granary problem (feeding refugees from the east; the player's purse and stores as the difference between order and riot — the purse mechanic, D-017); the rebellion's war-work (the sage's people preparing the south's defence — contact deepens, the rebellion is no longer a whisper); the Empire's counter-pressure (the tribute demand rises; the player's own city-state ambitions strain against it — 'his own city state' beat approaching); the deity's thinning rites (an offering the temple can no longer afford — sea gems and fish cost a famine price now); a defense errand for a village on the eastern road (the raids, from the player's side of the wall this time). kind ∈ history_arc|systemic|faction|emergent.",
  },
  {
    name: "dialogues",
    brief: "~12 rows: the breaking's voices — a conscription crier reading the Empire's demand (sons for the eastern wall); an eastern refugee grandmother (what the raiders left; 'they were herders once' — notes L21, the cattle nomads); a Retributor of Utu, grim, on why the sun god's paladins cannot be everywhere; a priestess whose rites now cost a famine price (the deity's worship thinning — the player's fruit); a warlord's herald from the eastern alliance offering terms no city wants to read; the beerhouse partner (act_ii) counting the days the vintages are worth less than the barrels; a child asking whether the sky is angry (the drought, plainly). Register: austere, mythic, the breaking everywhere, hope rationed like grain.",
  },
];

phase("Author Act III quests and dialogues in parallel");
const authored = await Promise.all(
  TABLES.map(async (t) => {
    const result = await agent(`author-${t.name}`).ask<TableRowResult>(
      `You are a content author for the schizo-game canon database (workspace root = repo root).

READ FIRST:
- db/canon/story.csv (act_iii), people.csv, quests.csv, dialogues.csv  (the Act I-II rows whose register you must darken, not repeat)
- db/schema/${t.name}.md and db/canon/${t.name}.csv                     (your table — APPEND rows, keep the header)
- DECISIONS.md (D-000..D-017), docs/game-design.md §6, docs/world-bible.md §4.1, §4.3

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
  "act3-data-report",
  [
    "# Act III quest + dialogue data",
    "",
    `**Gate:** ${green ? "GREEN" : "RED"} — lint ${lint.exitCode === 0 ? "green" : "FAILED"}, coverage ${coverage.exitCode === 0 ? "green" : "FAILED"}, kernel suite ${test.exitCode === 0 ? "passed" : "FAILED"}.`,
    "",
    ...TABLES.map((t) => {
      const a = authored.find((x) => x.table === t.name);
      return `- **${t.name}** — ${a?.rows_added ?? 0} rows (${a?.tags_used ?? ""})${a?.open_questions.length ? " — open: " + a.open_questions.join("; ") : ""}`;
    }),
  ].join("\n"),
  { title: "Act III data report", description: "The weakening: war-work, granaries, thinning rites.", primary: true },
);

return {
  conclusion: green
    ? `Act III data landed: ${authored.reduce((n, r) => n + r.rows_added, 0)} rows across quests and dialogues, gated green (lint, coverage, kernel suite).`
    : `Act III data cycle ended RED — lint ${lint.exitCode}, coverage ${coverage.exitCode}, suite ${test.exitCode}.`,
  findings: authored.flatMap((r) => r.open_questions.map((q) => ({ where: "db/canon/", what: r.table + ": " + q, evidence: "author escalation", status: "unconfirmed" as const, severity: "low" as const }))),
  verified: ["tools/canon_lint.py", "tools/coverage_check.py", "kernel build + full ctest"],
  notCovered: ["act_iv content (awaits the hidden-ending sequence, §11.12)", "UE5 bring-up (editor compiling)"],
};
