#!/bin/bash

# Start Batman-adv mesh network
modprobe batman-adv

while [ ! -d "/sys/class/net/wlan0" ]; do
    sleep 1
done

# Bring down interface to cleanly handle parameters alteration
ip link set wlan0 down
sleep 0.5

iwconfig wlan0 mode ad-hoc
iwconfig wlan0 essid "meshnet"
iwconfig wlan0 ap "02:12:34:56:78:9A"
iwconfig wlan0 channel 1
ip link set wlan0 up
sleep 1

# Bind interface into the mesh routing table
batctl if add wlan0
sleep 0.5

ip link set dev bat0 down
WLAN_MAC=$(cat /sys/class/net/wlan0/address)
ip link set dev bat0 address $WLAN_MAC

# Wake mesh back up
ip link set dev bat0 up
sleep 2

# Try avahi first
pkill avahi-autoipd || true
avahi-autoipd --daemonize --wait bat0
sleep 5

# Check if IP assigned, fallback to random if not
IP=$(ip addr show bat0 | grep "inet 169" | awk '{print $2}')
if [ -z "$IP" ]; then
    RANDOM_IP=$((RANDOM % 253 + 1))
    ip addr add 169.254.1.$RANDOM_IP/16 dev bat0
fi

# Enable IP forwarding
echo 1 > /proc/sys/net/ipv4/ip_forward

# Configure NAT
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
iptables -A FORWARD -i bat0 -o eth0 -j ACCEPT
iptables -A FORWARD -i eth0 -o bat0 -m state --state RELATED,ESTABLISHED -j ACCEPT
