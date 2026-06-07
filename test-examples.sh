#!/bin/bash
set -e
out=$(mktemp "${TMPDIR:-/tmp}/tree-sitter-haxe-examples.XXXXXX")
trap 'rm -f "$out"' EXIT

echo "Parsing all example .hx files..."
find examples -type f -name "*.hx" ! -name "_*.hx" | while read -r file; do
	echo "::group::Parsing $file"
	if ! npx tree-sitter parse "$file" >"$out" 2>&1; then
		echo "Failed to parse $file"
		cat "$out"
		exit 1
	fi
	echo "Parsed successfully: $file"
	echo "::endgroup::"
done
