#!/bin/bash

ORAN_MODULE_DIR="/opt/PureSoftware/MP_3.0/yang_models"
INSTALLER_DIR="/opt/PureSoftware/MP_3.0"
INSTALLER_BIN=$INSTALLER_DIR/bin/
INSTALLER_LIB=$INSTALLER_DIR/lib/
INSTALLER_INCLUDE=$INSTALLER_DIR/include/
INSTALLER_SHARE=$INSTALLER_DIR/share/
CONFIG_DATA_DIR=$INSTALLER_DIR/config_data_xml
#INSTALLER_DIR=$PWD


if [ -f "$INSTALLER_DIR/installation_log.txt" ];then
rm $INSTALLER_DIR/installation_log.txt

fi

if [ "x$1" == "x--server" ]; then

	$PWD/uninstall.sh
	if [ ! -d "/opt/PureSoftware/MP_3.0/yang_models" ] ;then
	mkdir -p /opt/PureSoftware/MP_3.0/yang_models && echo "creating /opt/PureSoftware/MP_3.0/yang_models directory"
	fi

	if [ ! -d "/opt/PureSoftware/MP_3.0/bin" ] ;then
	mkdir -p /opt/PureSoftware/MP_3.0/bin && echo "creating /opt/PureSoftware/MP_3.0/bin directory"
	fi


	if [ ! -d "/opt/PureSoftware/MP_3.0/lib" ] ;then
	mkdir -p /opt/PureSoftware/MP_3.0/lib && echo "creating /opt/PureSoftware/MP_3.0/lib directory"
	fi

	if [ ! -d "/opt/PureSoftware/MP_3.0/include" ] ;then
	mkdir -p /opt/PureSoftware/MP_3.0/include && echo "creating /opt/PureSoftware/MP_3.0/include directory"
	fi

	if [ ! -d "/opt/PureSoftware/MP_3.0/share" ] ;then
	mkdir -p /opt/PureSoftware/MP_3.0/share && echo "creating /opt/PureSoftware/MP_3.0/share directory"
	fi

	if [ ! -d "/opt/PureSoftware/MP_3.0/sysrepo" ] ;then
	mkdir -p /opt/PureSoftware/MP_3.0/sysrepo && echo "creating /opt/PureSoftware/MP_3.0/sysrepo directory"
	fi


apt-get update && apt-get install -y openssl libssl-dev vim python3-pip libpcre2-8-0 libpcre2-dev


echo "Copying library and headers "

cp -rf $PWD/usr/local/bin/* $INSTALLER_BIN


cp -rf $PWD/usr/local/include/* $INSTALLER_INCLUDE 

ln -srf $INSTALLER_DIR/include/* -t /usr/local/include/
cp -rf $PWD/usr/local/lib/* $INSTALLER_LIB 

cp -rf $PWD/mplane/build/libPS_MP.a $INSTALLER_LIB
ln -srf $INSTALLER_DIR/lib/* -t /usr/local/lib/
cp -rf $PWD/usr/local/share/* $INSTALLER_SHARE 

ln -srf $INSTALLER_DIR/share/* -t /usr/local/share/
cp -rf $PWD/usr/local/bin/netopeer* $INSTALLER_BIN

cp -rf $PWD/mplane/build/ruapp $INSTALLER_BIN
ln -srf $INSTALLER_DIR/bin/* -t /usr/local/bin/
echo "Copying oran specific yang modules"
cp -rf $PWD/mplane/oran_yang_model/* $ORAN_MODULE_DIR  #&& rm -rf $PWD/mplane/oran_yang_model

ldconfig &> $INSTALLER_DIR/installation_log.txt

echo "Setting up first time server installation... (do not repeat)" 


echo "Copy state data xml"
cp -rf $PWD/mplane/state_data_xml $INSTALLER_DIR
echo "Copy Config data xml"
cp -rf $PWD/mplane/config_data_xml $INSTALLER_DIR
echo "Installing ruapp and library"

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
sysrepocfg --copy-from=$CONFIG_DATA_DIR/del_mgmt.xml --module o-ran-delay-management --datastore running

fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-mplane-int"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-mplane-int"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/mplane_int.xml --module o-ran-mplane-int --datastore running
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "ietf-interfaces"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring ietf-interfaces"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/ietf_interef.xml --module ietf-interfaces --datastore running
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-processing-element"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-processing-element"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/proc_el.xml --module o-ran-processing-element --datastore running
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "o-ran-uplane-conf"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring o-ran-uplane-conf"
sysrepocfg --copy-from=$CONFIG_DATA_DIR/uplane_conf.xml --module o-ran-uplane-conf --datastore running
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "foxconn-sfp"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring foxconn-sfp"
sysrepocfg --edit=$CONFIG_DATA_DIR/foxconn-sfp-rw.xml -m foxconn-sfp --datastore running
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "foxconn-system"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring foxconn-system"
sysrepocfg --edit=$CONFIG_DATA_DIR/foxconn-system-rw.xml -m  foxconn-system --datastore running
fi


SCTL_MODULE=`echo "$SCTL_MODULES" | grep "foxconn-operations"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring foxconn-operations"
sysrepocfg --edit=$CONFIG_DATA_DIR/foxconn-oper-rw.xml -m foxconn-operations --datastore running
fi

SCTL_MODULE=`echo "$SCTL_MODULES" | grep "ietf-netconf-server"`
if [ ! -z "$SCTL_MODULE" ];then
echo "configuring ietf-netconf-server"
sysrepocfg --edit=$CONFIG_DATA_DIR/ssh_callhome.xml -m ietf-netconf-server --datastore running
fi

elif [ "x$1" == "x--client" ];then
echo " "
echo "Setting up client installation..."
cp -rf $PWD/usr/local/bin/netopeer2-cli $INSTALLER_BIN
ln -srf $INSTALLER_DIR/bin/netopeer2-cli /usr/local/bin/

echo "Copying rpc files in to /tmp/mplane/"
cp -rf $PWD/mplane/example $INSTALLER_DIR/ # && rm -rf $INSTALLER_DIR/mplane/example/
echo "Coppied"
else
echo "Incorrect First input Argument"
echo "To run as server please execute: ./install.sh --server"
echo "To run as client please execute: ./install.sh --client"
exit 0
fi
