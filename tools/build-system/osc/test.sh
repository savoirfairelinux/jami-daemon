#yes | osc meta pkg -F ../sflphone.meta home:jbonjean sflphone
yes | osc init home:jbonjean:sflphone sflphone-client-gnome
osc add *.tar.gz *.spec
yes | osc commit --force -m "Test"
