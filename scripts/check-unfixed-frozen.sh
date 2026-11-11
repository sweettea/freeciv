#!/bin/sh
# Verify that ai/unfixed, the vendored copy of the stock AI, is unmodified.
#
# Usage: scripts/check-unfixed-frozen.sh [BUILDDIR]
#
# ai/unfixed is the control a fixed ai/default is measured against (see
# ai/unfixed/README). It must never be edited, and this check must never
# be "fixed" by regenerating the manifest: a mismatch means the copy was
# changed and the change must be reverted.
#
# Checks, in order:
#  1. every file listed in ai/unfixed/MANIFEST.sha256 has its recorded
#     hash, and no unlisted source file has appeared beside them;
#  2. when git and the vendoring commit are available, each copied file's
#     recorded hash is that of its original at the vendoring commit, so
#     the manifest itself is a faithful record;
#  3. with BUILDDIR (a meson build dir with -Daimodules=...,unfixed), the
#     built module defines every symbol unfixed_syms.h renames under its
#     unf_ name, and references no symbol the live ai/default or
#     ai/classic defines, so no call made by the copy can resolve to the
#     live module. The live symbols are read from the build, so this
#     still holds once ai/default has moved on from the copy.

VENDOR_COMMIT=9d0461dad023f6d22768275a0323a0ae20cf33d7

srcdir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
manifest=ai/unfixed/MANIFEST.sha256
status=0

cd "$srcdir" || exit 1

# 1. The copy matches the manifest.
if ! sha256sum --quiet --strict -c "$manifest"; then
  echo "ERROR: ai/unfixed differs from its manifest." >&2
  status=1
fi
for f in $(find ai/unfixed -type f ! -name MANIFEST.sha256 ! -name README \
             | LC_ALL=C sort); do
  if ! grep -q "  $f\$" "$manifest"; then
    echo "ERROR: $f is not in $manifest." >&2
    status=1
  fi
done

# 2. The manifest records the originals. Copied files are those under
#    ai/unfixed/default and ai/unfixed/classic; their originals are the
#    same path with ai/unfixed/ removed.
if git -c core.fsmonitor=false rev-parse -q --verify \
     "$VENDOR_COMMIT^{commit}" > /dev/null 2>&1; then
  grep -E '  ai/unfixed/(default|classic)/' "$manifest" |
  while read -r sum f; do
    orig="ai/${f#ai/unfixed/}"
    osum=$(git -c core.fsmonitor=false show "$VENDOR_COMMIT:$orig" \
             | sha256sum | cut -d' ' -f1)
    if [ "$sum" != "$osum" ]; then
      echo "ERROR: $f is not $orig at $VENDOR_COMMIT." >&2
      exit 1
    fi
  done || status=1
else
  echo "Note: $VENDOR_COMMIT not available; originals not compared." >&2
fi

# 3. The built module links only to itself.
if [ -n "$1" ]; then
  stock="$1/libfc_ai.a"
  unf="$1/libfc_ai_unfixed.a"
  if [ ! -f "$stock" ] || [ ! -f "$unf" ]; then
    echo "ERROR: $1 lacks libfc_ai.a or libfc_ai_unfixed.a." >&2
    exit 1
  fi
  tmp=$(mktemp -d) || exit 1
  trap 'rm -rf "$tmp"' EXIT
  nm -A --defined-only -g "$stock" \
    | grep -E '(ai_default_dai[a-z]*|ai_classic_classicai)\.c\.o' \
    | awk '{ print $NF }' | LC_ALL=C sort -u > "$tmp/stock"
  nm -u "$unf" | awk '{ print $NF }' | LC_ALL=C sort -u > "$tmp/undef"
  nm --defined-only -g "$unf" | awk 'NF == 3 { print $NF }' \
    | LC_ALL=C sort -u > "$tmp/def"
  leaks=$(LC_ALL=C comm -12 "$tmp/stock" "$tmp/undef")
  sed -n 's/^#define \([A-Za-z0-9_]*\) unf_\1$/unf_\1/p' \
    ai/unfixed/unfixed_syms.h | LC_ALL=C sort -u > "$tmp/renamed"
  missing=$(LC_ALL=C comm -23 "$tmp/renamed" "$tmp/def")
  if [ ! -s "$tmp/stock" ] || [ ! -s "$tmp/renamed" ]; then
    echo "ERROR: no ai/default objects in $stock, or no renames." >&2
    status=1
  fi
  if [ -n "$leaks" ]; then
    echo "ERROR: ai/unfixed references live ai/default symbols:" $leaks >&2
    status=1
  fi
  if [ -n "$missing" ]; then
    echo "ERROR: ai/unfixed does not define:" $missing >&2
    echo "Is ai/unfixed/unfixed_syms.h stale?" >&2
    status=1
  fi
fi

if [ $status -ne 0 ]; then
  echo "ai/unfixed is the frozen control and must not be modified." >&2
  exit 1
fi

echo "ai/unfixed matches the stock AI vendored at $VENDOR_COMMIT."
