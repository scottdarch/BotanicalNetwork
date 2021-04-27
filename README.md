# Botanical Network

IoT stuff for monitoring and watering my plants and my trees.

![Office Living Gate](greenwall.jpeg)

## MQTT Server

Setup a Raspberry Pi as your MQTT server. First install mosquito:

```
apt install mosquitto mosquitto-clients
systemctl enable mosquitto.service
```
That's about it...but wait! How do we find this magical broker? Since this is all inside-the-firewall stuff we'll use mDNS. This is installed on raspian by default (avahi) which is why you can login to your pi using `ssh pi@mypy.local`. _Thanks to nonrectangular on Server Fault ("[Configure Zeroconf to broadcast multiple names](https://serverfault.com/questions/268401/configure-zeroconf-to-broadcast-multiple-names)")_ we can create an alias so that the pi hostname can be decoupled from our mDNS name for the mqtt server. Start by installing the avahi utilities:

```
sudo apt-get install avahi-utils
```

Next create `/etc/systemd/system/avahi-alias@.service` and add this:

```
[Unit]
Description=Publish %I as alias for %H.local via mdns

[Service]
Type=simple
ExecStart=/bin/bash -c "/usr/bin/avahi-publish -a -R %I $(avahi-resolve -4 -n %H.local | cut -f 2)"

[Install]
WantedBy=multi-user.target
```

Now you can create an alias like this:

```
systemctl enable --now avahi-alias@botanynet.local.service
```

Try it:

```
$ avahi-resolve-address -n botanynet.local
botnet.local	192.168.4.52
```
