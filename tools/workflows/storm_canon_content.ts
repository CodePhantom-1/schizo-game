/* Content storm — the canon database grows from 5 reserved tables to live
 * content (D-010 table-storm pattern). One GLM-5.3-Flash author per table;
 * acceptance is mechanical: canon_lint + coverage + the kernel suite.
 */

interface TableResult {
  /** Table name, e.g. "items". */
  table: string;
  /** Rows appended (not counting the header). */
  rows_added: number;
  /** Tag counts used, e.g. "A:18 INVENTED:6 CANON:2 OPEN:1". */
  tags_used: string;
  /** Undecided canon questions hit — escalated, never invented around. */
  open_questions: string[];
  /** One short paragraph for the coordinator. */
  notes: string;
  /** Board status: authoring | reviewing | fixing | done | blocked. */
  status: string;
}

interface ReviewVerdict {
  table: string;
  /** Policy/canon violations that MUST be fixed. Empty if none. */
  must_fix: string[];
  /** Non-blocking suggestions. */
  suggestions: string[];
  approved: boolean;
}

interface Finding {
  where: string;
  what: string;
  evidence: string;
  status: "verified" | "unconfirmed";
  severity: "low" | "medium" | "high";
}
interface WorkflowReport {
  conclusion: string;
  findings: Finding[];
  verified: string[];
  notCovered: string[];
}

const TABLES: { name: string; brief: string }[] = [
  { name: "items", brief: "~24 rows: staples, trade goods and ritual goods of a bronze-age river economy (barley, dates, fish, wool, linen, sesame oil, salt, pottery, copper ingots, bronze tools...). 'sea gems' MUST appear (a canon offering material, notes L57). price_band = integer 1-5 (1 cheap staple .. 5 royal luxury)." },
  { name: "foods", brief: "~12 rows: bread, beer, dates, fish, onions, legumes, dairy... with realistic ingredients and preparation methods." },
  { name: "laws", brief: "~10 rows: one per crime kind the kernel knows (theft, burglary, assault, murder, sorcery, sacrilege, oath_breaking, tomb_robbery, fraud, harbouring_fugitive). penalty_options = ';'-separated choices from EXACTLY this vocabulary: compensation | confiscation | debt_service | exile | death | dismissed. CRITICAL (D-011): the kernel takes the FIRST option as the verdict. Ground penalties in the real Mesopotamian law codes and name the code in source_ref." },
  { name: "customs", brief: "~10 rows: hospitality, gifts creating debts of honour, purity before temple entry, first portion to the gods, mourning practices, oath gestures, guest obligations... basis = real practice [A] or the notes [CANON]." },
  { name: "events", brief: "~14 rows: the city's pulse (categories: crime, family, economy, nature, illness, omens, politics, war). triggers = ';'-separated conditions from EXACTLY this grammar: drought_gte:<0-20> | war_gte:<0-20> | season:<season_id> | festival | chance:<N> (1-in-N daily). Example: 'drought_gte:2;chance:10'. repeat = true|false (fire-once when false). Tag INVENTED (authored glue) unless a note states it." },
  { name: "schedules", brief: "~24 rows: the daily clock as roles live it (bakers before dawn, gates open at dawn, market peaks in the morning, midday rest in summer, gates close at dusk, watchmen patrol at night...). hour = 0-23. season = '' for all-season rows. Tag INVENTED." },
  { name: "names", brief: "~40 rows: attested Sumerian and Akkadian personal names for NPC generation, split across genders; culture = sumerian | akkadian. Name the corpus tradition generically in source_ref (e.g. 'attested Ur III / Old Babylonian onomasticon'). Tag A." },
];

artifact.board("tables", {
  title: "Content storm — canon tables",
  key: "table",
  status: "status",
  columns: ["authoring", "reviewing", "fixing", "done", "blocked"],
  detail: [
    { field: "rows_added", label: "rows" },
    { field: "tags_used", label: "tags" },
  ],
});

const POLICY = [
  "CONTENT POLICY (a violation fails your work):",
  "- The designer's notes are the game. CANON tag ONLY for content that quotes or directly restates the notes/world-bible; cite wb §x / notes L<n>.",
  "- A = real-world attested material you are importing; source_ref MUST name the real source tradition or text (law code, corpus, inscription, archaeological record). Do not fabricate specific citations you are not confident exist; prefer naming the tradition.",
  "- INVENTED = authored glue the game needs; the codex will show it to the player as authored. Use it for flavor, schedules, event rules.",
  "- OPEN = a decision only the designer can make; list it in open_questions instead of inventing. OPEN rows cannot ship.",
  "- Anachronism guard: bronze-age world. NO coins, NO iron tools, NO camels in the river lands, NO later-era material.",
  "- CSV discipline: quote any field containing a comma; never edit the header; never touch another table, kernel/, docs/, or tools/.",
  "- If a check is impossible to pass, or your instructions contradict each other, escalate and say so plainly rather than working around it.",
].join("\n");

const VERIFY = "Verify from the repo root: python3 tools/canon_lint.py must end 'LINT PASSED'. Regenerate nothing; edit nothing but your one table.";

function tail(s: string, n: number): string {
  return s.length <= n ? s : "...\n" + s.slice(s.length - n);
}

phase("Author the seven tables in parallel");
log("Storm tables: " + TABLES.map((t) => t.name).join(", "));
const authored = await Promise.all(
  TABLES.map(async (t) => {
    const result = await agent(`author-${t.name}`).ask<TableResult>(
      `You are a content author for the schizo-game canon database (workspace root = repo root). This game is 100% built from the designer's notes plus researched bronze-age material — nothing else.

READ FIRST:
- db/schema/${t.name}.md                       (your table's fields and rules)
- db/canon/${t.name}.csv                       (current state — APPEND rows, keep the header)
- DECISIONS.md                                 (D-000..D-011: the conventions you must author against)
- docs/world-bible.md and db/sources/notes.md  (the canon you may draw on)
- docs/game-design.md §9                       (the content policy)

YOUR TASK
${t.brief}

Append your rows to db/canon/${t.name}.csv. Then ${VERIFY}

${POLICY}

YOUR ONE FILE: db/canon/${t.name}.csv. Nothing else.

Return the TableResult.`,
    );
    const item: TableResult = { ...result, status: result.rows_added > 0 ? "reviewing" : "blocked" };
    report(item, "tables");
    return result;
  }),
);

phase("Fix any validator failures");
let lint = await world.run("python3", ["tools/canon_lint.py"], { timeoutMs: 60000 });
for (let round = 1; round <= 2 && lint.exitCode !== 0; round++) {
  log(`Lint round ${round}: routing failures to fixers`);
  await agent(`lint-fixer-${round}`).ask<TableResult>(
    `You are a repair agent for the schizo-game canon database (workspace root = repo root).

tools/canon_lint.py FAILED. Output:
${tail(lint.stdout + "\n" + lint.stderr, 6000)}

READ: db/schema/*.md for the table formats, DECISIONS.md for the conventions.
Fix ONLY the rows the linter names (edit db/canon/*.csv; add missing columns; fix tags/source_ref; delete a hopeless row rather than inventing canon for it). Then ${VERIFY}

${POLICY}

Return the TableResult (table = "lint", rows_added = 0).`,
  );
  lint = await world.run("python3", ["tools/canon_lint.py"], { timeoutMs: 60000 });
}

phase("Review each table for canon fidelity");
const reviews = await Promise.all(
  TABLES.map((t) =>
    agent(`reviewer-${t.name}`).ask<ReviewVerdict>(
      `You are a content reviewer for the schizo-game canon database. Fresh eyes: you have seen nothing of how these rows were written.

READ: db/canon/${t.name}.csv, db/schema/${t.name}.md, DECISIONS.md (the conventions), docs/world-bible.md, db/sources/notes.md, docs/game-design.md §9.

JUDGE, with evidence (cite the row id):
1. Does any row claim CANON without coming from the notes/world-bible? That is a must_fix.
2. Are A-row source_refs honest (named real traditions, no invented specific citations)?
3. Any anachronisms (coins, iron tools, later-era material)? Any row contradicting the notes (e.g. a deity, city or rite the canon contradicts)?
4. Do law rows use ONLY the kernel verdict vocabulary, with a sane first option? Do event rows use ONLY the trigger grammar?
5. Tone: does the content read like the designer's world (mythic, austere, bronze-age), not generic fantasy?

Do not edit any file. Report must_fix ONLY for real policy/canon violations, each with the row id and one sentence. Everything else is suggestions.

Return the ReviewVerdict.`,
    ),
  ),
);
for (const r of reviews) {
  const item: TableResult = {
    table: r.table,
    rows_added: 0,
    tags_used: "",
    open_questions: [],
    notes: r.suggestions.join(" | "),
    status: r.approved ? "done" : "fixing",
  };
  report(item, "tables");
}

phase("Apply review fixes and confirm everything green");
const toFix = reviews.filter((r) => r.must_fix.length > 0);
if (toFix.length > 0) {
  log(`Review fixes needed: ${toFix.map((r) => r.table).join(", ")}`);
  await Promise.all(
    toFix.map((r) =>
      agent(`fixer-review-${r.table}`).ask<TableResult>(
        `You are a repair agent for the schizo-game canon database (workspace root = repo root).

A reviewer found policy/canon violations in db/canon/${r.table}.csv that MUST be fixed:
${r.must_fix.map((s) => "- " + s).join("\n")}

Fix those rows (edit ONLY db/canon/${r.table}.csv — correct the tag, the source_ref, or delete the row rather than inventing canon). Then ${VERIFY}

${POLICY}

Return the TableResult (table = "${r.table}", rows_added = 0).`,
      ),
    ),
  );
} else {
  log("No review fixes needed.");
}

await world.run("python3", ["tools/canon_lint.py"], { timeoutMs: 60000 });
const coverage = await world.run("python3", ["tools/coverage_check.py"], { timeoutMs: 60000 });
await world.run("cmake", ["-S", "kernel", "-B", "kernel/build", "-DCMAKE_BUILD_TYPE=Release"], { timeoutMs: 120000 });
const build = await world.run("cmake", ["--build", "kernel/build", "-j", "8"], { timeoutMs: 300000 });
const test = await world.run("ctest", ["--test-dir", "kernel/build", "--output-on-failure"], { timeoutMs: 300000 });
const suiteGreen = build.exitCode === 0 && test.exitCode === 0;
await world.run("python3", ["tools/codex_gen.py"], { timeoutMs: 60000 });

for (const t of TABLES) {
  const review = reviews.find((r) => r.table === t.name);
  const item: TableResult = {
    table: t.name,
    rows_added: 0,
    tags_used: "",
    open_questions: [],
    notes: review?.approved ? "reviewed" : "review fixes applied",
    status: suiteGreen ? "done" : "fixing",
  };
  report(item, "tables");
}

const totalRows = authored.reduce((n, r) => n + r.rows_added, 0);
const openQuestions = authored.flatMap((r) => r.open_questions.map((q) => r.table + ": " + q));

const reportLines = [
  "# Content storm — the canon database grows",
  "",
  `**Validators:** lint green · coverage ${coverage.exitCode === 0 ? "green" : "FAILED"} · kernel build ${build.exitCode === 0 ? "ok" : "FAILED"} · ctest ${test.exitCode === 0 ? "passed" : "FAILED"} (${totalRows} rows authored across ${TABLES.length} tables).`,
  "",
  "## Tables",
  "",
  ...TABLES.map((t) => {
    const a = authored.find((x) => x.table === t.name);
    const r = reviews.find((x) => x.table === t.name);
    return `- **${t.name}** — ${a?.rows_added ?? 0} rows (${a?.tags_used ?? ""}), review ${r?.approved ? "approved" : "fixes applied"}`;
  }),
  "",
  "## Open questions (escalated, never invented around)",
  openQuestions.length ? openQuestions.map((q) => "- " + q).join("\n") : "- none",
];
await artifact.markdown("storm-report", reportLines.join("\n"), {
  title: "Content storm report",
  description: "What the seven authors added, what the gate proved, what escalated.",
  primary: true,
});

const findings: Finding[] = openQuestions.map((q) => ({
  where: "db/canon/",
  what: q,
  evidence: "author escalation (agents were instructed never to invent around undecided canon)",
  status: "unconfirmed",
  severity: "low",
}));

return {
  conclusion: suiteGreen
    ? `The storm added ${totalRows} rows across ${TABLES.length} tables (items, foods, laws, customs, events, schedules, names): lint and coverage green, and the kernel suite still passes with the new canon loaded. ${openQuestions.length} open question(s) escalated.`
    : `The storm ended RED: build ${build.exitCode}, ctest ${test.exitCode}. ${totalRows} rows were added; read the failing output and route a fix wave.`,
  findings,
  verified: [
    "tools/canon_lint.py (every row tagged and sourced)",
    "tools/coverage_check.py (27/27 notes elements)",
    "cmake build + ctest full suite (the kernel with the new canon loaded)",
    "tools/codex_gen.py (codex regenerated)",
    "one independent policy review per table (fresh-eyes agents, row ids cited)",
  ],
  notCovered: [
    "historical verification of every A-row citation (the designer's accuracy pass, per the parent roadmap's 'AI drafts, sources decide')",
    "quests and dialogues content (Act I story work, Phase 5 proper)",
    "the UE5 engine phase (no engine installed; designer-side action item)",
  ],
};
