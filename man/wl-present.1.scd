wl-present(1)

# NAME

wl-present - an interactive client for *wl-mirror*(1)

# SYNOPSIS

*wl-present* [help|mirror|set-output|set-region|set-scaling|custom] [args...]

# DESCRIPTION

*wl-present* is an wrapper around *wl-mirror*(1) that uses its streaming mode together with *pipectl*(1) to interactively control what is being captured.

The subcommands of *wl-present* are intended to be mapped to keyboard shortcuts in your wayland compositor for easy access.

# COMMANDS

*help*
	Show help message and exit.

*mirror* [OUTPUT]
	Start *wl-mirror*(1) capturing OUTPUT. If no output is given, *slurp*(1) is used to select the output to capture.

*set-output* [OUTPUT]
	Set the captured output of a running *wl-present* session. If no output is given, *slurp*(1) is used to select the output to capture.

*set-region* [REGION]
	Set the captured region of a running *wl-present* session. If no region is given, *slurp*(1) us used to select a region to capture.

*set-scaling* [SCALING]
	Set the scaling mode of a running *wl-present* session. If no region is given, *rofi*(1) or *dmenu*(1) is used to select a scaling mode.

*custom* [OPTIONS]
	Send custom options to a running *wl-present* session. If no options are given, *rofi*(1) or *dmenu*(1) are used to select an option.

# AUTHORS

Maintained by Ferdinand Bachmann <theferdi265@gmail.com>. More information on *wl-mirror* can be found at <https://github.com/Ferdi265/wl-mirror>.

# SEE ALSO

*wl-mirror*(1) *pipectl*(1) *slurp*(1) *rofi*(1) *dmenu*(1)
