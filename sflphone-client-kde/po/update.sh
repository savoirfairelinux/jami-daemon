xgettext --c++ --kde -ktr2i18n -o sflphone-client-kde.pot ../src/*.cpp ../src/*.h ../build/src/*.h ../build/src/*.cpp

for fichier in `find  .  -name *.po `
do
msgmerge --update $fichier sflphone-client-kde.pot
done