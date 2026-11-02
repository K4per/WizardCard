# Domain Docs

## Layout

This repo uses a single context:

- `GLOSSARY.md` at the repo root defines domain terms.
- `docs/adr/` holds architecture decision records.

## Before exploring

Read `GLOSSARY.md` and any ADRs relevant to the area being explored.
If either is absent, proceed silently. The `domain-modeling` skill creates these documents lazily when terms or decisions are resolved.

## Use the glossary's vocabulary

When naming a domain concept in an issue, proposal, hypothesis, or test, use the term defined in `GLOSSARY.md`.
If a needed concept is missing, reconsider whether it belongs to the project or note the gap for `domain-modeling`.

## Flag ADR conflicts

If a proposal contradicts an existing ADR, identify the record and explain why it should be reconsidered before overriding the decision.
