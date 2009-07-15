for fichier in `find  .  -name sflphone.po `
do
locale=`echo $fichier | cut -d / -f2`
cp $fichier ../sflphone-client-kde/po/$locale/sflphone-client-kde.po
done
