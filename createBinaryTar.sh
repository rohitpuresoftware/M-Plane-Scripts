#!/bin/bash
##################################################

export NETOPEER2_WORKSPACE=$PWD

if [[ $EUID -ne 0 ]]; then
   echo "Netopeer2 Needs super user previlage to build and install modules."
   echo "Please login as super user" 
   exit 1
fi


# Build the artifacts
bash $PWD/netopeer2_install.sh --server

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

cd $PWD/lib_ruapp
make
make objclean
cd $NETOPEER2_WORKSPACE

cp -rf $PWD/bin_unpack/* $PWD/installer/
mkdir -p $PWD/installer/mplane
cp -rf $PWD/lib_ruapp/user_rpcs $PWD/installer/mplane
cp -rf $PWD/lib_ruapp/oran_yang_model $PWD/installer/mplane
cp -rf $PWD/lib_ruapp/state_data_xml $PWD/installer/mplane
cp -rf $PWD/lib_ruapp/build $PWD/installer/mplane

# Write script here to copy your files in $PWD/installer/
# those will be packed in binary as it is.
# Install script to be written separately

tar -czvf binary_packed.tar.gz ./installer/ && rm -rf installer && echo "Please share the generated file 'binary_packed.tar.gz' as portable binary."

