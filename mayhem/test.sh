#!/usr/bin/env bash
# mayhem/test.sh — run upstream's interpreted integration tests (ctest label `interpreted`) with the
# Release souffle CLI that build.sh built in build-test/. Each test runs souffle on a .dl program and
# diffs its output relations and stdout/stderr against the committed expected files.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

if [ ! -x build-test/src/souffle ]; then
  echo "build-test/src/souffle missing: mayhem/build.sh did not build the test CLI" >&2
  emit_ctrf cmake-ctest 0 1
  exit 1
fi

# Integration tests of the interpreter, on the evaluation/semantic/syntactic/example suites.
# Excluded: `interface` (each test compiles a C++ driver against the generated program), provenance
# and profile, and the tests that need libffi user-defined functors (issue2508) or sqlite/zlib IO:
# those libraries are built off.
LABEL_EXCLUDE='provenance|profile|interface'
NAME_EXCLUDE='issue2508|functor|sqlite|gzip|zlib|libfoo|ffi|swig'
log=$(mktemp)
(cd build-test && ctest -L interpreted -LE "$LABEL_EXCLUDE" -E "$NAME_EXCLUDE" -j"$MAYHEM_JOBS" --timeout 300 --output-on-failure) >"$log" 2>&1
tail -40 "$log"
total=$(grep -oE 'tests passed, [0-9]+ tests? failed out of [0-9]+' "$log" | grep -oE '[0-9]+$' | tail -1)
failed=$(grep -oE '[0-9]+ tests? failed out of' "$log" | grep -oE '^[0-9]+' | tail -1)
rm -f "$log"
if [ -z "$total" ] || [ "$total" -eq 0 ]; then
  echo "ctest ran no tests" >&2
  emit_ctrf cmake-ctest 0 1
  exit 1
fi
emit_ctrf cmake-ctest $(( total - failed )) "$failed"
