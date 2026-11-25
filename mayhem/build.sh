#!/usr/bin/env bash
# mayhem/build.sh — build souffle's fuzz harnesses (sanitized libsouffle) and the test build.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

# Optional libraries (libffi functors, sqlite IO, zlib, ncurses explain UI, OpenMP) are off: the
# fuzzed surfaces are souffle's own parser/compiler/interpreter/fact readers. SOUFFLE_GIT=OFF: the
# version comes from PACKAGE_VERSION instead of `git describe` (tags may be absent in a checkout).
COMMON_OPTS=(-DSOUFFLE_GIT=OFF -DPACKAGE_VERSION=2.5 -DSOUFFLE_USE_CURSES=OFF -DSOUFFLE_USE_SQLITE=OFF
  -DSOUFFLE_USE_ZLIB=OFF -DSOUFFLE_USE_OPENMP=OFF -DSOUFFLE_USE_LIBFFI=OFF -DSOUFFLE_USE_LIBCPP=OFF
  -DSOUFFLE_BASH_COMPLETION=OFF)

# 1) Sanitized libsouffle for the harnesses (build dirs match upstream's gitignored `*build*/`).
cmake -S . -B build-fuzz "${COMMON_OPTS[@]}" -DSOUFFLE_ENABLE_TESTING=OFF -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1" -DCMAKE_CXX_FLAGS_DEBUG=""
cmake --build build-fuzz --target libsouffle -j"$MAYHEM_JOBS"

# 2) Harnesses: libFuzzer binary + standalone reproducer each.
H="$SRC/mayhem/harnesses"
INC=(-I"$SRC/src" -I"$SRC/src/include" -I"$SRC/build-fuzz/src")
LIB="$SRC/build-fuzz/src/libsouffle.a"
CXXF=(-std=c++17 -fwrapv $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1)
mkdir -p "$SRC/build-fuzz/mayhem"
O="$SRC/build-fuzz/mayhem"
$CXX "${CXXF[@]}" -c "$SRC/mayhem/lsan_off.cc" -o "$O/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$O/standalone_main.o"
build_one() {  # name source [extra flags...]
  local name="$1" src="$2"; shift 2
  $CXX "${CXXF[@]}" "${INC[@]}" "$@" -c "$src" -o "$O/$name.o"
  $CXX "${CXXF[@]}" $LIB_FUZZING_ENGINE "$O/$name.o" "$O/lsan_off.o" "$LIB" -lpthread -ldl -o "/mayhem/$name"
  $CXX "${CXXF[@]}" "$O/$name.o" "$O/standalone_main.o" "$O/lsan_off.o" "$LIB" -lpthread -ldl \
    -o "/mayhem/$name-standalone"
}
build_one fuzz_parser "$H/fuzz_parser.cpp"
build_one fuzz_facts_csv "$H/fuzz_facts_csv.cpp"
build_one fuzz_facts_json "$H/fuzz_facts_json.cpp"

# 2b) fuzz_cli: the real souffle CLI (upstream's src/souffle.cpp main) linked against the same sanitized
# libsouffle, run as a file-input target. It covers what the in-process harnesses can't: semantic
# checks, AST/RAM transforms and the interpreter, which exit() on errors (Transformer::apply ->
# ErrorReport::exitIfErrors()), a normal process exit for a file-input target.
$CXX "${CXXF[@]}" "${INC[@]}" -c "$SRC/src/souffle.cpp" -o "$O/souffle_cli.o"
$CXX "${CXXF[@]}" "$O/souffle_cli.o" "$O/lsan_off.o" "$LIB" -lpthread -ldl -o /mayhem/fuzz_cli

# 3) Test build: upstream's normal Release build of the souffle CLI, run by test.sh through ctest.
cmake -S . -B build-test "${COMMON_OPTS[@]}" -DSOUFFLE_ENABLE_TESTING=ON -DSOUFFLE_TEST_EVALUATION=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-test --target souffle -j"$MAYHEM_JOBS"
