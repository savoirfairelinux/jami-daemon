this is the general algorithm that the ring-nick-bootstrap.sh script intends to automate


* determine the files are necesarry for successful id bootstrap and validate ~/.local/share/ring/* subdirs containing appropriately named credential files

```
  such as:
    export.gz                         // required
    ring_device.crt or dht.crt        // required with matching .key
    ring_device.key or dht.key        // required with matching .crt
    knownDevices or knownDevicesNames // TODO: required to bootstrap? - one possibly deprecated orphan
    ca.key                            // TODO: some dirs may not have this file - possibly deprecated orphan
```

* e.g. parse line for key #RING_UID_HERE#
```
$ openssl x509 -in ring_device.crt -text -noout | grep "Issuer: CN = Ring, UID"
....
  Issuer: CN = Ring, UID = '0123456789ABCDEF0123456789ABCDEF01234567'
....
```


* create config YAML from template - DRING_YAML_IN contains toto's config replaced with the following the following sed placeholders:

```
accounts:
  - id: #LOCAL_DIRNAME_HERE#
    Account.archivePath: #LOCAL_PATH_HERE#/export.gz
      certificate: #LOCAL_PATH_HERE#/ring_device.crt
      privateKey: #LOCAL_PATH_HERE#/ring_device.key
    ringAccountReceipt: "{\"id\":\"#RING_UID_HERE#\",\"dev\":\"#DUMMY_ACC_RCPT_DEV_HERE#\",\"eth\":\"#DUMMY_ACC_RCPT_ETH_HERE#\",\"announce\":\"#DUMMY_ACC_RCPT_ANN_HERE#\"}"
    ringAccountReceiptSignature: " !!binary \"#DUMMY_ACC_RCPT_SIG_HERE#\""
    Account.deviceName: #DUMMY_DEVICE#
preferences:
  order: #LOCAL_DIRNAME_HERE#
```


* save this file identically three times:

```
$HOME/.config/ring/dring.yml
$HOME/.config/ring/_dring.yml
$HOME/.config/ring/dring.yml.bak
```


* launch fresh daemon then client

```
$ killall gnome-ring ; while (($?)) ; do killall gnome-ring ; done ;
$ killall dring      ; while (($?)) ; do killall dring      ; done ;

$ cd /usr/lib/ring/dring
$ ./dring --debug --console
$ gnome-ring --debug --version
```
