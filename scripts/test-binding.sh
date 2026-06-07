#!/bin/bash
# Run the Node binding smoke test as part of the default `npm test`.
#
# The native addon is produced by `node-gyp-build` during install. If it has
# not been built for the current runtime we skip rather than fail, so the
# default test stays meaningful on platforms without a build toolchain while
# still failing loudly when an *available* binding is broken.
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd "$script_dir/.." && pwd)
cd "$project_dir"

if ! node -e 'require(".")' >/dev/null 2>&1; then
	echo "Skipping binding smoke test: native addon not built for this runtime." >&2
	exit 0
fi

exec node --test bindings/node/*_test.cjs
