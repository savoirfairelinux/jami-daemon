for i in *.po
do
    locale=`echo $i | cut -d. -f1`
    cp -v $i ../kde/po/$locale/sflphone-client-kde.po
done
