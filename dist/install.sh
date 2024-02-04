#!/bin/sh

LSMCD_HOME=/usr/local/lsmcd
TMP_DIR=/tmp/lsmcd
MODE=regular

# handle arguments
while getopts h:m: flag
do
    case "${flag}" in
        h) LSMCD_HOME=${OPTARG};;
        m) MODE=${OPTARG};;
    esac
done

#rm -rf $LSMCD_HOME

if [ ! -d ${LSMCD_HOME}/bin ]; then
    mkdir -p ${LSMCD_HOME}/bin
fi

if [ ! -d $TMP_DIR ]; then
    mkdir -p $TMP_DIR
fi

cp ../src/lsmcd  ${LSMCD_HOME}/bin/.
cp ../dist/bin/lsmcdctrl ${LSMCD_HOME}/bin/. 
if [ -d $LSMCD_HOME/conf ]; then
    UPDATE=1
else
    cp -rn ../dist/conf  ${LSMCD_HOME}/conf
    UPDATE=0
fi

if [ $MODE != docker ]; then
    if [ -x /sbin/chkconfig ]; then
        cp ../dist/bin/lsmcd.init /etc/init.d/lsmcd
        chkconfig lsmcd on
    elif [ -x /bin/systemctl ]; then
        cp ../dist/bin/lsmcd.service /etc/systemd/system/lsmcd.service
        systemctl enable lsmcd.service
    else
        echo "Distro not recognized, contact tech support"
        exit 1
    fi
fi

if [ $UPDATE -eq 0 ]; then
    echo "Now you have to update ${LSMCD_HOME}/conf/node.conf" 
else
    echo "Update completed"
fi

