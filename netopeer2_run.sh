#!/bin/bash
##################################################

if [ "x$1" == "x--server" ]; then
    netopeer2-server -d -v 3
elif [ "x$1" == "x--client" ]; then
    netopeer2-cli
else
    echo "Incorrect First input Argument"
	echo "To run as server please execute: ./netopeer2_run.sh --server"
    echo "To run as client please execute: ./netopeer2_run.sh --client"	
fi
