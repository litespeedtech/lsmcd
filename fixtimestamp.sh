#When using git clone to get the source code, the timestamp will be lost.
#then when run make, it will cause re-config issue.

#!/bin/sh


touch  aclocal.m4
sleep 2

touch Makefile.in
touch src/Makefile.in
touch src/lcrepl/Makefile.in
touch src/repl/Makefile.in
touch src/memcache/Makefile.in
touch src/edio/Makefile.in
touch src/log4cxx/Makefile.in
touch src/socket/Makefile.in
touch src/shm/Makefile.in
touch src/util/Makefile.in
touch src/util/sysinfo/Makefile.in
touch src/lsr/Makefile.in

sleep 2 

touch configure
touch src/config.h.in
