Master server
=============

1.  Set FQDN in /etc/hosts
2.  apt-get install sendmail
3.  Install CA cert in /etc/ssl/certs
4.  mkdir /root/lasercrabs && mkdir /root/lasercrabs/crash_dumps
5.  Install lasercrabmaster in /root/lasercrabs
6.  Install dashboard.html in /root/lasercrabs
7.  Create /root/lasercrabs/config.txt and specify "itch_api_key", "gamejolt_api_key", "secret",
    and "ca_path" (/etc/ssl/certs)
8.  Install unit-status-mail.sh in /root
9.  Install unit-status-mail@.service in /etc/systemd/system
10.  Install lasercrabmaster.service in /etc/systemd/system
11. Set up nginx and letsencrypt
	- apt-get install nginx software-properties-common
	- Install lasercrabmaster-nginx.conf as /etc/nginx/sites-available/default
	- add-apt-repository ppa:certbot/certbot
	- apt-get update
	- apt-get install python-certbot-nginx
	- certbot --nginx certonly
12. systemctl enable lasercrabmaster
13. systemctl start lasercrabmaster

Game server
===========

1.  Set FQDN in /etc/hosts
2.  apt-get install sendmail
3.  mkdir /root/lasercrabs && mkdir /root/lasercrabs/rec
4.  Install lasercrabsrv and assets in /root/lasercrabs
5.  Create /root/lasercrabs/config.txt and specify "version", "public_ipv4", "secret",
    "public_ipv6", "record", "region", and "framerate_limit"
6.  Install unit-status-mail.sh in /root
7.  Install unit-status-mail@.service in /etc/systemd/system
8.  Install lasercrabsrv*.service in /etc/systemd/system
9.  systemctl enable lasercrabsrv*
10. systemctl start lasercrabsrv*
