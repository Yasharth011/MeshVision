#!/bin/bash

# script to setup MeshVision on raspberry pi

set -e # exit on error

echo "MeshVision Setup on Raspberry Pi"
# install libraries
echo "Installing required packages" 
sudo apt update
sudo apt install -y cmake apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio libgtk-3-0 libgtk-3-dev libnl-3-dev libnl-genl-3-dev 
echo "Packages installed successufully"

# build the application
cmake -B build -S . 
cmake --build build 

# set-up batman 
chmod +x ./setup-batman.sh
sudo ./setup-batman.sh

# set-up auto-start
mkdir -p ~/.config/autonstart
cp meshvision.toml ~/.config/autostart
