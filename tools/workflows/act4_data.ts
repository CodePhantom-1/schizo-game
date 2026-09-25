/* Mini-storm — Act IV quest + dialogue data: the endings and the founding.
 * SCOPED (designer rule, §11.12): the hidden ending's sequence is the
 * designer's and is NOT authored here. Ending 4 gets only its door's frame —
 * no steps, no sequence rows. Endings 1-3 and the kingship founding are canon
 * (endings.csv) and are served. Two GLM-5.3-Flash authors; the gate decides.
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
  title: "Act IV data — the endings and the founding",
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
  "- THE HARD LINE: you do NOT author the hidden ending's sequence. No row may show, hint, or scaffold the Brotherhood's door opening, the summoning, or any step of the specific sequence (game-design §11.12 — the designer's). The Brotherhood's people remain unnamed roles; the order stays shut.",
  "- The three canon endings and the kingship are served from db/canon/endings.csv (verbatim canon: the Empire ending; the Sumerian ending with its hidden price; the Barbarian ending's council of warlords) and the founding requirements (endings §2: people, food, walls, temple and patron god, recognition, the royal seal).",
  "- The clock does not bend: the Empire falls on schedule in every path; the endings decide what rises.",
  "- Named people ONLY from db/canon/people.csv (the Prophet may appear in Act IV canon lines — his last rebellion is ENDING 4 canon, but his ending-4 scenes are OUT OF SCOPE here; do not author them). Everyone else is a role.",
  "- Register: the endings are weighted, each with its price in the canon's own words. The founding is austere: first stones, first laws, first harvests of one's own.",
  "- Anachronism guard: no coins, iron, camels, clocks. Silver in grains. Seasons per D-015. Ranks per D-015 (Lugal is the founding's crown).",
  "- CSV discipline: append rows; never edit the header or existing rows; quote fields with commas; edit ONLY your one table file.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

const TABLES: { name: string; brief: string }[] = [
  {
    name: "quests",
    brief: "~10 rows, act='act_iv'. The founding work (rank 6, Lugal — endings §2's five requirements as quests): the people (settlers and refugees given roofs and a reason), the granary (a year's rations laid in), the walls (the first stones on the ground the player has earned), the temple and the patron god (the founding rite, high favour, D-016 offering rows available), recognition (a treaty or open independence — the notes' 'right of kingship', notes L39). Plus one quest per canon path's approach: the Empire's summons (accept the past life? the throne of a breaking thing), the Prophet's last request (Act IV support without authoring ending-4's scenes — carry the south's preparations, nothing more), the eastern road out (the Barbarian path's abandonment of the river lands). kind ∈ history_arc|faction|emergent.",
  },
  {
    name: "dialogues",
    brief: "~10 rows: the endings' voices — the descendant's steward (the Empire ending's supplanting looms; 'the throne does not notice it is breaking'); a rebel captain on what victory costs ('we put Akkad in ashes and something was already waiting in the ashes' — the canon's own hidden-price hint, endings §3.2, D-004 ambiguity kept); a warlord's terms ('no crown among us' — the council, notes L36); the founding voices (a settler's first roof; the seal-cutter asking whose face goes on the seal — the player's own, the notes' founding); the cliffhanger's edge WITHOUT its content: an unnamed presence telling the player he is needed elsewhere is OUT OF SCOPE (that is the hidden path's frame, the designer's §5 cliffhanger) — instead, end on the city's own voice: 'the first law is yours to speak' (the founding, the kingdom, the counteraction of the degenerate forces — notes L11). Register: weighted, each price in the canon's own words.",
  },
];

phase("Author Act IV quests and dialogues in parallel");
const authored = await Promise.all(
  TABLES.map(async (t) => {
    const result = await agent(`author-${t.name}`).ask<TableRowResult>(
      `You are a content author for the schizo-game canon database (workspace root = repo root).

READ FIRST:
- db/canon/endings.csv (THE canon for this act — verbatim ending lore and prices)
- db/canon/story.csv (act_iv, epilogue), people.csv, quests.csv, dialogues.csv
- db/schema/${t.name}.md and db/canon/${t.name}.csv   (your table — APPEND rows, keep the header)
- DECISIONS.md (D-000..D-017), docs/endings.md §2-3, docs/game-design.md §6, docs/world-bible.md §4

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
  "act4-data-report",
  [
    "# Act IV quest + dialogue data (scoped)",
    "",
    `**Gate:** ${green ? "GREEN" : "RED"} — lint ${lint.exitCode === 0 ? "green" : "FAILED"}, coverage ${coverage.exitCode === 0 ? "green" : "FAILED"}, kernel suite ${test.exitCode === 0 ? "passed" : "FAILED"}.`,
    "",
    `**Out of scope by rule:** the hidden ending's sequence (game-design §11.12 — the designer's). Ending 4's door is framed, never opened.`,
    "",
    ...TABLES.map((t) => {
      const a = authored.find((x) => x.table === t.name);
      return `- **${t.name}** — ${a?.rows_added ?? 0} rows (${a?.tags_used ?? ""})${a?.open_questions.length ? " — open: " + a.open_questions.join("; ") : ""}`;
    }),
  ].join("\n"),
  { title: "Act IV data report", description: "The three canon paths and the founding; the hidden door untouched.", primary: true },
);

return {
  conclusion: green
    ? `Act IV data landed (scoped): ${authored.reduce((n, r) => n + r.rows_added, 0)} rows across quests and dialogues — the three canon paths, the founding work, the hidden door untouched. Gated green.`
    : `Act IV data cycle ended RED — lint ${lint.exitCode}, coverage ${coverage.exitCode}, suite ${test.exitCode}.`,
  findings: authored.flatMap((r) => r.open_questions.map((q) => ({ where: "db/canon/", what: r.table + ": " + q, evidence: "author escalation", status: "unconfirmed" as const, severity: "low" as const }))),
  verified: ["tools/canon_lint.py", "tools/coverage_check.py", "kernel build + full ctest"],
  notCovered: ["the hidden ending's sequence and its content — the designer's (game-design §11.12)", "UE5 bring-up (compile finishing)"],
};
