#!/bin/bash

# bash script for quick set up LiteSpeed Memcached with SASL support on cPanel system

if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root"
    exit 1
fi

UBUNTU="0"
if cat /etc/*release | grep -q "CentOS Linux 7" ; then
    echo -e "\nDetecting CentOS 7.x...\n"
elif cat /etc/*release | grep -q "Ubuntu" ; then
    echo -e "\nDetecting Ubuntu...\n"
    UBUNTU="1"
elif cat /etc/*release | grep -q "CloudLinux 7" ; then
    echo -e "\nDetecting CloudLinux 7.x...\n"
elif cat /etc/*release | grep -q "CloudLinux" ; then
    echo -e "\nDetecting CloudLinux ...\n"
    dnf config-manager --set-enabled cloudlinux-PowerTools > /dev/null 2>&1
    dnf config-manager --set-enabled PowerTools > /dev/null 2>&1
elif cat /etc/*release | grep -qi "CentOS" ; then
    echo -e "\nDetecting CentOS ...\n"
    dnf config-manager --set-enabled PowerTools > /dev/null 2>&1
    dnf config-manager --set-enabled powertools > /dev/null 2>&1
else
    echo "This script only supports CentOS, CloudLinux and Ubuntu system!"
    exit 1
fi

if [[ ! -d /usr/local/cpanel ]] ; then
  echo "cPanel not detected..."
  exit
fi

create_sasl_user() {

if [[ -f /etc/sasllsmcd ]] ; then
user_list=$(sasldblistusers2 /etc/sasllsmcd | cut -d@ -f1)
#get current user list
fi

for name in $(ls /home/);
do
  if [[ -d /home/$name/public_html ]] ; then
  #check public_html existance to make sure it's vhost user instead of cPanel created dir
        if ! echo "$user_list" | grep -i -q "$name" ; then
            #check if user already in the list to avoid override existing users
            passwd=$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 10 ; echo '')
            echo "$passwd" | saslpasswd2 -p -f /etc/sasllsmcd "$name"
            # use -p to set a random password without prompt
            echo "$name added into LSMCD"
        else
            echo "$name already in the list..."
        fi
  fi
done
sudo chown nobody:nobody /etc/sasllsmcd
sudo chmod 600 /etc/sasllsmcd
echo -e "\n\n\nthe password generated is random and not usable , user will need to go to cPanel LSMCD manager to reset a new password"

}

install_lsmcd() {
if [ $UBUNTU = "1" ]; then
  apt-get install git build-essential zlib1g-dev libexpat1-dev openssl libssl-dev libsasl2-dev libpcre3-dev sasl2-bin -y
else
  if ! yum groupinstall -y "Development Tools" ; then
    echo "yum install failed, do you have root privilege ?"
    exit 1
  fi

  if ! yum install -y autoconf automake zlib-devel openssl-devel expat-devel pcre-devel libmemcached-devel cyrus-sasl* ; then
    echo "yum install failed, do you have root privilege ?"
    exit 1
  fi
fi

if ! mkdir -p /home/lsmcd_tmp ; then
  echo "failed to create temp dir, exit"
  exit 1
fi

if ! cd /home/lsmcd_tmp ; then
  echo "failed to enter temp dir, exit"
  exit 1
fi

git clone https://github.com/litespeedtech/lsmcd.git

if ! cd lsmcd ; then
  echo "failed to enter temp dir, exit"
  exit 1
fi

./fixtimestamp.sh
./configure CFLAGS=" -O3" CXXFLAGS=" -O3"
if ! make ; then
  echo "something wrong...exit"
  exit 1
fi

if ! make install ; then
  echo "something wrong...exit"
  exit 1
fi

sed -i 's|Cached.UseSasl=.*|Cached.UseSasl=true|g' /usr/local/lsmcd/conf/node.conf
sed -i 's|#Cached.SaslDB=.*|Cached.SaslDB=/etc/sasllsmcd|g' /usr/local/lsmcd/conf/node.conf


rm -rf /dev/shm/lsmcd/*
systemctl daemon-reload
systemctl start lsmcd
systemctl enable lsmcd

git clone https://github.com/litespeedtech/lsmcd_cpanel_plugin.git
if ! cd lsmcd_cpanel_plugin/res/lsmcd_usermgr ; then
  echo "failed to enter temp dir, exit"
  exit 1
fi

if [ $UBUNTU = "1" ]; then
  apt-get install python3 python3-pip -y
else
  yum -y install python3 python3-pip
fi

./install.sh

rm -rf /home/lsmcd_tmp
}

if [[ -f /usr/local/lsmcd/conf/node.conf ]] ; then
  create_sasl_user
else
  install_lsmcd
  create_sasl_user
fi
