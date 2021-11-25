#!/bin/bash

ORAN_MODULE_DIR="/opt/PureSoftware/MP_3.0/yang_models"
INSTALLER_DIR="/opt/PureSoftware/MP_3.0"
INSTALLER_BIN=$INSTALLER_DIR/bin/
INSTALLER_LIB=$INSTALLER_DIR/lib/
INSTALLER_INCLUDE=$INSTALLER_DIR/include/
INSTALLER_SHARE=$INSTALLER_DIR/share/

#INSTALLER_DIR=$PWD

if [ -f "$INSTALLER_DIR/installation_log.txt" ];then
rm $INSTALLER_DIR/installation_log.txt

fi

if [ "x$1" == "x--server" ]; then

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

if [ -f "/usr/local/bin/netopeer2-server" ];then
	set --  $(/usr/local/bin/netopeer2-server -V) 

	netopeer2_version=$2 

	echo "version trying to install ">> $INSTALLER_DIR/installation_log.txt
	set -- $($PWD/usr/local/bin/netopeer2-server -V)

	new_netopeer2_version=$2
fi


if [ ! -z "$netopeer2_version" ] ;then

	if  [[ "x$netopeer2_version" == "x$new_netopeer2_version" ]]  ;then

		echo "Already newest version available want to continue y/n : "

		read input

		echo "Already newest version of netopeer2 available ">> $INSTALLER_DIR/installation_log.txt

		echo  >> $netopeer2_version >> $INSTALLER_DIR/installation_log.txt

		echo "For more details check for $INSTALLER_DIR/installation_log.txt"

		if [ "$input" == "n" ];then
			exit 0
		fi
	fi
fi

apt-get update && apt-get install -y openssl libssl-dev vim python3-pip libpcre2-8-0 libpcre2-dev


echo "Copying library and headers "

cp -rf $PWD/usr/local/bin/* $INSTALLER_BIN


cp -rf $PWD/usr/local/include/* $INSTALLER_INCLUDE 

ln -srf $INSTALLER_DIR/include/* -t /usr/local/include/
cp -rf $PWD/usr/local/lib/* $INSTALLER_LIB 

cp -rf $PWD/mplane/build/libPS_MP.so $INSTALLER_LIB
ln -srf $INSTALLER_DIR/lib/* -t /usr/local/lib/
#rm /usr/lib/libPS_MP.so 
ln -srf $INSTALLER_DIR/lib/libPS_MP.so -t /usr/lib/
cp -rf $PWD/usr/local/share/* $INSTALLER_SHARE 

ln -srf $INSTALLER_DIR/share/* -t /usr/local/share/
cp -rf $PWD/usr/local/bin/netopeer* $INSTALLER_BIN

cp -rf $PWD/mplane/build/ruapp $INSTALLER_BIN
ln -srf $INSTALLER_DIR/bin/* -t /usr/local/bin/
echo "Copying oran specific yang modules"
cp -rf $PWD/mplane/oran_yang_model/* $ORAN_MODULE_DIR  #&& rm -rf $PWD/mplane/oran_yang_model

ldconfig &> $INSTALLER_DIR/installation_log.txt

echo "Setting up first time server installation... (Should not be repeat)" >> $INSTALLER_DIR/installation_log.txt


echo "Copy state data xml"
cp -rf $PWD/mplane/state_data_xml $INSTALLER_DIR
echo "Installing ruapp and library"

echo "Running netopeer2 setup scripts"
$PWD/netopeer2_scripts/setup.sh && $PWD/netopeer2_scripts/merge_hostkey.sh && $PWD/netopeer2_scripts/merge_config.sh #&& rm -rf $INSTALLER_DIR/netopeer2_scripts


echo "Running o-ran yang installation scripts"
$PWD/oran_scripts/install_oran_yang_model.sh $ORAN_MODULE_DIR #&& rm -rf $INSTALLER_DIR/oran_scripts
sysrepoctl -i $ORAN_MODULE_DIR/o-ran-usermgmt.yang && sysrepocfg -W $INSTALLER_DIR/state_data_xml/o-ran-user.xml -m o-ran-usermgmt -f "xml"
sysrepoctl -c  o-ran-wg4-features -e STATIC-TRANSMISSION-WINDOW-CONTROL
#echo 7 > /proc/sys/kernel/printk
#echo 1 > /sys/bus/pci/rescan
#insmod /lib/modules/4.19.90-rt35/extra/yami.ko scratch_buf_size=0x20000000 scratch_buf_phys_addr=0x2360000000
#source /usr/local/dpdk/dpaa2/dynamic_dpl.sh dpmac.5 dpmac.3
#fi
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
