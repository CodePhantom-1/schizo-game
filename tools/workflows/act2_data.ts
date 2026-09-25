/* Mini-storm — Act II quest + dialogue data: the web (business, alliances,
 * the deepening contact with the cult and the rebels). Two GLM-5.3-Flash
 * authors; the deterministic gate decides. Story beats: story.csv:act_ii.
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
  title: "Act II data — the web",
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
  "- db/sources/notes.md is THE canon. INVENTED glue only (tag INVENTED) with source_ref citing the canon beat served (story.csv:act_ii, wb §x, notes L<n>) — plus CANON rows only if restating notes verbatim.",
  "- Named people come ONLY from db/canon/people.csv (law_giver, the_prophet, the_warchief). Everyone else is a role. The Prophet may now APPEAR (Act II is the deepening web) but stays enigmatic: no doctrine revealed beyond the notes (unite the city states, push back the barbarians, fix the drought, rebirth Sumerian culture — notes L19).",
  "- The Brotherhood of the Serpent stays HIDDEN (wb §2): its people are unnamed roles, its doctrine never confirmed, membership never shown. Act II deepens the whispers, not the order (the order itself opens only through the designer's act_iv sequence).",
  "- The player's Act II (game-design §6): the web — business, alliances, contact with the mysterious cult and the rebels. Quests: deeper business (workshops, loans, caravan partnerships per rpg-systems §5), alliance errands, the rebellion's pull, the Brotherhood's second noticing.",
  "- Anachronism guard: no coins, iron, camels, clocks. Silver in grains. Seasons: rains/sowing/harvest/vintage (D-015). Ranks: D-015 labels.",
  "- CSV discipline: append rows; never edit the header or existing rows; quote fields with commas; edit ONLY your one table file.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

const TABLES: { name: string; brief: string }[] = [
  {
    name: "quests",
    brief: "~10 rows, act='act_ii'. The web deepens: a workshop partnership (ship shares are the port's business — City of the Moon is the sea-trade hub, notes L52); a caravan partnership venture; a loan to make (the player becomes a creditor — rpg-systems §5 rates: 20% silver); an alliance errand between city states; the rebellion's work (carry something for the sage of the swamps' people — contact deepens but no doctrine); the Brotherhood's second noticing (the veiled courier returns ONLY because the player has been noticed — never cold, never named); a temple favour at the temple of sun and moon. kind ∈ history_arc|systemic|faction|emergent.",
  },
  {
    name: "dialogues",
    brief: "~12 rows: Act II's voices — the creditor's ledger-keeper (silver by weight, the traditional rates); a warchief's envoy from the east (the alliance of barbarians, war-bargaining); the veiled courier's second appearance (conditional, earned, unnamed order — 'you were seen'); a rebel's question about liberty and the south; the temple of sun and moon's steward on the festival of the reaping; a drowned sailor's tale of the plumed serpent lands (wider world lore, notes L146); the Prophet himself speaking once — enigmatic, four promises only (notes L19). Register: austere, mythic, the breaking everywhere.",
  },
];

phase("Author Act II quests and dialogues in parallel");
const authored = await Promise.all(
  TABLES.map(async (t) => {
    const result = await agent(`author-${t.name}`).ask<TableRowResult>(
      `You are a content author for the schizo-game canon database (workspace root = repo root).

READ FIRST:
- db/canon/story.csv, people.csv, quests.csv, dialogues.csv  (canon beats; the Act I rows whose register you must match and deepen)
- db/schema/${t.name}.md and db/canon/${t.name}.csv           (your table — APPEND rows, keep the header)
- DECISIONS.md (D-000..D-016), docs/game-design.md §5-6, §8, docs/world-bible.md §4

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
  "act2-data-report",
  [
    "# Act II quest + dialogue data",
    "",
    `**Gate:** ${green ? "GREEN" : "RED"} — lint ${lint.exitCode === 0 ? "green" : "FAILED"}, coverage ${coverage.exitCode === 0 ? "green" : "FAILED"}, kernel suite ${test.exitCode === 0 ? "passed" : "FAILED"}.`,
    "",
    ...TABLES.map((t) => {
      const a = authored.find((x) => x.table === t.name);
      return `- **${t.name}** — ${a?.rows_added ?? 0} rows (${a?.tags_used ?? ""})${a?.open_questions.length ? " — open: " + a.open_questions.join("; ") : ""}`;
    }),
  ].join("\n"),
  { title: "Act II data report", description: "The web deepens: business, alliances, the second noticing.", primary: true },
);

return {
  conclusion: green
    ? `Act II data landed: ${authored.reduce((n, r) => n + r.rows_added, 0)} rows across quests and dialogues, gated green (lint, coverage, kernel suite).`
    : `Act II data cycle ended RED — lint ${lint.exitCode}, coverage ${coverage.exitCode}, suite ${test.exitCode}.`,
  findings: authored.flatMap((r) => r.open_questions.map((q) => ({ where: "db/canon/", what: r.table + ": " + q, evidence: "author escalation", status: "unconfirmed" as const, severity: "low" as const }))),
  verified: ["tools/canon_lint.py", "tools/coverage_check.py", "kernel build + full ctest"],
  notCovered: ["act_iii/act_iv content (later cycles)", "UE5 bring-up (editor compiling)"],
};
