#!/bin/bash

# ring beta2-2017-02-09 bootstrapped nick recovery


# DBG_NO_IO=1
# DBG_NO_PROMPT=1
DBG_NO_LAUNCH=1


# filesystem
source ./toto.dat # $DUMMY_ACC_RCPT_DEV , $DUMMY_ACC_RCPT_ETH ,
                  # $DUMMY_ACC_RCPT_ANN , $DUMMY_ACC_RCPT_SIG , $DUMMY_DEVICE
readonly THIS_DIR=`cd $(dirname "$0") && pwd`
readonly RING_CFG_DIR=$HOME/.config/ring
readonly RING_CRED_DIR=$HOME/.local/share/ring
readonly DRING_YAML_IN="$THIS_DIR/DRING_YAML_IN"
readonly DRING_YAML_IN_TEXT="`cat $THIS_DIR/DRING_YAML_IN`"
readonly DRING_YAML_OUT="$THIS_DIR/DRING_YAML_OUT"

readonly RING_UID_REGEX='CN = Ring, UID ='
readonly HEX_DIGITS_REGEX='^[0-9a-fA-F]*$'

readonly SELECT_ACCOUNT_PROMPT="select an account to restore:"


declare -A RingUids=()


function IsHex() # a_string
{
  [ "$1" ] && echo "$1" | grep -e $HEX_DIGITS_REGEX > /dev/null ; return $? ;
}

function Backup()
{
  timestamp=`date +%s.%N`

  # create backups
  if [ -d $RING_CRED_DIR ]
  then credentials_backup_dirname="$HOME-backup-ring-$timestamp"

[ "$DBG_NO_IO" ] || \

       cp -r $RING_CRED_DIR $credentials_backup_dirname
       echo "created credentials backup directory: $credentials_backup_dirname"
  else mkdir "$RING_CRED_DIR" && echo "created directory: $RING_CRED_DIR"
  fi

  if [ -d $RING_CFG_DIR ]
  then config_backup_dirname="$HOME-backup-dring-$timestamp"

[ "$DBG_NO_IO" ] || \

       cp -r $RING_CFG_DIR  $config_backup_dirname
       echo "created config backup directory: $config_backup_dirname"
  else mkdir "$RING_CFG_DIR" && echo "created directory: $RING_CFG_DIR"
  fi
}

function SanityCheck()
{
for cert_file in `ls "$RING_CRED_DIR/certificates"` ; do echo "cert_file=$cert_file" ; done ;
for crls_file in `ls "$RING_CRED_DIR/crls"`         ; do echo "crls_file=$crls_file" ; done ;
for cred_dir in  `ls -d $RING_CRED_DIR/*/ | grep -v -E "certificates|crls"` ; do cred_dirname=`basename $cred_dir` ; printf "  cred_dirname=$cred_dirname - " ; IsHex "$cred_dirname" || printf "!" ; printf "IsHex('$cred_dirname')=>$?\n" ; done ;

  declare -a cert_files=()
  declare -a crls_files=()

  for cert_file in `ls "$RING_CRED_DIR/certificates"` ; do cert_files=(${cert_files[@]} $cert_file) ; done ;
  for crls_file in `ls "$RING_CRED_DIR/crls"`         ; do crls_files=(${crls_files[@]} $crls_file) ; done ;
  for cred_dir in `ls -d $RING_CRED_DIR/*/ | grep -v -E "certificates|crls"`
  do missing=''
     cred_dir=${cred_dir:0:-1}
     cred_dirname=`basename $cred_dir`

     IsHex "$cred_dirname" || (echo "!IsHex - ignoring: $cred_dirname" ; return)

     cd "$cred_dir"
     [ -f './export.gz'                         ] || missing="$missing (export.gz)"
     [ -f './ring_device.crt' -o -f './dht.crt' ] || missing="$missing (ring_device.crt or dht.crt)"
     [ -f './ring_device.key' -o -f './dht.key' ] || missing="$missing (ring_device.key or dht.key)"

     if [ -z "$missing" ]
     then detected_version=-1
          [ -f 'ca.key'                                    ] && detected_version=0
          [ -f 'knownDevices'      -a -f 'dht.crt'         ] && detected_version=1
          [ -f 'knownDevicesNames' -a -f 'ring_device.crt' ] && detected_version=2

echo "detected_version=$detected_version"

          RingUids[$cred_dir]=$detected_version
     else echo "$cred_dir missing file(s): $missing"
     fi
  done

  echo "(${#cert_files[@]}) files found in $RING_CRED_DIR/certificates/"
  echo "(${#crls_files[@]}) files found in $RING_CRED_DIR/crls/"
  echo "(${#RingUids[@]}) valid candidate directories found under $RING_CRED_DIR/"
  [ -d $RING_CRED_DIR -a -d $RING_CFG_DIR ] || (err=42 && echo "cannot find or create ring directories - quitting")
  ((${#RingUids[@]}))                       || (err=43 && echo "cannot find any ring credentials - quitting")

  (($err)) && return $err || return 0
}

function ParseUids()
{
  # find all dht.crt or ring_device.crt
  for cred_path in ${!RingUids[@]} ; do ParseUid `ls $cred_path/*.crt` ; done ;

printf "\nRingUids[${#RingUids[@]}]=" ; for cred_path in ${!RingUids[@]} ; do printf "\n  RingUids[$cred_path] => ${RingUids[$cred_path]}" ; done ; printf "\n\n" ;
}

function ParseUid() # (cert_file)
{
  cert_file=$1
  cred_dirpath=`dirname $cert_file`
  cred_dirname=`basename $(dirname $cert_file)`

echo -e "\nparsing cert_file=$cert_file" ; # [ -f "$cert_file" ] && echo cert_file || echo !cert_file
# echo "cred_dirname=$cred_dirname"        ; # IsHex "$cred_dirname" && echo cred_dirname || echo !cred_dirname

  [ -f "$cert_file" ] && IsHex "$cred_dirname" || return

  declare -a line_buf=(`openssl x509 -in $cert_file -text -noout | grep "$RING_UID_REGEX"`)
  ring_uid=${line_buf[$((${#line_buf[@]} - 1))]}

  IsHex "$ring_uid" || return

  # cache results
  credentials_version=${RingUids[$cred_dirpath]}
  [ "$credentials_version" -gt "0" ] && RingUids[$cred_dirpath]="$ring_uid"

[ "$credentials_version" !=  ''  ] && printf "parsed ring_uid=$ring_uid in $cred_dirname - "
[ "$credentials_version" -gt "0" ] && printf "v$credentials_version\n" || printf "unknown version - ignoring\n"
}

function SelectAccount()
{
  # prompt user to generate config files
  declare -a cred_paths
  for cred_path in "${!RingUids[@]}" ; do cred_paths=(${cred_paths[@]} $cred_path) ; done ;
  ((${#cred_paths[@]})) && echo "the following account directories were found:"
  for cred_path_n in "${!cred_paths[@]}" ; do echo "  $(($cred_path_n+1))) ${cred_paths[$cred_path_n]}" ; done ;

[ "$DBG_NO_PROMPT" ] && echo "$SELECT_ACCOUNT_PROMPT - *debug no prompt*" && GenerateConfig "${cred_paths[0]}" && return

  cred_path_n=-1
  n_files=${#cred_paths[@]}
  until [ "$cred_path_n" -ge "0" -a "$cred_path_n" -lt "$n_files" ]
  do printf "$SELECT_ACCOUNT_PROMPT (1-$(($n_files)), 0 to cancel): " ; read cred_path_n ;
  done
  [ "$cred_path_n" -gt "0" ] && GenerateConfig "${cred_paths[$(($cred_path_n-1))]}"
}

function GenerateConfig() # cred_path - e.g. $RING_CRED_DIR/0123456789ABCDEF'
{
  cred_path=$1
  cred_dirname=`basename $cred_path 2> /dev/null`

echo -e "\n--GenerateConfig()>" ; IsHex "$cred_dirname" && [ "$ring_uid" -a -d "$cred_path" ] || echo -e "invalid state for cred_dirname=$cred_dirname\n<GenerateConfig()--"

  IsHex "$cred_dirname" && [ "$ring_uid" -a -d "$cred_path" ] || return

  ring_uid=${RingUids[$cred_path]}

echo "compiling configuration file for '$cred_path' ($ring_uid)"

  # export to disk
  dring_yaml_out="$DRING_YAML_OUT-$cred_dirname"
  repalce_regex="s|#LOCAL_DIRNAME_HERE#|$cred_dirname|g           ; \
                 s|#LOCAL_PATH_HERE#|$cred_dirpath|g              ; \
                 s|#RING_UID_HERE#|$ring_uid|                     ; \
                 s|#DUMMY_ACC_RCPT_DEV_HERE#|$DUMMY_ACC_RCPT_DEV| ; \
                 s|#DUMMY_ACC_RCPT_ETH_HERE#|$DUMMY_ACC_RCPT_ETH| ; \
                 s|#DUMMY_ACC_RCPT_ANN_HERE#|$DUMMY_ACC_RCPT_ANN| ; \
                 s|#DUMMY_ACC_RCPT_SIG_HERE#|$DUMMY_ACC_RCPT_SIG| ; \
                 s|#DUMMY_DEVICE#|$DUMMY_DEVICE|"
[ "$DBG_NO_IO" ] || \
  sed -e "$repalce_regex" "$DRING_YAML_IN" > "$dring_yaml_out"

echo "wrote config file: $dring_yaml_out"
echo "<GenerateConfig()--"
}

function Launch()
{
[ "$DBG_NO_LAUNCH" ] && return

  # save config file identically three times:
  cp $dring_yaml_out $HOME/.config/ring/dring.yml
  cp $dring_yaml_out $HOME/.config/ring/_dring.yml
  cp $dring_yaml_out $HOME/.config/ring/dring.yml.bak

  # launch fresh daemon then client
  killall gnome-ring ; while (($?)) ; do killall gnome-ring ; done ;
  killall dring      ; while (($?)) ; do killall dring      ; done ;

  cd /usr/lib/ring/dring
  ./dring --debug --console &
  gnome-ring --debug --version
}

function Main
{
  SanityCheck ; (($?)) && exit ;
  ParseUids
  SelectAccount
  Launch
}


# Main
function DbgMain
{
echo -e "\n\n\n\n\n\n\n\n====== in ======"

Main

echo -e "====== out ======"
}
DbgMain ; DEBUG=0 ; while (($DEBUG)) ; do sleep 5 ; DbgMain ; done ;
