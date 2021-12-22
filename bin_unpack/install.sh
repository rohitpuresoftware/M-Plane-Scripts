#!/bin/bash

ORAN_MODULE_DIR="/home/user/PureSoftware/MP_3.0/yang_models"
INSTALLER_DIR="/home/user/PureSoftware/MP_3.0"
INSTALLER_BIN=$INSTALLER_DIR/bin/
INSTALLER_LIB=$INSTALLER_DIR/lib/
INSTALLER_INCLUDE=$INSTALLER_DIR/include/
INSTALLER_SHARE=$INSTALLER_DIR/share/
CONFIG_DATA_DIR=$INSTALLER_DIR/config_data_xml

export PATH="$PATH:$INSTALLER_BIN"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$INSTALLER_LIB"

if [ -f "$INSTALLER_DIR/installation_log.txt" ];then
rm $INSTALLER_DIR/installation_log.txt

fi


	$PWD/uninstall.sh
	if [ ! -d "$INSTALLER_DIR/yang_models" ] ;then
	mkdir -p $INSTALLER_DIR/yang_models && echo "creating $INSTALLER_DIR/yang_models directory"
	fi

	if [ ! -d "$INSTALLER_DIR/bin" ] ;then
	mkdir -p $INSTALLER_DIR/bin && echo "creating $INSTALLER_DIR/bin directory"
	fi


	if [ ! -d "$INSTALLER_DIR/lib" ] ;then
	mkdir -p $INSTALLER_DIR/lib && echo "creating $INSTALLER_DIR/lib directory"
	fi

	if [ ! -d "$INSTALLER_DIR/include" ] ;then
	mkdir -p $INSTALLER_DIR/include && echo "creating $INSTALLER_DIR/include directory"
	fi

	if [ ! -d "$INSTALLER_DIR/share" ] ;then
	mkdir -p $INSTALLER_DIR/share && echo "creating $INSTALLER_DIR/share directory"
	fi

	if [ ! -d "$INSTALLER_DIR/sysrepo" ] ;then
	mkdir -p $INSTALLER_DIR/sysrepo && echo "creating $INSTALLER_DIR/sysrepo directory"
	fi


#apt-get update && apt-get install -y openssl libssl-dev vim python3-pip libpcre2-8-0 libpcre2-dev


echo "Copying library and headers "

cp -rf $PWD/usr/local/bin/* $INSTALLER_BIN


cp -rf $PWD/usr/local/include/* $INSTALLER_INCLUDE 

cp -rf $PWD/usr/local/lib/* $INSTALLER_LIB 

cp -rf $PWD/mplane/build/libPS_MP.a $INSTALLER_LIB
cp -rf $PWD/usr/local/share/* $INSTALLER_SHARE 

cp -rf $PWD/usr/local/bin/netopeer* $INSTALLER_BIN


cp -rf $PWD/mplane/build/ruapp $INSTALLER_BIN
echo "Copying oran specific yang modules"
cp -rf $PWD/mplane/oran_yang_model/* $ORAN_MODULE_DIR  #&& rm -rf $PWD/mplane/oran_yang_model

ldconfig &> $INSTALLER_DIR/installation_log.txt

echo "Setting up first time server installation... (do not repeat)" 


echo "Copy state data xml"
cp -rf $PWD/mplane/state_data_xml $INSTALLER_DIR
echo "Copy Config data xml"
cp -rf $PWD/mplane/config_data_xml $INSTALLER_DIR
echo "Installing ruapp and library"

cp -rf $PWD/usr/local/lib/tsize.sh /usr/local/bin/

export SYSREPOCTL_EXECUTABLE=`which sysrepoctl`
export SYSREPOCFG_EXECUTABLE=`which sysrepocfg`
export OPENSSL_EXECUTABLE=`which openssl`

cp -rf $PWD/mplane/example $INSTALLER_DIR 
echo "Running netopeer2 setup scripts"
$PWD/netopeer2_scripts/setup.sh && $PWD/netopeer2_scripts/merge_hostkey.sh && $PWD/netopeer2_scripts/merge_config.sh #&& rm -rf $INSTALLER_DIR/netopeer2_scripts


echo "Running o-ran yang installation scripts"

$PWD/oran_scripts/install_oran_yang_model.sh $ORAN_MODULE_DIR #&& rm -rf $INSTALLER_DIR/oran_scripts
echo "Enabling feature"

SCTL_MODULES=`sysrepoctl -l`


SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-wg4-features"`
if [ ! -z "$SCTL_MODULE" ];then
echo "Enabling ORDERED-TRANSMISSION feature in o-ran-wg4-features"
sysrepoctl --change o-ran-wg4-features --enable-feature ORDERED-TRANSMISSION
fi


SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-module-cap"`
if [ ! -z "$SCTL_MODULE" ];then
echo "enabling PRACH-STATIC-CONFIGURATION-SUPPORTED SRS-STATIC-CONFIGURATION-SUPPORTED CONFIGURABLE-TDD-PATTERN-SUPPORTED in o-ran-module-cap"
sysrepoctl --change o-ran-module-cap --enable-feature PRACH-STATIC-CONFIGURATION-SUPPORTED
sysrepoctl --change o-ran-module-cap --enable-feature SRS-STATIC-CONFIGURATION-SUPPORTED
sysrepoctl --change o-ran-module-cap --enable-feature CONFIGURABLE-TDD-PATTERN-SUPPORTED
fi



SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-usermgmt"`
if [ -z "$SCTL_MODULE" ];then
echo "configuring o-ran-usermgmt"
sysrepoctl -i $ORAN_MODULE_DIR/o-ran-usermgmt.yang && sysrepocfg -W $CONFIG_DATA_DIR/o-ran-user.xml -m o-ran-usermgmt --datastore running -f "xml"
fi


SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-delay-management"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-delay-management"
sysrepoctl --change o-ran-delay-management --enable-feature ADAPTIVE-RU-PROFILE
sysrepocfg --copy-from=$CONFIG_DATA_DIR/del_mgmt.xml --module o-ran-delay-management --datastore startup 

fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-mplane-int"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-mplane-int"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/mplane_int.xml --module o-ran-mplane-int --datastore startup 
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "ietf-interfaces"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring ietf-interfaces"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/ietf_interef.xml --module ietf-interfaces --datastore startup 
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-processing-element"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-processing-element"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/proc_el.xml --module o-ran-processing-element --datastore startup 
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-uplane-conf"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-uplane-conf"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/uplane_conf.xml --module o-ran-uplane-conf --datastore startup 
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "ietf-netconf-server"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring ietf-netconf-server"
sysrepocfg --edit=$CONFIG_DATA_DIR/ssh_callhome.xml -m ietf-netconf-server --datastore startup
echo "copying startup data to running data"
sysrepocfg -C startup

echo "Installation has been done "

if [ "x$1" == "x--server" ]; then

cp -rf $PWD/usr/local/bin/netopeer2-server $INSTALLER_BIN
fi

elif [ "x$1" == "x--client" ];then
echo " "
echo "Setting up client installation..."
cp -rf $PWD/usr/local/bin/netopeer2-cli $INSTALLER_BIN

echo "Copying rpc files in to /tmp/mplane/"
cp -rf $PWD/mplane/example $INSTALLER_DIR/ # && rm -rf $INSTALLER_DIR/mplane/example/
echo "Coppied"
else
echo "Incorrect First input Argument"
echo "To run as server please execute: ./install.sh --server"
echo "To run as client please execute: ./install.sh --client"
exit 0
fi
