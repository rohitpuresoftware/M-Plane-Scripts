INSTALLER_DIR="/home/user/PureSoftware/MP_3.0"
INSTALLER_BIN=$INSTALLER_DIR/bin/
INSTALLER_LIB=$INSTALLER_DIR/lib/

export PATH="$PATH:$INSTALLER_BIN"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$INSTALLER_LIB"
echo $PATH

if [ "x$1" == "x--server" ]; then
    kill -9 `lsof -t -i:830`
   nohup netopeer2-server -d -v 3&
elif [ "x$1" == "x--client" ]; then
    if [ "x$3" == "x" ]; then
        netopeer2-cli -t 5
    else
        netopeer2-cli $2 $3
    fi
else
    echo "Incorrect First input Argument"
        echo "To run as server please execute: ./netopeer2_run.sh --server"
    echo "To run as client please execute: ./netopeer2_run.sh --client"
fi

