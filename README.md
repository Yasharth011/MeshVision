# MeshVision
MeshVision is a decentralized, mesh-network-based video streaming application powered by `batman-adv` and `GStreamer`. It is designed to facilitate robust, peer-to-peer video transmission across ad-hoc wireless nodes.

## Prerequisites
To ensure minimal latency and a smooth setup process, please review the following hardware and software requirements before starting:

* **Hardware:** Raspberry Pi 5 (Highly Recommended)
* **Operating System:** Debian Trixie
* **Network Interface:** Your Wi-Fi adapter **must** support `ad-hoc` or `IBSS` mode, as this is a strict requirement for `batman-adv` mesh routing.

> **Note:** While it may run on alternative hardware or older OS versions, doing so may introduce significant video latency or complicate the network configuration.

---

## Directory Structure

```text
.
├── CMakeLists.txt
├── include/
│   ├── batman.h
│   └── view.h
├── meshvision.desktop
├── meshvision.service
├── meshvision.sh
├── README.md
├── setup.sh
└── src/
    ├── batman.c
    ├── main.c
    └── view.c

```

---

## Setup Guide

Getting MeshVision running is fully automated via the provided setup script.

**1. Clone the repository**

```bash
git clone https://github.com/Yasharth011/MeshVision.git
cd MeshVision

```

**2. Run the setup script**
This script will install required dependencies, compile the application, and configure the necessary systemd/autostart services.

```bash
chmod +x setup.sh
sudo ./setup.sh

```

**3. Reboot**
Once the setup is complete, restart your Raspberry Pi:
```bash
sudo reboot

```

*Note: Upon reboot, the application will launch automatically. Please allow 30–40 seconds for the `batman-adv` mesh network to fully initialize and assign IPs before the GUI appears.*

---

## Future Improvements
* Fix and stabilize the `batman-adv` Netlink node listener for dynamic peer detection.
* Reduce the 30-40 second application startup time by optimizing the background network initialization delays.
