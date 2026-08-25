#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/goc-feature-tests.XXXXXX")"
trap 'rm -rf "$OUT_DIR"' EXIT

if [[ -n "${GOC_COMPILER:-}" && -n "${GOC_VM:-}" ]]; then
    compiler="$GOC_COMPILER"
    vm="$GOC_VM"
else
    build_dir="$OUT_DIR/build"
    cmake -S "$ROOT/goc" -B "$build_dir"
    cmake --build "$build_dir"
    compiler="$build_dir/goc"
    vm="$build_dir/vm"
fi

tests=(
    "arithmetic:28"
    "functions:50"
    "arrays:17"
    "pointers:42"
    "heap_pointers:26"
    "loops:16"
    "floats:3.25"
    "float_comparisons:101"
    "recursion:120"
    "operators:111712"
    "globals_scopes:727"
    "initializers:123"
    "overloads:12"
    "heap_reuse:729"
    $'literals:1A\n10'
)

for item in "${tests[@]}"; do
    name="${item%%:*}"
    expected="${item#*:}"
    source="$ROOT/tests/feature/$name.cpp"
    bytecode="$OUT_DIR/$name.bin"

    "$compiler" "$source" -o "$bytecode" -q
    actual="$("$vm" "$bytecode")"

    if [[ "$actual" != "$expected" ]]; then
        printf 'FAIL %-14s expected <%s> got <%s>\n' "$name" "$expected" "$actual" >&2
        exit 1
    fi

    printf 'PASS %-14s %s\n' "$name" "$actual"
done

negative_tests=(
    "missing_function"
    "missing_main"
    "scope_leak"
)

for name in "${negative_tests[@]}"; do
    source="$ROOT/tests/negative/$name.cpp"
    bytecode="$OUT_DIR/$name.bin"
    diagnostics="$OUT_DIR/$name.log"

    if "$compiler" "$source" -o "$bytecode" -q >"$diagnostics" 2>&1; then
        printf 'FAIL %-14s expected compilation failure\n' "$name" >&2
        exit 1
    fi

    printf 'PASS %-14s rejected invalid program\n' "$name"
done

runtime_error_tests=(
    "use_after_free"
)

for name in "${runtime_error_tests[@]}"; do
    source="$ROOT/tests/runtime_errors/$name.cpp"
    bytecode="$OUT_DIR/$name.bin"
    diagnostics="$OUT_DIR/$name.log"

    "$compiler" "$source" -o "$bytecode" -q
    if "$vm" "$bytecode" >"$diagnostics" 2>&1; then
        printf 'FAIL %-14s expected VM failure\n' "$name" >&2
        exit 1
    fi

    printf 'PASS %-14s rejected invalid runtime access\n' "$name"
done
