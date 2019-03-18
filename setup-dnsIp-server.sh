#!/bin/bash

#define insert raw in /etc/rc.local
insert_raw_in_rclocal=2

#get the version of OS
linuxOSVersion=$( cat /proc/version )

rootPath="/opt/dnsServer"
dns_local_path="$rootPath/myDnsServer/dnsIp"
#sudo chown -R root:root /opt/myDnsServer/*

sudo rm -rf /usr/bin/restart.ServerDNSIpReceive
sudo ln -s $dns_local_path/restart.ServerDNSIpReceive.sh /usr/bin/restart.ServerDNSIpReceive
sudo chmod +x $dns_local_path/serverDNSIpReceive $dns_local_path/restart.ServerDNSIpReceive.sh /usr/bin/restart.ServerDNSIpReceive

sudo chkconfig iptables off #永久关闭防火墙
sudo systemctl disable firewalld.service #永久关闭防火墙

#设置服务开机自启动
sudo sed -i $insert_raw_in_rclocal'i/usr/bin/restart.ServerDNSIpReceive start' /etc/rc.local
sudo echo "" >> /etc/rc.local
sudo sed -i $insert_raw_in_rclocal'i/usr/bin/restart.ServerDNSIpReceive start' /etc/rc.d/rc.local
sudo echo "" >> /etc/rc.d/rc.local
sudo chmod +x /etc/rc.local
sudo chmod +x /etc/rc.d/rc.local
sudo chkconfig --add restart.ServerDNSIpReceive start #添加该服务,到系统服务
sudo chkconfig restart.ServerDNSIpReceive on #设置此系统服务开启

sudo restart.ServerDNSIpReceive start

