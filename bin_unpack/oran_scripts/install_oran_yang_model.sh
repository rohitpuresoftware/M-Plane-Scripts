##!/usr/bin/env bash
#!/bin/bash
ORAN_MODULE_DIR=$1
ORAN_MODULE_PERMS=600
MODULES_OWNER=$(id -un)
MODULES_GROUP=$(id -gn)

# env variables ORAN_MODULE_DIR, ORAN_MODULE_PERMS must be defined and NP2_MODULE_OWNER, NP2_MODULE_GROUP will be used if
# defined when executing this script!
if [ -z "$ORAN_MODULE_DIR" -o -z "$ORAN_MODULE_PERMS" ]; then
    echo "Required environment variables not defined!"
    exit 1
fi

# optional env variable override
if [ -n "$SYSREPOCTL_EXECUTABLE" ]; then
    SYSREPOCTL="$SYSREPOCTL_EXECUTABLE"
# avoid problems with sudo PATH
elif [ `id -u` -eq 0 ]; then
    SYSREPOCTL=`su -c 'which sysrepoctl' -l $USER`
else
    SYSREPOCTL=`which sysrepoctl`
fi

MODDIR=${ORAN_MODULE_DIR}
PERMS=${ORAN_MODULE_PERMS}
OWNER=${NP2_MODULE_OWNER}
GROUP=${NP2_MODULE_GROUP}

# array of modules to install

MODULES=(
"iana-hardware.yang"
"ietf-hardware.yang"
"o-ran-hardware.yang"		
"o-ran-wg4-features.yang"	
"o-ran-file-management.yang"	
"o-ran-operations.yang"		
"mplane-rpcs@2021-06-21.yang"
"o-ran-ald-port.yang"	
"o-ran-ald.yang"	
"iana-if-type.yang"
"o-ran-interfaces.yang"		
"o-ran-compression-factors.yang"
"o-ran-module-cap.yang"
"o-ran-troubleshooting.yang"
"o-ran-trace.yang"	
"ietf-dhcpv6-types.yang"
"o-ran-dhcp.yang"	
"o-ran-delay-management.yang"
"o-ran-supervision.yang"	
"o-ran-fan.yang"	
"o-ran-sync.yang"	
"o-ran-software-management.yang"	
"o-ran-fm.yang"		
"o-ran-mplane-int.yang"	
"o-ran-ethernet-forwarding.yang" 
"o-ran-processing-element.yang"
"o-ran-beamforming.yang"
"o-ran-transceiver.yang" 
"o-ran-externalio.yang" 
"o-ran-laa.yang"
"o-ran-antenna-calibration.yang"
"o-ran-lbm.yang"
"o-ran-udp-echo.yang"
"o-ran-ecpri-delay.yang"
"o-ran-performance-management.yang"
"o-ran-ves-subscribed-notifications.yang"	
"o-ran-laa-operations.yang"
)

#"o-ran-uplane-conf.yang"
# functions
INSTALL_MODULE() {
    CMD="'$SYSREPOCTL' -a -i $MODDIR/$1 -s '$MODDIR' -p '$PERMS' -v2"
    if [ ! -z ${OWNER} ]; then
        CMD="$CMD -o '$OWNER'"
    fi
    if [ ! -z ${GROUP} ]; then
        CMD="$CMD -g '$GROUP'"
    fi
    eval $CMD
    local rc=$?
    if [ $rc -ne 0 ]; then
        exit $rc
    fi
}

UPDATE_MODULE() {
echo "in UPDATE_MODULE"

    CMD="'$SYSREPOCTL' -a -U $MODDIR/$1 -s '$MODDIR' -p '$PERMS' -v2"
    if [ ! -z ${OWNER} ]; then
        CMD="$CMD -o '$OWNER'"
    fi
    if [ ! -z ${GROUP} ]; then
        CMD="$CMD -g '$GROUP'"
    fi
    eval $CMD
    local rc=$?
    if [ $rc -ne 0 ]; then
        exit $rc
    fi
}

ENABLE_FEATURE() {
    "$SYSREPOCTL" -a -c $1 -e $2 -v2
    local rc=$?
    if [ $rc -ne 0 ]; then
        exit $rc
    fi
}

# get current modules
SCTL_MODULES=`$SYSREPOCTL -l`


for i in "${MODULES[@]}"; do

    name=`echo "$i" | sed 's/\([^@]*\).*/\1/'`
	
	name=`echo ${name%%.*}`	
	
    SCTL_MODULE=`echo "$SCTL_MODULES" | grep "^$name \+|[^|]*| I "`

    if [ ! -z "$SCTL_MODULE" ]; then
	echo "uninstalling $name"
	$SYSREPOCTL -u $name
	fi
done

for j in "${MODULES[@]}"; do
    echo "Installing $j"
    name=`echo "$j" | sed 's/\([^@]*\).*/\1/'`
	
	name=`echo ${name%%.*}`	
	
    SCTL_MODULE=`echo "$SCTL_MODULES" | grep "^$name \+|[^|]*| I "`

    if [ -z "$SCTL_MODULE" ]; then
        # install module with all its features
        INSTALL_MODULE "$j"
       # continue
    fi
	
			

#    sctl_revision=`echo "$SCTL_MODULE" | sed 's/[^|]*| \([^ ]*\).*/\1/'`
#   revision=`echo "$i" | sed 's/[^@]*@\([^\.]*\).*/\1/'`
#  if [ "$sctl_revision" \< "$revision" ]; then
        # update module without any features
#     file=`echo "$i" | cut -d' ' -f 1`
#    UPDATE_MODULE "$file"
#    fi

    # parse sysrepoctl features and add extra space at the end for easier matching
    sctl_features="`echo "$SCTL_MODULE" | sed 's/\([^|]*|\)\{6\}\(.*\)/\2/'` "
    # parse features we want to enable
    features=`echo "$i" | sed 's/[^ ]* \(.*\)/\1/'`
    while [ "${features:0:3}" = "-e " ]; do
        # skip "-e "
        features=${features:3}
        # parse feature
        feature=`echo "$features" | sed 's/\([^[:space:]]*\).*/\1/'`

        # enable feature if not already
        sctl_feature=`echo "$sctl_features" | grep " ${feature} "`
        if [ -z "$sctl_feature" ]; then
            # enable feature
            ENABLE_FEATURE $name $feature
        fi

        # next iteration, skip this feature
        features=`echo "$features" | sed 's/[^[:space:]]* \(.*\)/\1/'`
    done
done
