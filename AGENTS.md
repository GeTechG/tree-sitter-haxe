# Agent guide

Rules for AI agents and humans working in this grammar. `README.md` is the user-facing entry point.

## Checks

- `npm test` — corpus tests, the example files and the Node binding.
- `HAXE_CHECKOUT=<haxe checkout> npm run test-conformance-baseline` — the share of the Haxe standard library that parses cleanly must not drop below `scripts/conformance-baseline.json`. Raise the baseline only for an intentional improvement, in the same commit.

`src/` is generated from `grammar.js` by `npm run generate`: never edit it by hand, regenerate and commit it with the grammar change.

## Test-first

A grammar change starts with a failing test, failing for the right reason (a wrong or `ERROR` tree, not a broken harness):

1. Add the smallest source that shows the behaviour to `test/corpus/` with the tree it must produce, and see `npx tree-sitter test` fail on it.
2. Change `grammar.js`, regenerate, get it green.
3. Refactor with the whole suite green.

Never weaken or delete an existing corpus test, an example or a baseline to get green. Docs, config and other mechanical edits need no test.

## Review

Unless the change is trivial (mechanical, or a few obvious lines), finish with a cross-review of the whole branch diff before it lands: `/ai-brainstorm:ai-review`, a judge from another model family. Fix or rebut its findings and run it again until it comes back clean.
