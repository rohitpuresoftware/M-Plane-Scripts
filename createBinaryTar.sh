#!/bin/bash
##################################################

export NETOPEER2_WORKSPACE=$PWD

if [[ $EUID -ne 0 ]]; then
   echo "Netopeer2 Needs super user previlage to build and install modules."
   echo "Please login as super user" 
   exit 1
fi


echo "###########################################################"
echo "##              Building netopeer2 artifacts             ##"
echo "###########################################################"
bash $PWD/netopeer2_install.sh --server

echo "###########################################################"
echo "##                Installing the artifacts               ##"
echo "###########################################################"
rm -rf $PWD/installer/
mkdir $PWD/installer/

cd $PWD/libyang/build
make install DESTDIR=$PWD/../../installer/
cd $NETOPEER2_WORKSPACE

cd $PWD/libssh/build
make install DESTDIR=$PWD/../../installer/
cd $NETOPEER2_WORKSPACE

cd $PWD/sysrepo/build
make install DESTDIR=$PWD/../../installer/
cd $NETOPEER2_WORKSPACE


cd $PWD/libnetconf2/build
make install DESTDIR=$PWD/../../installer/
cd $NETOPEER2_WORKSPACE


cd $PWD/netopeer2/build
make install DESTDIR=$PWD/../../installer/
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
cp -rf $PWD/bin_unpack/* $PWD/installer/

echo "###########################################################"
echo "##      Copying user-rpc, state data & yang modules      ##"
echo "###########################################################"
mkdir -p $PWD/installer/mplane
cp -rf $PWD/libruapp/example $PWD/installer/mplane
cp -rf $PWD/libruapp/oran_yang_model $PWD/installer/mplane
cp -rf $PWD/libruapp/state_data_xml $PWD/installer/mplane

echo "###########################################################"
echo "##           Copying ruapp and ruapp library             ##"
echo "###########################################################"
cp -rf $PWD/libruapp/build $PWD/installer/mplane

# Write script here to copy your files in $PWD/installer/
# those will be packed in binary as it is.
# Install script to be written separately

echo "###########################################################"
echo "##           Packaging the binrary into tar.gz           ##"
echo "###########################################################"
tar -czvf binary_packed.tar.gz ./installer/ && rm -rf installer &&\
	echo "############################################################################################" &&\
	echo "## Please share the generated file 'binary_packed.tar.gz' as portable binary              ##" &&\
	echo "## To install, User need to untar this file and execute install.sh with --server/--client ##" &&\
	echo "## To run, user needs to execute run.sh with --server/--client                            ##" &&\
	echo "############################################################################################"

