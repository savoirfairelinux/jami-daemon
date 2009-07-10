xgettext --from-code=utf-8 --c++ --kde -ktr2i18n -ktr2i18n:2c,1 -kki18nc:1c,2 -kki18n -ki18n -ki18nc:1c,2 -o sflphone-client-kde.pot ../src/*.cpp ../src/*.h ../src/conf/*.h ../src/conf/*.cpp ../build/src/*.h ../build/src/*.cpp 

for fichier in `find  .  -name *.po `
do
msgmerge --update $fichier sflphone-client-kde.pot
done