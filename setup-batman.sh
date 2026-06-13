#!/bin/bash

# Batman-adv Mesh Network Setup Script

set -e  # Exit on any error

echo "Batman-adv Mesh Network Setup"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run this script as root (use sudo)"
    exit 1
fi

# Function to install packages
install_packages() {
    echo "Installing required packages..."
    apt update
    apt install -y batctl wireless-tools iw net-tools iproute2 avahi-autoipd
    echo "Packages installed successfully"
}

# Function to stop conflicting services
stop_services() {
    echo "Stopping conflicting network services..."
    systemctl stop wpa_supplicant 2>/dev/null || true
    systemctl disable wpa_supplicant 2>/dev/null || true
    systemctl stop dhcpcd 2>/dev/null || true
    systemctl disable dhcpcd 2>/dev/null || true
    systemctl stop NetworkManager 2>/dev/null || true
    systemctl disable NetworkManager 2>/dev/null || true
    echo "Services stopped"
}

# Function to setup wireless interface
setup_wireless() {
    echo "Configuring wireless interface..."
    
    # Unblock WiFi
    rfkill unblock all
    sleep 2
    
    # Block execution until wlan0 hardware directory structure exists
    while [ ! -d "/sys/class/net/wlan0" ]; do
        sleep 1
    done
    
    # Configure wireless interface
    ip link set wlan0 down
    sleep 0.5
    iwconfig wlan0 mode ad-hoc
    iwconfig wlan0 essid "meshnet"
    iwconfig wlan0 channel 1
    ip link set wlan0 up
    sleep 0.5
    
    echo "Wireless interface configured"
}

# Function to setup batman-adv
setup_batman() {
    echo "Setting up Batman-adv..."
    
    # Load batman-adv module
    modprobe batman-adv
    
    # Add wireless interface to batman-adv
    batctl if add wlan0
    sleep 0.5

    # set batman if mac as wlan mac
    ip link set dev bat0 down
    WLAN_MAC=$(cat /sys/class/net/wlan0/address)
    ip link set dev bat0 address $WLAN_MAC

    # Bring up bat0 interface securely
    ip link set dev bat0 up
    sleep 2
    
    echo "Batman-adv setup complete"
}

# Function to configure IP address
configure_ip() {
    echo "Configuring IP address..."

    # Kill any existing avahi-autoipd instance
    pkill avahi-autoipd || true
    sleep 1

    # Make sure bat0 is up
    ip link set up dev bat0
    sleep 2

    # Try avahi-autoipd first
    avahi-autoipd --daemonize --wait bat0
    sleep 5

    # Check if IP was assigned
    IP=$(ip addr show bat0 | grep "inet 169" | awk '{print $2}')
    
    if [ -z "$IP" ]; then
        echo "Avahi failed, assigning random fallback IP..."
        RANDOM_IP=$((RANDOM % 253 + 1))
        ip addr add 169.254.1.$RANDOM_IP/16 dev bat0
        IP="169.254.1.$RANDOM_IP/16"
    fi

    echo "IP address configured: $IP"
    ip addr show bat0 | grep inet
}

# Function to create startup script
create_startup_script() {
    echo "Creating startup script..."
    
    cp start_mesh.sh /usr/local/bin/
    chmod +x /usr/local/bin/start_mesh.sh

    echo "Startup script created: /usr/local/bin/start_mesh.sh"
}

# Function to create systemd service
create_systemd_service() {
    echo "Creating systemd service..."
    
    cp mesh-network.service /etc/systemd/system/mesh-network.service

    systemctl daemon-reload
    systemctl enable mesh-network.service
    echo "Systemd service created and enabled"
}

# Main execution
main() {
    echo "Starting Batman-adv mesh network setup..."
    
    install_packages
    stop_services
    setup_wireless
    setup_batman
    configure_ip
    create_startup_script
    create_systemd_service
    
    echo "mesh network is now configured!"
}

# Run main function
main
