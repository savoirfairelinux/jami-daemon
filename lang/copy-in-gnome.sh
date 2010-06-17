for fichier in `find  .  -name *.po `
do
locale=`echo $fichier | cut -d / -f2`
cp $fichier ../sflphone-client-gnome/po/$locale/$locale.po
echo "$fichier	copied to	../sflphone-client-gnome/po/$locale/$locale.po"
done
