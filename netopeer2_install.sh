#!/bin/bash
##################################################

export NETOPEER2_WORKSPACE=$PWD

if [[ $EUID -ne 0 ]]; then
   echo "Netopeer2 Needs super user previlage to build and install modules."
   echo "Please login as super user" 
   exit 1
fi

if [ "x$1" == "x--server" ]; then
    export NTPR2_SERVER=1
elif [ "x$1" == "x--client" ]; then
    export NTPR2_CLIENT=1
    # Do nothing, just force user to know what he is running
else
    echo "Incorrect First input Argument"
        echo "To run as server please execute: ./netopeer2_install.sh --server"
    echo "To run as client please execute: ./netopeer2_install.sh --client"
    exit 0
fi

#Basic Packages need to install.
###################################################
apt-get update -y

apt-get install -y git
if [[ $? > 0 ]]
then
    echo "The git module installation failed, exiting."
    exit
else
    echo "The git module installation ran successfully, continuing with script."
fi

apt-get install -y cmake
if [[ $? > 0 ]]
then
    echo "The cmake module installation failed, exiting."
    exit
else
    echo "The cmake module installation ran successfully, continuing with script."
fi
apt-get install -y build-essential
if [[ $? > 0 ]]
then
    echo "The build-essential module installation failed, exiting."
    exit
else
    echo "The build-essential module installation ran successfully, continuing with script."
fi
apt-get install -y libpcre3-dev libpcre3
if [[ $? > 0 ]] 
then
    echo "The libpcre3-dev libpcre3 module installation failed, exiting."
    exit
else
    echo "The libpcre3-dev libpcre3 installation ran successfully, continuing with script."
fi
apt-get install -y zlib1g-dev
if [[ $? > 0 ]]
then
    echo "The zlib1g-dev module installation failed, exiting."
    exit
else
    echo "The zlib1g-dev installation ran successfully, continuing with script."
fi
apt-get install -y zlib1g
if [[ $? > 0 ]]
then
    echo "The zlib1g module installation failed, exiting."
    exit
else
    echo "The zlib1g module installation ran successfully, continuing with script."
fi
apt-get install -y libssl-dev
if [[ $? > 0 ]]
then
    echo "libssl-devmodule failed, exiting."
    exit
else
    echo "The libssl-dev module installation ran successfully, continuing with script."
fi

##################################################

#Core Dependencies
echo "Downloading LibYang Module.."
echo " "
echo " "
git clone https://github.com/CESNET/libyang.git
cd $PWD/libyang
mkdir build
cd build
cmake ..
make
make install
cd $NETOPEER2_WORKSPACE

echo "Downloading LibSSH Module.."
echo " "
echo " "
git clone http://git.libssh.org/projects/libssh.git
cd $PWD/libssh
mkdir build
cd build
cmake ..
make
make install
cd $NETOPEER2_WORKSPACE

echo "Downloading OpenSSL Module.."
echo " "
if [  -n "$(uname -a | grep Ubuntu)" ]; then
    echo "OpenSSL is already present in Ubuntu. No Need to install" 
else
    echo "Install OpenSSL on ARM processors Manually"
fi  

echo "Downloading sysrepo Module.."
echo " "
echo " "
git clone https://github.com/sysrepo/sysrepo.git
cd $PWD/sysrepo
mkdir build
cd build
make sr_clean
cmake ..
make
make install
ldconfig
cd $NETOPEER2_WORKSPACE


echo "Downloading libnetconf2 Module.."
echo " "
echo " "
git clone https://github.com/CESNET/libnetconf2.git
cd $PWD/libnetconf2
mkdir build
cd build
cmake ..
make
make install
cd $NETOPEER2_WORKSPACE


echo "Downloading neetopeer2 Module.."
echo " "
echo " "

export NETOPEER2_WORKSPACE=$PWD

git clone https://github.com/odidev/netopeer2.git
cd $PWD/netopeer2
mkdir build
cd build
cmake ..
make
make install
ldconfig

echo " "
echo " "
echo "Setting up server and client installation..."
if [[ -z $NTPR2_SERVER ]]; then
    echo " "
    echo "Copping rpc files in to /tmp/"
    cp ../mpra_src/user_rpcs/* /tmp/
    echo "Coppied"
fi


#if [[ ! -z `sysrepoctl -l | grep aircond | cut -d " " -f 1` ]]; then
#    echo " "
#    echo "Un installing the existing yang model"
#    sysrepoctl -u aircond
#    echo "Un installed"
#fi

echo " "
echo "Installing yang models"
for yangfile in $NETOPEER2_WORKSPACE/netopeer2/mpra_src/yang_model/*; do
    sysrepoctl -i ${yangfile}
done
echo "Installed"

cd $NETOPEER2_WORKSPACE

echo " "


echo "#########################################################"
echo "Netopeer2 Installation successfully."
echo "Please use netopeer2_run.sh to start Server and Client"
echo "#########################################################"

