#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/goc-feature-tests.XXXXXX")"
trap 'rm -rf "$OUT_DIR"' EXIT

compiler="$ROOT/goc/goc"
vm="$ROOT/goc/vm"

if [[ ! -x "$compiler" || ! -x "$vm" ]]; then
    cmake -S "$ROOT/goc" -B "$ROOT/goc/build"
    cmake --build "$ROOT/goc/build"
    compiler="$ROOT/goc/build/goc"
    vm="$ROOT/goc/build/vm"
fi

tests=(
    "arithmetic:28"
    "functions:50"
    "arrays:17"
    "pointers:42"
    "heap_pointers:26"
    "loops:16"
    "floats:3.25"
    "recursion:120"
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
