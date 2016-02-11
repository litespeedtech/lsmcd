#!/bin/sh

#cd `dirname "$0"`

#If install.sh in admin/misc, need to change directory
#LSINSTALL_DIR=`dirname "$0"`
#cd $LSINSTALL_DIR/


LSCA_HOME=/usr/local/lsmcd

rm -rf $LSCA_HOME/bin/*

cp ../src/lsmcd $LSCA_HOME/bin/.
cp -r ../dist/bin/lsmcdctrl $LSCA_HOME/bin/. 
cp -r ../dist/conf  $LSCA_HOME/bin/conf

echo "Now you have to update LSCA_HOME/bin/conf/node.conf" 

