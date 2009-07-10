xgettext --from-code=utf-8 --language=C --language=C++ --kde \
     -k_ -ktr2i18n -ktr2i18n:2c,1 -kki18nc:1c,2 -kki18n -ki18n -ki18nc:1c,2 \
     -o sflphone.pot \
     ../sflphone-client-kde/src/*.cpp        ../sflphone-client-kde/src/*.h \
     ../sflphone-client-kde/src/conf/*.h     ../sflphone-client-kde/src/conf/*.cpp \
     ../sflphone-client-kde/build/src/*.h    ../sflphone-client-kde/build/src/*.cpp \
     ../sflphone-client-gnome/src/*.c        ../sflphone-client-gnome/src/config/*.c \
     ../sflphone-client-gnome/src/dbus/*.c   ../sflphone-client-gnome/src/contacts/*.c

for fichier in `find  .  -name *.po `
do
msgmerge --update $fichier sflphone.pot
done