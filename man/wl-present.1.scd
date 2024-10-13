wl-present(1)

# NAME

wl-present - an interactive client for *wl-mirror*(1)

# SYNOPSIS

*wl-present* [OPTIONS] <COMMAND> [args...]

# DESCRIPTION

*wl-present* is an wrapper around *wl-mirror*(1) that uses its streaming mode
together with *pipectl*(1) to interactively control what is being captured.

The subcommands of *wl-present* are intended to be mapped to keyboard shortcuts
in your wayland compositor for easy access.

# COMMANDS

*help*
	Show help message and exit.

*mirror* [OUTPUT] [OPTIONS]
	Start *wl-mirror*(1) capturing OUTPUT. If no output is given, *slurp*(1) is
	used to select the output or region to capture. If any additional OPTIONS
	are specified, they are passed on to *wl-mirror*.

*set-output* [OUTPUT]
	Set the captured output of a running *wl-present* session. If no output is
	given, *slurp*(1) is used to select the output to capture.

*set-region* [REGION]
*unset-region*
	Set the captured region of a running *wl-present* session. If no region is
	given, *slurp*(1) us used to select a region to capture.

*set-scaling* [SCALING]
	Set the scaling mode of a running *wl-present* session. If no region is
	given, *dmenu*(1) (or a compatible replacement) is used to select a scaling
	mode.

*freeze*,
*unfreeze*,
*toggle-freeze*
	Set the freeze state of a running *wl-present* session.

*fullscreen*
*unfullscreen*
	Set the fullscreen state of a running *wl-present* session.

*fullscreen-output* [OUTPUT]
*no-fullscreen-output*
	Set the fullscreen target output of a running *wl-present* session.

*custom* [OPTIONS]
	Send custom options to a running *wl-present* session. If no options are
	given, *dmenu*(1) (or a compatible replacement) are used to select an
	option.

# OPTIONS

*-n*, *--name*
	Use an alternative pipe name (default is wl-present).
	This allows multiple instances of wl-present to run at the same time.

# ENVIRONMENT VARIABLES

*WL_PRESENT_DMENU*
	Overrides the used dmenu implementation. When set, this command is used to
	select options instead of the default. When unset, *wl-present* attempts to
	use *wofi*(1), *wmenu*(1), *fuzzel*(1), *rofi*(1), and *dmenu*(1), in this
	order.

	Example: WL_PRESENT_DMENU="rofi -dmenu -p present"

*WL_PRESENT_PIPECTL*
	Overrides the used pipectl implementation. When set, this command is used to
	pipe options to *wl-mirror*(1) instead of the default. When unset,
	*wl-present* attempts to use *pipectl*(1) or an included pipectl shim
	written in bash if pipectl is not available.

*WL_PRESENT_SLURP*
	Overrides the used slurp implementation and options. When set, this command
	is used to select regions and outputs instead of the default. When unset,
	*wl-present* attempts to use *slurp*(1) without any additional options.

	Example: WL_PRESENT_SLURP="slurp -c #859900 -w 2"

*WL_PRESENT_PIPE_NAME*
	Overrides the default pipe name (default is wl-present). This is equivalent
	to the *--name* option above.

	Example: WL_PRESENT_PIPE_NAME=monitor2 wl-present mirror HDMI-A-1

# AUTHORS

Maintained by Ferdinand Bachmann <ferdinand.bachmann@yrlf.at>. More information on
*wl-mirror* can be found at <https://github.com/Ferdi265/wl-mirror>.

# SEE ALSO

*wl-mirror*(1) *pipectl*(1) *slurp*(1) *wofi*(1) *wmenu*(1) *fuzzel*(1)
*rofi*(1) *dmenu*(1)
