#!/bin/sh
# W28: auto-workers must be willing to stack on a worksite when the
# summed activity rate saves whole turns of completion.

count_stacks() {
  "$SQ" "$1" units | awk -F, '
    $2 == "Workers" || $2 == "Engineers" { n[$1 "," $3 "," $4]++ }
    END { c = 0; for (k in n) if (n[k] >= 2) c++; print c }'
}

pre=$(count_stacks "$POS")
[ "$pre" -eq 0 ] || {
  echo "position already contains $pre stacked worker tiles; regenerate"
  exit 1
}

post=$(count_stacks "$POST")
[ "$post" -ge 1 ] || {
  echo "no worker stack formed within the window ($post)"
  exit 1
}

exit 0
