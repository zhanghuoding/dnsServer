#!/bin/bash

#define insert raw in /etc/rc.local
insert_raw_in_rclocal=2

rootPath="/opt/dnsServer"
dns_local_path="$rootPath/myDnsServer/dnsIp"
#sudo chown -R root:root /opt/dnsServer/*

sudo rm -rf /usr/bin/restart.ClientDNSIpUpload
sudo ln -s $dns_local_path/restart.ClientDNSIpUpload.sh /usr/bin/restart.ClientDNSIpUpload
sudo chmod +x $dns_local_path/clientDNSIpUpload $dns_local_path/restart.ClientDNSIpUpload.sh /usr/bin/restart.ClientDNSIpUpload

sudo chkconfig iptables off #永久关闭防火墙
sudo systemctl disable firewalld.service #永久关闭防火墙

#设置服务开机自启动
sudo sed -i $insert_raw_in_rclocal'i/usr/bin/restart.ClientDNSIpUpload start' /etc/rc.local
sudo echo "" >> /etc/rc.local
sudo sed -i $insert_raw_in_rclocal'i/usr/bin/restart.ClientDNSIpUpload start' /etc/rc.d/rc.local
sudo echo "" >> /etc/rc.d/rc.local
sudo chmod +x /etc/rc.local
sudo chmod +x /etc/rc.d/rc.local
sudo chkconfig --add restart.ClientDNSIpUpload start #添加该服务,到系统服务
sudo chkconfig restart.ClientDNSIpUpload on #设置此系统服务开启

sudo restart.ClientDNSIpUpload start

