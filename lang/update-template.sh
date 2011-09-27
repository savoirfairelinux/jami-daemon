#!/bin/bash

# Parse the GNOME client only to build the langage template

xgettext --from-code=utf-8 --language=C -k_ -kN_ -kc_:1c,2  -kn_:1,2    -ktr2i18n -ktr2i18n:2c,1 -kki18nc:1c,2 -kki18n -ki18n -ki18nc:1c,2 -o sflphone.pot `find ../gnome/src -name \*.c`

