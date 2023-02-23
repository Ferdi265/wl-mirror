#!/bin/bash

usage() {
    echo "usage: wl-present <command> [argument]"
    echo
    echo "start wl-mirror and control the mirrored output and region in a convenient way"
    echo
    echo "commands:"
    echo "  help                show this help"
    echo "  mirror [output]     start wl-mirror on output [output] (default asks via slurp)"
    echo "  set-output [output] set the recorded output (default asks via slurp)"
    echo "  set-region [region] set the recorded region (default asks via slurp)"
    echo "  set-scaling [scale] set the scaling mode (default asks via rofi)"
    echo "  freeze              freeze the screen"
    echo "  unfreeze            resume the screen capture after freeze"
    echo "  toggle-freeze       toggle freeze state of screen capture"
    echo "  custom [options]    send custom options to wl-mirror (default asks via rofi)"
    echo
    echo "dependencies:"
    echo "  wl-mirror, pipectl, slurp, and rofi or dmenu"
    exit 0
}

type -p rofi >/dev/null
if [[ $? -eq 0 ]]; then
    DMENU=dmenu-rofi
else
    DMENU=dmenu
fi

dmenu-rofi() {
    rofi -dmenu -width 30 -columns 1 "$@"
}

slurp-output() {
    slurp -b \#00000000 -B \#00000000 -c \#859900 -w 4 -f %o -or 2>/dev/null
}

slurp-region() {
    slurp -b \#00000000 -c \#859900 -w 2 2>/dev/null
}

mirror() {
    pipectl -n wl-present -o | wl-mirror -S "$1"
}

mirror-cmd() {
    pipectl -n wl-present -i <<< "$1"
}

set-output() {
    mirror-cmd "$1"
}

set-region() {
    mirror-cmd "-r '$1'"
}

set-scaling() {
    mirror-cmd "-s $1"
}

ask-output() {
    slurp-output
    [[ $? -ne 0 ]] && exit 1
}

ask-region() {
    slurp-region
    [[ $? -ne 0 ]] && exit 1
}

ask-scaling() {
    (echo linear; echo nearest; echo exact) | "$DMENU" -p "wl-present scaling"
    [[ $? -ne 0 ]] && exit 1
}

ask-custom() {
    cat <<EOF | "$DMENU" -p "wl-present custom"
--verbose
--no-verbose
--show-cursor
--no-show-cursor
--invert-colors
--no-invert-colors
--freeze
--unfreeze
--toggle-freeze
--scaling linear
--scaling nearest
--scaling exact
--transform
--region
--no-region
EOF
    [[ $? -ne 0 ]] && exit 1
}

if [[ $# -eq 0 || $# -gt 2 ]]; then
    usage
fi

case "$1" in
    help) usage;;
    mirror) mirror "${2:-$(ask-output)}";;
    set-output) set-output "${2:-$(ask-output)}";;
    set-region) set-region "${2:-$(ask-region)}";;
    set-scaling) set-scaling "${2:-$(ask-scaling)}";;
    freeze) mirror-cmd --freeze;;
    unfreeze) mirror-cmd --unfreeze;;
    toggle-freeze) mirror-cmd --toggle-freeze;;
    custom) mirror-cmd "${2:-$(ask-custom)}";;
    *) usage;;
esac
