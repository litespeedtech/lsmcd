LiteSpeed Memcache Compatible Persistent HA replication Cache Server 
=======

Compile
--------
- git clone https://github.com/litespeedtech/lsmcd.git or download project via "wget https://github.com/litespeedtech/lsmcd/archive/master.zip"
- cd lsmcd directory and run configure and make.   
  ./configure CFLAGS=" -O3" CXXFLAGS=" -O3"  
  make

Install
--------
- cd dist, run "install.sh" so that lsmcd service is added.
- update the default configure "/usr/local/lsmcd/conf/node.conf".  
  There is example.conf under dist/conf, just replace with your testing IP.
- now start or stop it.  
  service lsmcd start/stop

Configuration Documentation
--------

Lsmcd configuration setting is different from memcached. For detail description, please read through https://www.litespeedtech.com/support/wiki/doku.php/litespeed_wiki:lsmcd.
