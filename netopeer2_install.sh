#!/bin/bash
##################################################
ORAN_MODULE_DIR=/tmp/mplane/oran_yang_model
export NETOPEER2_WORKSPACE=$PWD

if [[ $EUID -ne 0 ]]; then
   echo "Netopeer2 Needs super user previlage to build and install modules."
   echo "Please login as super user" 
   exit 1
fi

if [ "x$1" == "x--setup" ]; then
    export SWITCH="SETUP"
elif [ "x$1" == "x--build" ]; then
    export SWITCH="BUILD"
else
    echo "Incorrect First input Argument"
    echo "To build: ./netopeer2_install.sh --build"
    echo "To setup dev env: ./netopeer2_install.sh --setup"
    exit 0
fi

function install_specific_pkg()
{
    apt-get install -y $1
    if [[ $? > 0 ]]; then
        echo "The $1 module installation failed, exiting."
        exit
    else
        echo "The $1 module installation ran successfully."
	echo ""
    fi
}

#Basic Packages need to install.
###################################################
apt-get update -y
function install_pkgs()
{
    PKGS=(
	"git"
	"cmake"
	"build-essential"
	"libpcre3-dev"
	"libpcre3"
	"zlib1g-dev"
	"zlib1g"
	"libssl-dev"
	"openssh-client"
	"libpcre2-8-0"
	"libpcre2-dev"
    )
    for pkg in "${PKGS[@]}"; do
        if [ "1" = `dpkg-query -W -f='${Status}' $pkg 2>/dev/null | grep -c "ok installed"` ];then
            echo "$pkg already installed"
        else
            install_specific_pkg $pkg
        fi
    done
}

##################################################
function build_specific_submodule()
{
    if [ "OpenSSL" = $1 ];then
        if [  -n "$(uname -a | grep Ubuntu)" ]; then
            echo "OpenSSL is already present in Ubuntu. No Need to install"
        else
            echo "Install OpenSSL on ARM processors Manually"
        fi
    else
        echo "#########################################################"
        echo "                 Building $1"
        echo "#########################################################"
        cd $PWD/$1
        mkdir build
        cd build
        cmake ..
        make
	if [ "sysrepo" = $1 ];then
	make shm_clean
	fi
        make install
        ldconfig
        cd $NETOPEER2_WORKSPACE
        echo "Done"
        echo ""
        echo ""
    fi
}

function build_submodules()
{
<<<<<<< HEAD
   git submodule update --init --recursive
=======
   # git submodule update --init --recursive
>>>>>>> upgraded packages to v2 devel branch & not working modifying key-less list in operational data
    DEPS=(
        "libyang"
        "libssh"
        "OpenSSL"
        "sysrepo"
        "libnetconf2"
        "netopeer2"
    )
    for dep in "${DEPS[@]}"; do
        build_specific_submodule $dep
    done
}

function build_libruapp()
{
    cd $PWD/libruapp
    make
    make install
    make objclean
    ldconfig
    cd $NETOPEER2_WORKSPACE
    echo ""
    echo ""
}

function copy_files()
{
    echo ""
    mkdir -p /tmp/mplane
    echo "Copping user-rpcs example, o-ran yang modules and state data xml files"
    cp -r $PWD/libruapp/example /tmp/mplane/
    cp -r $PWD/libruapp/oran_yang_model /tmp/mplane/
    cp -r $PWD/libruapp/state_data_xml /tmp/mplane/
    echo "Done"
}

function install_yang_modules()
{
    echo ""
    echo ""
    echo "Installing the yang oran specific yang modules"
    $PWD/bin_unpack/oran_scripts/install_oran_yang_model.sh $ORAN_MODULE_DIR
    echo "Done"
    echo ""
}

case $SWITCH in
    SETUP)
        install_pkgs
        build_submodules
        build_libruapp
        copy_files
        install_yang_modules
        ;;
    BUILD)
        install_pkgs
        build_submodules
        build_libruapp
        ;;
    *)
        echo "Wrong option"
        ;;
esac

cd $NETOPEER2_WORKSPACE
echo " "

echo "#########################################################"
echo "Netopeer2 Installation successfully."
echo "Please use netopeer2_run.sh to start Server and Client"
echo "#########################################################"

