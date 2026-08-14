#!/bin/sh
# Verify that the vendored baseline AI (ai/baseline/) is unmodified.
#
# ai/baseline is the frozen measurement control for the deep-AI project;
# see ai/baseline/README.baseline. Any change to it invalidates every
# benchmark number ever recorded, so this check must never be "fixed" by
# regenerating the manifest.

srcdir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

cd "$srcdir" || exit 1

if ! sha256sum --quiet --strict -c ai/baseline/MANIFEST.sha256; then
  echo "ERROR: ai/baseline differs from the vendored snapshot." >&2
  echo "It is the frozen benchmark control and must not be modified." >&2
  exit 1
fi

echo "ai/baseline matches the vendored snapshot."
