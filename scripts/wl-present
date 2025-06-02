#!/bin/bash

usage() {
    echo "usage: wl-present [options] <command> [argument]"
    echo
    echo "start wl-mirror and control the mirrored output and region in a convenient way"
    echo
    echo "commands:"
    echo "  help                        show this help"
    echo "  mirror [output] [options]   start wl-mirror on output [output] (default asks via slurp)"
    echo "  set-output [output]         set the recorded output (default asks via slurp)"
    echo "  set-region [region]         set the recorded region (default asks via slurp)"
    echo "  unset-region                unset the recorded region"
    echo "  set-scaling [scale]         set the scaling mode (default asks via rofi)"
    echo "  freeze                      freeze the screen"
    echo "  unfreeze                    resume the screen capture after freeze"
    echo "  toggle-freeze               toggle freeze state of screen capture"
    echo "  fullscreen                  fullscreen the wl-mirror window"
    echo "  unfullscreen                unfullscreen the wl-mirror window"
    echo "  fullscreen-output [output]  set fullscreen target output, implies fullscreen (default asks via slurp)"
    echo "  no-fullscreen-output        unset fullscreen target output, implies unfullscreen"
    echo "  custom [options]            send custom options to wl-mirror (default asks via rofi)"
    echo
    echo "options:"
    echo "  -n, --name                  use an alternative pipe name (default is wl-present)"
    echo "                              this allows multiple instances of wl-present to run"
    echo "                              at the same time."
    echo "dependencies:"
    echo "  wl-mirror, bash, slurp, pipectl (optional), and either wofi, wmenu, rofi, fuzzel, or dmenu"
    echo
    echo "environment variables":
    echo "  WL_PRESENT_DMENU            overrides the used dmenu implementation"
    echo "  WL_PRESENT_PIPECTL          overrides the used pipectl implementation"
    echo "  WL_PRESENT_SLURP            overrides the used slurp implementation"
    echo "  WL_PRESENT_PIPE_NAME        overrides the default pipe name (default is wl-present)"
    exit 0
}

if [[ -n "$WL_PRESENT_DMENU" ]]; then
    DMENU="$WL_PRESENT_DMENU"
elif type -p wofi >/dev/null; then
    DMENU="wofi -d"
elif type -p wmenu >/dev/null; then
    DMENU=wmenu
elif type -p fuzzel >/dev/null; then
    DMENU="fuzzel -d"
elif type -p rofi >/dev/null; then
    DMENU="rofi -dmenu"
else
    DMENU=dmenu
fi

if [[ -n "$WL_PRESENT_PIPECTL" ]]; then
    PIPECTL="$WL_PRESENT_PIPECTL"
elif type -p pipectl >/dev/null; then
    PIPECTL=pipectl
else
    PIPECTL=pipectl-shim
fi

if [[ -n "$WL_PRESENT_SLURP" ]]; then
    SLURP="$WL_PRESENT_SLURP"
else
    SLURP=slurp
fi

if [[ -n "$WL_PRESENT_PIPE_NAME" ]]; then
    PRESENT_PIPE_NAME="$WL_PRESENT_PIPE_NAME"
else
    PRESENT_PIPE_NAME=wl-present
fi

pipectl-shim() {
    PIPEPATH="${XDG_RUNTIME_DIR:-"${TMPDIR:-/tmp}"}"
    PIPENAME="pipectl.$(id -u).pipe"
    MODE=
    FORCE=0

    while [[ $# -gt 0 && "${1:0:1}" == "-" ]]; do
        opt="$1"
        shift
        case "$opt" in
            -n|--name) arg="$1"; shift; PIPENAME="pipectl.$(id -u).$arg.pipe";;
            -i|--in) MODE=in;;
            -o|--out) MODE=out;;
            -f|--force) FORCE=1;;
            --) break;;
        esac
    done

    PIPE="$PIPEPATH/$PIPENAME"

    case "$MODE" in
        "in") if [[ ! -p "$PIPE" ]]; then
                echo "error: could not open pipe at '$PIPE': No such file or directory" >&2
                return 1
            else
                cat > "$PIPE"
            fi;;
        "out") if [[ "$FORCE" -eq 0 && -p "$PIPE" ]]; then
                echo "error: cannot create pipe at '$PIPE': File exists" >&2
                return 1
            else
                [[ "$FORCE" -eq 1 ]] && rm -f "$PIPE"
                mkfifo "$PIPE"; (tail -f "$PIPE" & echo > "$PIPE"; wait); rm -f "$PIPE"
            fi;;
    esac
}

slurp-output() {
    $SLURP -or -f '%o' 2>/dev/null
}

slurp-region() {
    $SLURP 2>/dev/null
}

slurp-output-or-region() {
    $SLURP -o -f '%x,%y %wx%h %o' 2>/dev/null
}

mirror() {
    if [[ "$#" -eq 0 || "$1" =~ ^- ]]; then
        OUTPUT_REGION=$(ask-output-or-region)

        ARGS=("-S" "-r" "$OUTPUT_REGION" "$@")
    else
        OUTPUT="$1"
        shift

        ARGS=("-S" "$@" "$OUTPUT")
    fi

    $PIPECTL -n "$PRESENT_PIPE_NAME" -o | wl-mirror "${ARGS[@]}"
}

mirror-cmd() {
    $PIPECTL -n "$PRESENT_PIPE_NAME" -i <<< "$1"
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

set-fullscreen-output() {
    mirror-cmd "--fullscreen-output $1"
}

ask-output() {
    slurp-output
    [[ $? -ne 0 ]] && exit 1
}

ask-region() {
    slurp-region
    [[ $? -ne 0 ]] && exit 1
}

ask-output-or-region() {
    slurp-output-or-region
    [[ $? -ne 0 ]] && exit 1
}

ask-scaling() {
    (echo fit; echo cover; echo exact; echo linear; echo nearest) | $DMENU -p "wl-present scaling"
    [[ $? -ne 0 ]] && exit 1
}

ask-custom() {
    cat <<EOF | $DMENU -p "wl-present custom"
--verbose
--no-verbose
--show-cursor
--no-show-cursor
--invert-colors
--no-invert-colors
--freeze
--unfreeze
--toggle-freeze
--fullscreen
--no-fullscreen
--fullscreen-output
--no-fullscreen-output
--scaling fit
--scaling cover
--scaling exact
--scaling linear
--scaling nearest
--backend
--transform
--region
--no-region
EOF
    [[ $? -ne 0 ]] && exit 1
}

while [[ $# -gt 0 && "${1:0:1}" == "-" ]]; do
    opt="$1"
    shift
    case "$opt" in
        -n|--name) arg="$1"; shift; PRESENT_PIPE_NAME="$arg";;
        -h|--help) usage;;
        --) break;;
    esac
done

if [[ $# -eq 0 ]]; then
    usage
fi

case "$1" in
    help) usage;;
    mirror) shift; mirror "$@";;
    set-output) set-output "${2:-$(ask-output)}";;
    set-region) set-region "${2:-$(ask-region)}";;
    unset-region|no-region) mirror-cmd --no-region;;
    set-scaling) set-scaling "${2:-$(ask-scaling)}";;
    freeze) mirror-cmd --freeze;;
    unfreeze) mirror-cmd --unfreeze;;
    toggle-freeze) mirror-cmd --toggle-freeze;;
    fullscreen) mirror-cmd --fullscreen;;
    unfullscreen|no-fullscreen) mirror-cmd --no-fullscreen;;
    fullscreen-output) set-fullscreen-output "${2:-$(ask-output)}";;
    no-fullscreen-output) mirror-cmd --no-fullscreen-output;;
    custom) mirror-cmd "${2:-$(ask-custom)}";;
    *) usage;;
esac
