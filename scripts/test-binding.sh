#!/bin/bash
# Run the Node binding smoke test as part of the default `npm test`.
#
# The native addon is produced by `node-gyp-build` during install. We skip ONLY
# when no build exists for the current runtime, and otherwise run the real smoke
# test so a present-but-broken binding (bad ABI, broken JS wrapper, missing
# export) fails loudly. We deliberately do not treat "require failed" as "not
# built", because that would mask exactly those breakages.
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd "$script_dir/.." && pwd)
cd "$project_dir"

# `node-gyp-build.path()` only resolves the compiled `.node` file path (it does
# not dlopen the addon), throwing when no build matches this platform/ABI. The
# sentinels distinguish three outcomes:
#   PRESENT   - a build exists -> run the smoke test (do not mask failures)
#   NO_BUILD  - no build for this runtime -> skip gracefully
#   <empty>   - node could not even run the probe -> environment is broken, fail
probe=$(node -e 'try { require("node-gyp-build").path("."); process.stdout.write("PRESENT"); } catch (e) { process.stdout.write("NO_BUILD"); }' 2>/dev/null) || true

case "$probe" in
	PRESENT)
		exec node --test bindings/node/*_test.cjs
		;;
	NO_BUILD)
		echo "Skipping binding smoke test: no native build for this runtime." >&2
		exit 0
		;;
	*)
		echo "Binding probe failed to run (broken Node environment)." >&2
		exit 1
		;;
esac
