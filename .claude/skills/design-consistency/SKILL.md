---
name: design-consistency
description: Keep Outpost Commander's design documents agreeing with each other, and put each decision where the next person will find it. Use this skill whenever anything under Design/ is written or changed — a figure moves, an ADR is authored or amended, an open question is answered, a milestone plan gains a step, a rule is added to AGENTS.md — and whenever a change to code or the wire format moves a number the documents quote. Use it before handing back any pull request that touches Design/ or AGENTS.md, and whenever someone asks "is the documentation still consistent", "where should this decision go", "should this be an ADR", "is this an open question" or "did I update everything". Running `Scripts/CheckDesign.py` is a precondition, not advice: a figure here is stated in three places by design, each reads as authoritative, and when one moves and the others do not there is nothing in the text to tell them apart.
---

# Design consistency

A figure in this tree is deliberately stated more than once — in `TechnicalDesign.md`, in the ADR that
ruled it, and in the `OpenQuestions.md` row that asked. That redundancy is a feature: each document
answers a different question and none is a stub pointing at another. **The cost is that a figure which
moves in one place and not the others leaves two copies that both read as true.**

That is not hypothetical here. ADR-003's budget table has diverged from `TechnicalDesign.md` §4 before;
a trim of `AGENTS.md` once deleted a constraint ADR-007 cites; the entity count moved from 102 to 110
and left stale copies behind. Each was found by a sweep and none by reading.

## Run the checker. Before handing anything back

```bash
python3 Scripts/CheckDesign.py
```

It normalises whitespace before matching, because a figure wraps across a line break and a naive grep
for `96 bytes` misses `96\nbytes` — an earlier sweep let exactly that through. It checks four things:

- **The datagram figures, recomputed** by `Scripts/DatagramBudget.py` rather than restated, so the checker cannot itself go stale against the numbers it is policing.
- **Figures that have drifted before**, from a small manifest, in both directions: the superseded value
  must be gone, and the current one must actually appear where it is expected.
- **Citations**: every `ADR-NNN` exists or is declared reserved in `Design/ADR/README.md`; every `Q<n>`
  is on the register; every ADR is reachable from its README.
- **Shape**: links resolve, and no table row has lost a column.

## Adding a manifest row: make the pattern claim-shaped

This is the one thing that will be got wrong, so it is worth the paragraph. **This tree records why a
figure changed**, so the superseded value legitimately appears in good prose — "tap-to-visible is 152 ms
average where 10 Hz made it 252 ms" is correct and a bare `/10 Hz/` flags it. Match the sentence that
*asserts* the old value — `snapshots go out at 10 Hz` — never the number alone.

A checker that cries wolf on correct prose gets switched off, and then it catches nothing at all. When
in doubt, leave the row out: a missing check costs one defect, a noisy one costs the whole tool.

## Where a decision goes

Four places, and the difference is not filing preference — it is who has to be able to settle an
argument with it later.

**An ADR**, under `Design/ADR/` — a decision *taken while building*: a wire format, a subsystem's shape,
a number that could reasonably have been another number, an exception to a rule. It carries status, date
and owner, and it states context, decision, consequences and the measurements it owes. **Write it in the
same commit as the change it justifies.** The test: will someone six months from now ask *why is it this
way* — and would re-deriving the answer take longer than reading it?

**A rule in `AGENTS.md` §5** — only when a design decision has to **constrain the shape of code**. R25
and up are reserved deliberately: a rule with no source behind it is a rule nobody can settle an
argument with, so a new rule cites the ADR or design section that is its source, the way R22 to R24 do.
If you cannot name that source, what you have is an ADR, not a rule.

**A row on `Design/OpenQuestions.md`** — a question the design has not answered, recorded **before the
code that needs it is written**, not after. A question discovered while implementing is still an open
question; the register is what stops it being settled silently by whoever hit it first.

**Nowhere** — an implementation detail with no alternative worth recording. This is the right answer
more often than the other three, and a tree that files everything is as unreadable as one that files
nothing.

Two rules cut across all four. **Figures are measured, not estimated** — if you quote one, say how you
measured it, and say plainly when it is arithmetic on the design's own numbers rather than an
observation. And **a decision nobody wrote down gets re-litigated every few months** by whoever forgot
it, which is the whole reason any of this exists.

## What the checker cannot see

- **A figure that is wrong everywhere.** Consistency is not correctness; the sweep proves the copies
  agree, never that they are right. That is what the measurements each ADR owes are for.
- **A decision that was taken and not recorded at all.** Nothing can detect the absence of a document
  nobody wrote. This is the most common failure and the only defence is the four-way above.
- **Prose that contradicts itself in words rather than numbers** — two paragraphs that disagree about
  what a subsystem does, with no figure between them. Read the diff.
- **A citation that resolves but is wrong** — a link to ADR-007 where ADR-008 was meant. The target
  exists, so the checker is happy.

## What to hand back

```
Checker:      CheckDesign.py, N issues   ← if this line is empty, the work is not done
Figures moved: <each one, its old and new value, and every document that states it>
Decisions:    <each, and which of the four places it went, and why not the others>
Measured:     <any figure quoted, and how it was obtained -- computed, observed, or arithmetic>
Still open:   <anything that became a question, and its row on the register>
```

If a figure moved, say which documents you changed **and** that the checker is clean — those are two
different claims, and this is the skill that exists because the second one kept not following from the
first.
