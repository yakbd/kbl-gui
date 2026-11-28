#!/bin/sh
# The target picker, end to end: for each layout and each target, the editor opens the layout, picks the target,
# validates, builds, builds the installer or archive and saves the unedited layout (--build), and the self-test edits,
# saves and validates for that target (--self-test --target). Each result is compared with what the yakbd tool itself
# says about the layout, so a layout that does not build for a target is a pass when the editor shows that too.
# Writes <out>/summary.txt, one line per case: PASS or FAIL, the case, the reason.
#   tools/targets-check.sh <editor executable> <out> [layout.kbl.json ...]
# The editor finds yakbd as always ($YAKBD_CLI first). It needs a display (DISPLAY, WAYLAND_DISPLAY or
# QT_QPA_PLATFORM=offscreen). TARGETS overrides the targets (default: windows macos xkb).
set -u
gui=$1
out=$2
shift 2
here=$(cd "$(dirname "$0")" && pwd)
[ $# -gt 0 ] || set -- "$here/../tests/data/macos-option-deadkeys.kbl.json" "$here/../tests/data/xkb-caps-deadkeys.kbl.json" \
  "$here/../tests/data/minimal-qwerty.kbl.json"
targets=${TARGETS:-windows macos xkb}
mkdir -p "$out"
summary="$out/summary.txt"
: > "$summary"
cli=${YAKBD_CLI:-}
[ -n "$cli" ] || cli=$("$gui" --self-test --target windows "$1" 2>/dev/null | sed -n 's/^ok   yakbd [^ ]* (\(.*\)) validates.*/\1/p')
say() { echo "$1 $2: $3" | tee -a "$summary"; }

echo "editor: $gui" > "$out/about.txt"
echo "yakbd: $cli ($("$cli" version 2>&1))" >> "$out/about.txt"

for layout in "$@"; do
  name=$(basename "$layout" .kbl.json)
  for t in $targets; do
    id="$name/$t"
    log="$out/$name-$t"
    if [ "$t" = windows ]; then pick=-windows; else pick="-target $t"; fi
    # What the tool says: exit code and the summary line "E error(s), W warning(s), ...; L loss(es), D default(s) used".
    "$cli" validate $pick "$layout" > "$log.cli.txt" 2>&1
    want=$?
    if [ $want -eq 2 ]; then
      say FAIL "$id target" "yakbd has no target $t ($(head -1 "$log.cli.txt"))"
      continue
    fi
    losses=$(sed -n 's/.*; \([0-9]*\) loss(es).*/\1/p' "$log.cli.txt" | tail -1)

    # 1. The window: pick, validate, build, archive, save unchanged.
    rm -rf "$log.build"
    "$gui" --no-settings --target "$t" --build "$log.build" "$layout" > "$log.gui.txt" 2>&1
    step() { grep "^check $1 $t " "$log.gui.txt" | head -1; }
    v=$(step validate)
    case "$v" in
      *" PASS "*) seen=0 ;;
      *" FAIL "*) seen=1 ;;
      *) say FAIL "$id validate" "no result from the editor ($(tail -1 "$log.gui.txt"))"; continue ;;
    esac
    if [ $seen -ne $want ]; then
      say FAIL "$id validate" "the editor says ${v#check validate $t } but yakbd exits $want"
    elif ! echo "$v" | grep -q "losses=$losses,"; then
      say FAIL "$id losses" "yakbd reports $losses loss(es), the Problems panel: $v"
    else
      say PASS "$id validate" "exit $want, $losses loss(es) in the Problems panel"
    fi
    for s in build archive; do
      r=$(step $s)
      if [ -z "$r" ]; then
        if [ $s = archive ] && grep -q "^check build $t" "$log.gui.txt" && ! grep -q "^check timeout" "$log.gui.txt"; then
          say PASS "$id $s" "the target has no installer command"
        else
          say FAIL "$id $s" "no result ($(tail -1 "$log.gui.txt"))"
        fi
      elif [ $want -eq 0 ] && echo "$r" | grep -q " PASS "; then
        say PASS "$id $s" "${r#check $s $t PASS }"
      elif [ $want -ne 0 ] && echo "$r" | grep -q " FAIL "; then
        say PASS "$id $s" "refused, as yakbd refuses the layout"
      else
        say FAIL "$id $s" "$r (yakbd validate exits $want)"
      fi
    done
    r=$(step save-unchanged)
    case "$r" in
      *" PASS "*) say PASS "$id save-unchanged" "${r#check save-unchanged $t PASS }" ;;
      *) say FAIL "$id save-unchanged" "${r:-no result}" ;;
    esac

    # 2. Without a window: open, save unchanged, edit, save, reopen, validate for the target.
    if [ $want -ne 0 ]; then
      say PASS "$id self-test" "not run: the layout does not build for $t"
    elif "$gui" --self-test --target "$t" "$layout" > "$log.self.txt" 2>&1; then
      say PASS "$id self-test" "$(grep -c '^ok' "$log.self.txt") steps"
    else
      say FAIL "$id self-test" "$(grep FAIL "$log.self.txt" | head -1)"
    fi
  done
done
echo "$(grep -c '^PASS' "$summary") passed, $(grep -c '^FAIL' "$summary") failed" | tee "$out/totals.txt"
