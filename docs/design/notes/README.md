# Owner design notes

This is an inbox for design notes the owner gives during sessions, **recorded close to verbatim**.
Each note is dated, and links to the design documents that act on it.

- Notes are never rewritten.
- If a later decision changes a note's direction, the note links to that decision.

| # | Date | Topic | Acted on in |
|---|---|---|---|
| [N001](N001-claude-playtesting.md) | 2026-09-27 | Claude must be able to play and test the game during development | [playtest-harness.md](../playtest-harness.md), `SYSTEMS.md` R15, walkthrough G13 |
| [N002](N002-intended-use-tests.md) | 2026-09-27 | Track intended uses of gameplay functions; keep purpose tests permanently; ask when unsure | [purposes.md](../purposes.md), playtest-harness §9, R15 |
| [N003](N003-permadeath-and-wizard-mode.md) | 2026-09-27 | Hard permadeath; getting stuck or dying to bad luck is fine; wizard mode ships | G04 D4.4, playtest-harness D-PT1/D-PT2, purposes P-CH-04/05, P-TL-01 |
| [N004](N004-realism-spirit-and-purpose-answers.md) | 2026-09-27 | **The game's spirit:** super-realistic zombie survival, real-world populations. Answers to the six purpose questions. | purposes.md, scent-mobs.md "round 2" |
| [N005](N005-the-event-and-aftermath-stages.md) | 2026-09-27 | The instant demonic turning, 1 in 100 not turned, **3 stages (1 week / 1 month / 1 year) as difficulty**, no NPCs, aftermath in all world generation | ADR-0009, world-generation.md "Aftermath model", purposes P-WO-08–10, G04 |
| [N006](N006-rivers-simplified.md) | 2026-09-27 | Rivers: no drainage simulation. A long stream between far points, generated at finer detail, with spacing. | ADR-0010, world-generation.md "Rivers (N006)" |
| [N007](N007-event-parameters-clock-utilities-fires.md) | 2026-09-27 | Per-world event (moment, 80–99% turned), survivor traces and encampments, zombie decay and limb damage, **global clock gradient**, utilities, fires | ADR-0011, world-generation.md N007 section, purposes P-WO-11–17, P-EN-09 |
| [N008](N008-ai-authored-content-and-properties.md) | 2026-09-27 | AI-authored content: archetypes, detail budget, environment-driven mobs and places, generic properties and tags (handle/socket attach, ifImpacted) | content-model.md, purposes P-IT-01–06, walkthrough G14 |
| [N009](N009-revisit-aging-gate.md) | 2026-09-27 | Visited places stay unchanged unless you've been away about 1 month | ADR-0011 item 4, purposes P-WO-18 |
| [N010](N010-name-attachment-joints-descriptions-brands.md) | 2026-09-27 | Name **"Post Event: Onwards"**; attachment surface and strength; joint costs and methods; prose descriptions; fictional brands with name composition | content-model.md §6–9, brands.md, purposes P-IT-07–10 |
