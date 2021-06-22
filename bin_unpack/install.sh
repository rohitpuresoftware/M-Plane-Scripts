apt-get update && apt-get install -y openssl libssl-dev vim

echo "Copying library and headers"
cp -rf ./usr/local/bin/sysrepo* /usr/local/bin/
cp -rf ./usr/local/bin/yang* /usr/local/bin/
cp -rf ./usr/local/include/* /usr/local/include/
cp -rf ./usr/local/lib/* /usr/local/lib/
cp -rf ./usr/local/include/* /usr/local/include/
cp -rf ./usr/local/share/* /usr/local/share/

echo "Copying oran specific yang modules"
cp -rf ./mplane/oran_yang_model /usr/local/share/yang/modules/netopeer2/ && rm -rf ./mplane/oran_yang_model
mkdir -p /tmp/mplane

ldconfig &> /dev/null

if [ "x$1" == "x--server" ]; then
    echo "Setting up first time server installation... (Should not be repeasted)"
    cp -rf ./usr/local/bin/netopeer2-server /usr/local/bin/netopeer2-server
    echo "Copy state data xml"
    cp -rf ./mplane/state_data_xml /tmp/mplane/
    echo "Installing ruapp and library"
    cp -rf ./mplane/build/ruapp /usr/local/bin/
    cp -rf ./mplane/build/libruapp.so /usr/lib/
    echo "Running netopeer2 setup scripts"
    ./netopeer2_scripts/setup.sh && ./netopeer2_scripts/merge_hostkey.sh && ./netopeer2_scripts/merge_config.sh && rm -rf ./netopeer2_scripts
    echo "Running o-ran yang installation scripts"
    ./oran_scripts/install_oran_yang_model.sh && rm -rf ./oran_scripts
    #echo 7 > /proc/sys/kernel/printk
    #echo 1 > /sys/bus/pci/rescan
    #insmod /lib/modules/4.19.90-rt35/extra/yami.ko scratch_buf_size=0x20000000 scratch_buf_phys_addr=0x2360000000
    #source /usr/local/dpdk/dpaa2/dynamic_dpl.sh dpmac.5 dpmac.3
elif [ "x$1" == "x--client" ]; then
    echo " "
    echo "Setting up client installation..."
    cp -rf ./usr/local/bin/netopeer2-cli /usr/local/bin/netopeer2-cli
    echo "Copping rpc files in to /tmp/mplane/"
    cp -rf ./mplane/example /tmp/mplane/ && rm -rf ./example/
    echo "Coppied"
else
    echo "Incorrect First input Argument"
    echo "To run as server please execute: ./install.sh --server"
    echo "To run as client please execute: ./install.sh --client"
    exit 0
fi
