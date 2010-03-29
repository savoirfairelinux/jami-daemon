SERVERPORT=5064

sipp -sf account_uas_register_bis.xml 192.168.50.79 -i 192.168.50.182 -p ${SERVERPORT} -l 1 -m 1

sipp -sf account_uas_receive_transfer.xml 192.168.50.79 -i 192.168.50.182 -p ${SERVERPORT} -l 1