#!/bin/bash
set -euo pipefail

pinned_revision=94673be33d7b4938c59acd39314d3a6e02674724
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd "$script_dir/.." && pwd)
baseline="$script_dir/conformance-baseline.json"
check_baseline=false

if [[ ${1:-} == "--check-baseline" ]]; then
	check_baseline=true
	shift
fi

checkout=${HAXE_CHECKOUT:-${1:-}}
if [[ -z "$checkout" || ! -d "$checkout/std" ]]; then
	echo "Set HAXE_CHECKOUT or pass a Haxe checkout path." >&2
	exit 2
fi
checkout=$(cd "$checkout" && pwd)

actual_revision=$(git -C "$checkout" rev-parse HEAD)
if [[ "$actual_revision" != "$pinned_revision" ]]; then
	echo "Expected Haxe revision $pinned_revision, got $actual_revision." >&2
	exit 2
fi

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/tree-sitter-haxe-conformance.XXXXXX")
trap 'rm -rf "$temp_dir"' EXIT

measure() {
	local name=$1
	local root=$2
	local paths_file="$temp_dir/$name.paths"
	local output_file="$temp_dir/$name.out"

	find "$root" -type f -name '*.hx' ! -path '*/.git/*' -print | sort >"$paths_file"
	(
		cd "$project_dir"
		npx tree-sitter parse --paths "$paths_file" --stat >"$output_file" 2>&1 || true
	)

	local summary total clean failed percent errors missing
	summary=$(grep 'Total parses:' "$output_file" | tail -n 1)
	total=$(sed -E 's/.*Total parses: ([0-9]+).*/\1/' <<<"$summary")
	clean=$(sed -E 's/.*successful parses: ([0-9]+).*/\1/' <<<"$summary")
	failed=$(sed -E 's/.*failed parses: ([0-9]+).*/\1/' <<<"$summary")
	percent=$(sed -E 's/.*success percentage: ([0-9.]+)%.*/\1/' <<<"$summary")
	# grep exits 1 with no matches; guard it so a clean (zero-error) run records
	# 0 instead of aborting under `set -o pipefail`.
	errors=$({ grep -o '(ERROR' "$output_file" || true; } | wc -l)
	missing=$({ grep -o '(MISSING' "$output_file" || true; } | wc -l)

	printf '{"totalFiles":%s,"cleanFiles":%s,"failedFiles":%s,"successPercent":%s,"errorNodes":%s,"missingNodes":%s}' \
		"$total" "$clean" "$failed" "$percent" "$errors" "$missing"
}

std_result=$(measure std "$checkout/std")
repository_result=$(measure repository "$checkout")
result=$(printf '{"haxeRevision":"%s","note":"Full-repository metrics include intentionally malformed and display-marker fixtures.","std":%s,"repository":%s}' \
	"$pinned_revision" "$std_result" "$repository_result")

node -e 'console.log(JSON.stringify(JSON.parse(process.argv[1]), null, 2))' "$result"

if [[ "$check_baseline" == true ]]; then
	node - "$baseline" "$result" <<'NODE'
const fs = require("node:fs");
const baseline = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const result = JSON.parse(process.argv[3]);
if (
  result.std.cleanFiles < baseline.std.cleanFiles ||
  result.std.errorNodes > baseline.std.errorNodes ||
  result.std.missingNodes > baseline.std.missingNodes
) {
  console.error("Standard-library conformance regressed against the recorded baseline.");
  process.exit(1);
}
NODE
fi
