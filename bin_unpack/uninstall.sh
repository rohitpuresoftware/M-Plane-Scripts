#!/bin/bash
if [ -d "/opt/PureSoftware/MP_3.0" ];then
	rm -rf /opt/PureSoftware/MP_3.0/
fi
rm -rf /usr/local/bin/netopeer2-*

rm -rf /usr/local/bin/sysrepo*

rm -rf /usr/local/bin/yang*

rm -rf /usr/local/include/libnetconf2 /usr/local/include/libssh  /usr/local/include/libyang  /usr/local/include/nc_client.h  /usr/local/include/nc_server.h  /usr/local/include/sysrepo  /usr/local/include/sysrepo.h  /usr/local/include/sysrepo_types.h
rm -rf /usr/local/man

rm -rf /usr/local/share/yang

rm -rf /usr/local/lib/libyang.so*

rm -rf /usr/local/lib/libssh.so*

rm -rf /usr/local/lib/libnetconf2.so*

rm -rf /usr/local/lib/libPS_MP.so

rm -rf /usr/local/lib/pkgconfig/

rm -rf /usr/local/lib/cmake
rm -rf /dev/shm/sr*
rm -rf /usr/local/lib/pkgconfig
rm -rf /usr/local/lib/libsysrepo.so*
rm -rf /usr/local/share/man/*
rm -rf /usr/local/lib/pkgconfig
#cp libruapp/app/subscribe_oran_apis.h /usr/local/include

#cp libruapp/lib/oran_apis.h /usr/local/include
