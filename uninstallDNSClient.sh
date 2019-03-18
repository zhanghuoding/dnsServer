#!/bin/bash

rootPath="/opt/dnsServer"

#server config file path
relativePath="myDnsServer/dnsIp"
dns_local_path="$rootPath/$relativePath"
ipMap_path="$dns_local_path/conf/ipMap.conf"

#modify nds config file
dns_resolv_conf="/etc/resolv.conf"
dns_named_conf="/etc/named.conf"
dns_named_rfc="/etc/named.rfc1912.zones"
dns_dirctory="/var/named"
dns_chroot_path="/var/named/chroot"

#get server ip and domain name from ipMap.conf
serverIp=$( cat $ipMap_path|grep "serverIp="|awk 'NR==1' )
serverIp=${serverIp#*=}
domainName=$( cat $ipMap_path|grep "domainNameSuffix="|awk 'NR==1' )
domainName=${domainName#*=.}

#creat the name of rev zone file
rev_zone_fileName=$(echo $serverIp | awk -F"." '{print $1"."$2"."$3}').zone
original_rev_zone_fileName=$(echo $serverIp | awk -F"." '{print $3"."$2"."$1}').in-addr.arpa


sudo restart.ServerDNSIpReceive stop
sudo restart.ClientDNSIpUpload stop

sudo mv /etc/rc.local.backup /etc/rc.local
sudo mv /etc/rc.d/rc.local.backup /etc/rc.d/rc.local

sudo rm -f  ${dns_resolv_conf}.frequentlyUsed
sudo mv ${dns_resolv_conf}.backup $dns_resolv_conf
sudo mv ${dns_named_conf}.backup $dns_named_conf
sudo mv ${dns_named_rfc}.backup $dns_named_rfc
####################################################
sudo rm -rf $dns_chroot_path${dns_resolv_conf}.backup
sudo rm -rf $dns_chroot_path${dns_named_conf}.backup
sudo rm -rf $dns_chroot_path${dns_named_rfc}.backup
sudo rm -rf $dns_chroot_path${dns_resolv_conf}
sudo rm -rf $dns_chroot_path${dns_named_conf}
sudo rm -rf $dns_chroot_path${dns_named_rfc}

sudo rm -f $dns_dirctory/$domainName.zone
sudo rm -f $dns_dirctory/$domainName.zone.backup
sudo rm -f $dns_dirctory/$rev_zone_fileName
sudo rm -f $dns_dirctory/$rev_zone_fileName.backup

sudo rm -f $dns_chroot_path$dns_dirctory/$domainName.zone
sudo rm -f $dns_chroot_path$dns_dirctory/$domainName.zone.backup
sudo rm -f $dns_chroot_path$dns_dirctory/$rev_zone_fileName
sudo rm -f $dns_chroot_path$dns_dirctory/$rev_zone_fileName.backup

sudo rm -f /usr/bin/restart.ClientDNSIpUpload
sudo rm -f /usr/bin/restart.ServerDNSIpReceive

#Remove bind dns related files into chrooted directory
sudo rm -rf $dns_chroot_path$dns_dirctory/data/cache_dump.db
sudo rm -rf $dns_chroot_path$dns_dirctory/data/named_stats.txt
sudo rm -rf $dns_chroot_path$dns_dirctory/data/named_mem_stats.txt
sudo rm -rf $dns_chroot_path$dns_dirctory/data/named.run
sudo rm -rf $dns_chroot_path$dns_dirctory/dynamic
sudo rm -rf $dns_chroot_path$dns_dirctory/dynamic/managed-keys.bind



sudo  /usr/libexec/setup-named-chroot.sh /var/named/chroot off

#not launch when computer boot
#sudo chkconfig named off
#sudo service named stop

#####sudo systemctl start named.service
sudo systemctl stop named
sudo systemctl disable named
#sudo systemctl enable named
#sudo systemctl start named

sudo systemctl stop named-chroot
sudo systemctl disable named-chroot
sudo rm -f '/etc/systemd/system/multi-user.target.wants/named-chroot.service'


sudo rm -rf $rootPath

echo -e "\033[40;31mUninstall complete!\033[0m"
