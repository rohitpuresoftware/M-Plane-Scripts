#!/bin/bash
##################################################

export NETOPEER2_WORKSPACE=$PWD
RELEASE_PACKAGE="PS_MP_V3.0"

if [[ $EUID -ne 0 ]]; then
   echo "Netopeer2 Needs super user previlage to build and install modules."
   echo "Please login as super user" 
   exit 1
fi


echo "###########################################################"
echo "##              Building netopeer2 artifacts             ##"
echo "###########################################################"
bash $PWD/netopeer2_install.sh --build

echo "###########################################################"
echo "##                Installing the artifacts               ##"
echo "###########################################################"
rm -rf $PWD/$RELEASE_PACKAGE
mkdir $PWD/$RELEASE_PACKAGE

cd $PWD/libyang/build
make install DESTDIR=$NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/
cd $NETOPEER2_WORKSPACE

cd $PWD/libssh/build
make install DESTDIR=$NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/
cd $NETOPEER2_WORKSPACE

cd $PWD/sysrepo/build
make install DESTDIR=$NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/
cd $NETOPEER2_WORKSPACE


cd $PWD/libnetconf2/build
make install DESTDIR=$NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/
cd $NETOPEER2_WORKSPACE


cd $PWD/netopeer2/build
make install DESTDIR=$NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/
cp $PWD/netopeer2-cli $NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/usr/local/bin/
chmod 777 $NETOPEER2_WORKSPACE/$RELEASE_PACKAGE/usr/local/bin/netopeer2-cli
cd $NETOPEER2_WORKSPACE

echo "###########################################################"
echo "##                   Building libruapp                   ##"
echo "###########################################################"
cd $PWD/libruapp
make
make objclean
cd $NETOPEER2_WORKSPACE


echo "###########################################################"
echo "##               Copying packaging scripts               ##"
echo "###########################################################"
cp -rf $PWD/bin_unpack/* $PWD/$RELEASE_PACKAGE

echo "###########################################################"
echo "##      Copying user-rpc, state data & yang modules      ##"
echo "###########################################################"
mkdir -p $PWD/$RELEASE_PACKAGE/mplane
cp -rf $PWD/libruapp/example $PWD/$RELEASE_PACKAGE/mplane
cp -rf $PWD/libruapp/oran_yang_model $PWD/$RELEASE_PACKAGE/mplane
cp -rf $PWD/libruapp/state_data_xml $PWD/$RELEASE_PACKAGE/mplane

echo "###########################################################"
echo "##           Copying ruapp and ruapp library             ##"
echo "###########################################################"
cp -rf $PWD/libruapp/build $PWD/$RELEASE_PACKAGE/mplane

# Write script here to copy your files in $PWD/$RELEASE_PACKAGE/
# those will be packed in binary as it is.
# Install script to be written separately

echo "###########################################################"
echo "##           Packaging the binrary into tar.gz           ##"
echo "###########################################################"
tar -czvf PS_MP_V3.0.tar.gz $RELEASE_PACKAGE && rm -rf $RELEASE_PACKAGE &&\
	echo "############################################################################################" &&\
	echo "## Please share the generated file 'PS_MP_V3.0.tar.gz' as portable binary              ##" &&\
	echo "## To install, User need to untar this file and execute install.sh with --server/--client ##" &&\
	echo "## To run, user needs to execute run.sh with --server/--client                            ##" &&\
	echo "############################################################################################"

