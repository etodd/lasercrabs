Master server
=============

1.  Set FQDN in /etc/hosts
2.  apt-get install sendmail
3.  Install CA cert in /etc/ssl/certs
4.  mkdir /root/deceiver && mkdir /root/deceiver/crash_dumps
5.  Install deceivermaster in /root/deceiver
6.  Install dashboard.html in /root/deceiver
7.  Create /root/deceiver/config.txt and specify "itch_api_key", "gamejolt_api_key", "secret",
    and "ca_path" (/etc/ssl/certs)
8.  Install unit-status-mail.sh in /root
9.  Install unit-status-mail@.service in /etc/systemd/system
10.  Install deceivermaster.service in /etc/systemd/system
11. Set up nginx and letsencrypt
	- apt-get install nginx software-properties-common
	- Install deceivermaster-nginx.conf as /etc/nginx/sites-available/default
	- add-apt-repository ppa:certbot/certbot
	- apt-get update
	- apt-get install python-certbot-nginx
	- certbot --nginx certonly
12. systemctl enable deceivermaster
13. systemctl start deceivermaster

Game server
===========

1.  Set FQDN in /etc/hosts
2.  apt-get install sendmail
3.  mkdir /root/deceiver && mkdir /root/deceiver/air && mkdir /root/deceiver/rec
4.  Install deceiversrv and assets in /root/deceiver
5.  Create /root/deceiver/config.txt and specify "version", "public_ipv4", "secret",
    "public_ipv6", "record", "region", and "framerate_limit"
6.  Install unit-status-mail.sh in /root
7.  Install unit-status-mail@.service in /etc/systemd/system
8.  Install deceiversrv*.service in /etc/systemd/system
9.  systemctl enable deceiversrv*
10. systemctl start deceiversrv*
