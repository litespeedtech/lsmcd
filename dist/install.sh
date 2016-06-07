#!/bin/sh

#cd `dirname "$0"`

#If install.sh in admin/misc, need to change directory
#LSINSTALL_DIR=`dirname "$0"`
#cd $LSINSTALL_DIR/
set -x

LSCA_HOME=/usr/local/lsmcd

rm -rf $LSCA_HOME

if [ ! -d ${LSCA_HOME}/bin ]; then
    mkdir -p ${LSCA_HOME}/bin
fi

cp ../src/lsmcd  ${LSCA_HOME}/bin/.
cp ../dist/bin/lsmcdctrl ${LSCA_HOME}/bin/. 
cp -r ../dist/conf  ${LSCA_HOME}/conf
cp ../dist/bin/lsmcd.init /etc/init.d/lsmcd
chkconfig lsmcd on

echo "Now you have to update ${LSCA_HOME}/conf/example.conf" 

