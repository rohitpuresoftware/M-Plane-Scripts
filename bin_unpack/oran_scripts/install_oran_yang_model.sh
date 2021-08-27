#!/usr/bin/env bash

ORAN_MODULE_DIR="/home/M-Plane-Scripts/libruapp/oran_yang_model"
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
"iana-hardware@2018-03-13.yang"
"ietf-hardware@2018-03-13.yang"
"o-ran-wg4-features@2020-12-10.yang"
"o-ran-hardware@2020-12-10.yang"
"ietf-netconf-acm@2018-02-14.yang"
"ietf-crypto-types@2019-07-02.yang"
"o-ran-file-management@2019-07-03.yang"
"o-ran-operations@2020-12-10.yang"
"mplane-rpcs@2021-06-21.yang"
"o-ran-ald-port@2019-07-03.yang"
"o-ran-ald@2019-07-03.yang"
"o-ran-ves-subscribed-notifications@2020-12-10.yang"
"iana-if-type@2017-01-19.yang"
"o-ran-interfaces@2020-12-10.yang"
"o-ran-processing-element@2020-04-17.yang"
"o-ran-ecpri-delay@2019-02-04.yang"
"o-ran-compression-factors@2020-08-10.yang"
"o-ran-module-cap@2020-12-10.yang"
"o-ran-troubleshooting@2019-02-04.yang"
"o-ran-trace@2019-07-03.yang"
"ietf-dhcpv6-types@2018-09-04.yang"
"o-ran-dhcp@2020-12-10.yang"
"o-ran-delay-management@2020-08-10.yang"
"o-ran-supervision@2020-12-10.yang"
"o-ran-performance-management@2020-12-10.yang"
"o-ran-sync@2020-08-10.yang"
"o-ran-software-management@2019-07-03.yang"
"o-ran-uplane-conf@2020-12-10.yang"
"o-ran-fan@2019-07-03.yang"
"o-ran-fm@2019-02-04.yang"
)

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
    echo "Installing $i"
    name=`echo "$i" | sed 's/\([^@]*\).*/\1/'`

    SCTL_MODULE=`echo "$SCTL_MODULES" | grep "^$name \+|[^|]*| I"`
    if [ -z "$SCTL_MODULE" ]; then
        # install module with all its features
        INSTALL_MODULE "$i"
        continue
    fi

    sctl_revision=`echo "$SCTL_MODULE" | sed 's/[^|]*| \([^ ]*\).*/\1/'`
    revision=`echo "$i" | sed 's/[^@]*@\([^\.]*\).*/\1/'`
    if [ "$sctl_revision" \< "$revision" ]; then
        # update module without any features
        file=`echo "$i" | cut -d' ' -f 1`
        UPDATE_MODULE "$file"
    fi

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
