XDG_CONFIG_HOME="$(mktemp -t --directory jami-unittest-XXXXXX)"
XDG_DATA_HOME="$XDG_CONFIG_HOME"
XDG_CACHE_HOME="$XDG_CACHE_HOME"

export XDG_CONFIG_HOME
export XDG_DATA_HOME
export XDG_CACHE_HOME

SIPLOGLEVEL=5

export SIPLOGLEVEL
