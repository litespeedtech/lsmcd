#!/bin/sh

LSMCD_HOME=/usr/local/lsmcd
TMP_DIR=/tmp/lsmcd

rm -rf $LSMCD_HOME

if [ ! -d ${LSMCD_HOME}/bin ]; then
    mkdir -p ${LSMCD_HOME}/bin
fi

if [ ! -d $TMP_DIR ]; then
    mkdir -p $TMP_DIR
fi

cp ../src/lsmcd  ${LSMCD_HOME}/bin/.
cp ../dist/bin/lsmcdctrl ${LSMCD_HOME}/bin/. 
cp -r ../dist/conf  ${LSMCD_HOME}/conf

cp ../dist/bin/lsmcd.init /etc/init.d/lsmcd
chkconfig lsmcd on

echo "Now you have to update ${LSMCD_HOME}/conf/node.conf" 

