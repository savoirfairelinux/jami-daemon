#!/bin/bash
# Get the KDE client
# To get all files, you have to create the tarball from scratch,
# then extract files from it. The directory is renamed "kde".
# $WORKSPACE is declared in setenv.sh
set -o errexit
source $(dirname $0)/setenv.sh
cd "$WORKSPACE"
curl -O https://projects.kde.org/projects/playground/network/sflphone-kde/repository/revisions/master/raw/data/config.ini
curl -O https://projects.kde.org/projects/kde/kdesdk/kde-dev-scripts/repository/revisions/master/raw/createtarball/create_tarball.rb
ruby create_tarball.rb --noaccount --application sflphone-kde
rm -rf kde
rm -rf sflphone-kde-*.tar.*
rm create_tarball.rb config.ini
mv sflphone-kde-* kde
