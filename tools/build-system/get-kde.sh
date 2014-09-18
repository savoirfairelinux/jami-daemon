#!/bin/bash
# Get the KDE client
# To get all files, you have to create the tarball from scratch,
# then extract files from it. The directory is renamed "kde".
# $WORKSPACE is declared in setenv.sh
set -o errexit
source $(dirname $0)/setenv.sh
cd "$WORKSPACE"
baseurl='https://projects.kde.org/projects'
config_uri='/playground/network/sflphone-kde/repository/revisions/master/raw/data/config.ini'
createtarball_uri='/kde/kdesdk/kde-dev-scripts/repository/revisions/master/raw/createtarball/create_tarball.rb'

set -x

# timeout in seconds
let -i timeout=300
let -i timestamp=$(date +%s)
while ! curl --fail --remote-name ${baseurl}${config_uri}
do
    if [ $(date +%s) -gt $(( $timestamp + $timeout)) ]; then
        break
    fi
    sleep 15
done
let -i timestamp=$(date +%s)
while ! curl --fail --remote-name ${baseurl}${createtarball_uri}
do
    if [ $(date +%s) -gt $(( $timestamp + $timeout)) ]; then
        break
    fi
    sleep 15
done

ruby create_tarball.rb --noaccount --application sflphone-kde
rm -rf kde
rm -rf sflphone-kde-*.tar.*
rm create_tarball.rb config.ini
mv sflphone-kde-* kde
