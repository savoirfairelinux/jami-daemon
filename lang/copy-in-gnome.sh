for fichier in `find  .  -name sflphone.po `
do
locale=`echo $fichier | cut -d / -f2`
cp $fichier ../sflphone-client-gnome/po/$locale/sflphone-client-gnome.po
echo "$fichier	copied to	../sflphone-client-gnome/po/$locale/sflphone-client-gnome.po"
done
