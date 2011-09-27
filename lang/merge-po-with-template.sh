#!/bin/bash

# Merge sflphone template into the existing po files

for file in `find  .  -name *.po`
do
    msgmerge --update $file sflphone.pot
done
