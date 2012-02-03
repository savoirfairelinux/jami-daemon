#!/bin/bash

# Requirements:

# 1) apt-get install kvm virsh

# 2) Make sure kvm and kvm_intel are installed
# lsmod | grep kvm

# 3) Get a .iso file for the coresponding distro

# 4) Install a new VM on a disk image
# kvm-img create vm0-ubuntu-10.04.1-desktop-i386.qcow2 -f qcow2 6G
# kvm -k en-us -usbdevice tablet -hda vm0-ubuntu-10.04.1-desktop-i386.qcow2 -cdrom ubuntu-10.04.1-desktop-i386.iso -boot d -m 512
# kvm -k en-us -usbdevice tablet -hda vm0-ubuntu-10.04.1-desktop-i386.qcow2 -cdrom ubuntu-10.04.1-desktop-i386.iso -boot c -m 512

# 5) Create the VM
# virsh -c qemu:///system define Ubuntu-10.04-i386-on-KVM.xml

# 6) Copy Host's public ssh key in order to avoid password request
# ssh-copy-id sflphone@machine

# 7) Take a snapshot of this disk image
# kvm-img snapshot -c tag_name disk_image_filename

# 8) Add NOPASSWORD in sudoers

VM=Ubuntu-10.04-i386-on-KVM

VIRSH="virsh -c qemu:///system"

# Get the full path to vm's
DISK_IMG=$($VIRSH dumpxml "${VM}"|grep source.file|cut -d"'" -f2)
echo "Disk Image: $DISK_IMG"
# Get MAC address for this vm
MAC_ADDR=$($VIRSH dumpxml "${VM}"|grep mac.address|cut -d"'" -f2)
echo "Mac Address: $MAC_ADDR"

# Reset disk to last snapshot
LAST_SNAPSHOT_ID=$(kvm-img snapshot -l $DISK_IMG|tail -n 1|cut -d' ' -f1)
kvm-img snapshot -a $LAST_SNAPSHOT_ID $DISK_IMG

# Create VM
$VIRSH start $VM

# Get its IP address
echo -n "Waiting for IP address..."
LAST_STAMP=$(grep $MAC_ADDR /var/lib/misc/dnsmasq.leases | cut -d' ' -f1)
while [ 1 ]
do
  NEXT_STAMP=$(grep $MAC_ADDR /var/lib/misc/dnsmasq.leases | cut -d' ' -f1)
  if [ $LAST_STAMP != $NEXT_STAMP ]; then
    IPADDR=$(grep $MAC_ADDR /var/lib/misc/dnsmasq.leases | cut -d' ' -f3)
    break
  fi
  echo -n "."
  sleep 0.5
done
echo " got $IPADDR"

HOST=sflphone@$IPADDR

# Connect ssh
echo -n "Waiting for ssh to start..."
while [ 1 ]; do
  ssh $HOST echo Connected && break
  sleep 2
done

# create an archive of the repository
# git archive --format=tar -o sflphone.tar HEAD
# gzip sflphone.tar
# scp -C sflphone.tar.gz $HOST

# sudo add-apt-repository ppa:savoirfairelinux

# sudo apt-get build-dep sflphone-common
# sudo apt-get build-dep sflphone-client-gnome




