#!/bin/bash
# Run the Node binding smoke test as part of the default `npm test`.
#
# The native addon is produced by `node-gyp-build` during install. We skip ONLY
# when node-gyp-build itself loads fine and reports that no build exists for the
# current runtime. Any other failure (missing/broken node-gyp-build, an
# unexpected resolver error, a broken Node environment, or a present-but-broken
# addon) fails loudly, so genuine breakage is never masked as "not built".
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd "$script_dir/.." && pwd)
cd "$project_dir"

# `node-gyp-build.path()` only resolves the compiled `.node` file path (it does
# not dlopen the addon); it throws `No native build was found ...` when nothing
# matches this platform/ABI. The probe emits one sentinel:
#   PRESENT       - a build exists -> run the smoke test (do not mask failures)
#   NO_BUILD      - node-gyp-build loaded, but no build for this runtime -> skip
#   DEP_ERROR:... - node-gyp-build itself failed to load/resolve -> fail
#   PROBE_ERROR:..- unexpected resolver error -> fail
#   <empty>       - node could not even run the probe -> fail
probe=$(node -e '
  let ngb;
  try { ngb = require("node-gyp-build"); }
  catch (e) { process.stdout.write("DEP_ERROR:" + e.message); process.exit(0); }
  try { ngb.path("."); process.stdout.write("PRESENT"); }
  catch (e) {
    if (/No native build was found/.test(e.message)) process.stdout.write("NO_BUILD");
    else process.stdout.write("PROBE_ERROR:" + e.message);
  }
' 2>/dev/null) || true

case "$probe" in
	PRESENT)
		exec node --test bindings/node/*_test.cjs
		;;
	NO_BUILD)
		echo "Skipping binding smoke test: no native build for this runtime." >&2
		exit 0
		;;
	"")
		echo "Binding probe failed to run (broken Node environment)." >&2
		exit 1
		;;
	*)
		echo "Binding probe error: $probe" >&2
		exit 1
		;;
esac
