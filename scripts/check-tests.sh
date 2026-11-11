#!/bin/sh
# Configure a fresh build directory and run the unit test suite.
# Usage: scripts/check-tests.sh [build-dir]
#
# The build directory (default: build-tests in the source tree) is
# deleted and recreated. Unless Criterion is installed, the first
# configure fetches it as a meson subproject from github.com.

srcdir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
builddir="${1:-$srcdir/build-tests}"

case "$builddir" in
  /*) ;;
  *) builddir="$srcdir/$builddir" ;;
esac

rm -rf "$builddir" || exit 1
meson setup "$builddir" "$srcdir" \
      -Dbuildtype=debugoptimized -Dtests=enabled \
      -Dclients=[] -Dfcmp=[] -Dtools=[] -Daudio=none -Dnls=false \
  || exit 1
ninja -C "$builddir" || exit 1
meson test -C "$builddir" --print-errorlogs
