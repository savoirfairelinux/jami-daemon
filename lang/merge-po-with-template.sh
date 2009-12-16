#!/bin/bash

# Merge sflphone template into the existing po files

for fichier in `find  .  -name *.po`
do
msgmerge --update $fichier sflphone.pot
done
