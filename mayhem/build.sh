#!/usr/bin/env bash
#
# mayhem/build.sh — build the Guitar fuzz harnesses (sanitized), their standalone reproducers, and the
# known-answer self-test that mayhem/test.sh runs (normal flags). Offline and re-runnable: the only
# external inputs are the Qt6/zlib dev packages baked into the image by mayhem/Dockerfile.
#
# Guitar is a Qt GUI application whose qmake build links one monolithic executable, so instead of
# building the app we compile just the non-GUI library sources the harnesses reach (git object/pack/log
# parsers, gzip/zip readers, the JSON stream parser and their Qt-free helpers) into one object set.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
rm -rf "$B"
mkdir -p "$B/fuzz" "$B/normal"

QT_CFLAGS="$(pkg-config --cflags Qt6Core Qt6Gui Qt6Network Qt6Widgets)"
INCLUDES="-I. -Isrc -Isrc/common -Isrc/texteditor -Isrc/process/src -Isrc/localvault/src -include pragma.h"
DEFS="-DUSE_QT -DAPP_GUITAR"
LIBS="$(pkg-config --libs Qt6Core Qt6Gui Qt6Network Qt6Widgets) -lz"

LIB_SOURCES="
src/AbstractGitSession.cpp src/Git.cpp src/GitBasicSession.cpp src/GitCommandCache.cpp src/GitDiff.cpp
src/GitDiffManager.cpp src/GitObjectManager.cpp src/GitPack.cpp src/GitPackIdxV2.cpp src/GitRunner.cpp
src/GitTypes.cpp src/Logger.cpp src/MemoryReader.cpp src/Profile.cpp src/TraceLogger.cpp src/gzip.cpp
src/common/AbstractSimpleIO.cpp src/common/misc.cpp src/common/qmisc.cpp src/common/realpath.cpp
src/common/unicode_conversion.cpp src/common/q/DateTime.cpp src/common/q/Dir.cpp src/common/q/FileInfo.cpp
src/process/src/AbstractProcess.cpp src/process/src/BasicProcessPosix.cpp src/process/src/ProcessHelper.cpp
src/zip/zipextract.cpp
"

# compile_set <outdir> <flags...>: compile every library source (plus the env stubs) in parallel.
compile_set() {
  local out="$1"; shift
  printf '%s\n' $LIB_SOURCES mayhem/harnesses/env_stubs.cpp | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
    f="$1"; out="$2"; shift 2
    "$CXX" -std=c++20 -fPIC "$@" -c "$f" -o "$out/$(echo "$f" | tr / _).o"' _ {} "$out" \
    $DEFS $QT_CFLAGS $INCLUDES "$@"
  rm -f "$out/libguitar.a"
  ar rcs "$out/libguitar.a" "$out"/*.o
}

# 1) Sanitized + DWARF<4 library, once.
compile_set "$B/fuzz" $SANITIZER_FLAGS $DEBUG_FLAGS
# 2) Normal-flag library for the self-test (kept separate so test.sh stays an honest oracle).
compile_set "$B/normal" -O1 $COVERAGE_FLAGS

# Harnesses reach private parsers (Git::parseCommitItem, Git::parseAheadBehind), hence -fno-access-control
# on the harness/self-test translation units only.
HFLAGS="-std=c++20 -fPIC -fno-access-control $DEFS $QT_CFLAGS $INCLUDES"

"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/fuzz/lsan_off.o"
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "${STANDALONE_FUZZ_MAIN:-/opt/mayhem/StandaloneFuzzTargetMain.c}" -o "$B/fuzz/standalone_main.o"

for t in fuzz_zip fuzz_gzip fuzz_gitparse fuzz_gitpack fuzz_jstream; do
  "$CXX" $HFLAGS $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "mayhem/harnesses/$t.cpp" \
    "$B/fuzz/lsan_off.o" "$B/fuzz/libguitar.a" $LIBS -o "$SRC/$t"
  "$CXX" $HFLAGS $SANITIZER_FLAGS $DEBUG_FLAGS "mayhem/harnesses/$t.cpp" "$B/fuzz/standalone_main.o" \
    "$B/fuzz/lsan_off.o" "$B/fuzz/libguitar.a" $LIBS -o "$SRC/$t-standalone"
done

# 3) Known-answer self-test (normal flags, no sanitizers).
"$CXX" $HFLAGS -O1 $COVERAGE_FLAGS mayhem/selftest.cpp "$B/normal/libguitar.a" $LIBS -o "$B/guitar_selftest"
