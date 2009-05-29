#!/bin/bash
#####################################################
# File Name: send-emails.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-04-20
# Last Modified: 2009-05-29 18:09:44 -0400
#####################################################

TAG=`date +%Y-%m-%d`
ROOT_DIR="/home/projects/sflphone"
PACKAGING_RESULT_DIR=${ROOT_DIR}/packages-${TAG}
STATUS="OK"

if [ "$1" -ne 0 ]; then
	STATUS="ERROR"
fi

echo
echo "Send notification emails"
echo

MAIL_SUBJECT="[ ${TAG} ] SFLphone Automatic Build System : ${STATUS}"

if [ "$1" -eq 0 ]; then
	echo | mail -s "${MAIL_SUBJECT}" -c emmanuel.milou@savoirfairelinux.com julien.bonjean@savoirfairelinux.com
else
#	(
#	for i in ${PACKAGING_RESULT_DIR}/*.log
#	do
#		uuencode $i $(basename $i)
#	done
#	)
	echo | mail -s "${MAIL_SUBJECT}" -c emmanuel.milou@savoirfairelinux.com julien.bonjean@savoirfairelinux.com 
fi

exit 0

