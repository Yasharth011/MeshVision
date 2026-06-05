#include <arpa/inet.h>
#include <batman.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static gboolean resolve_ip_from_arp(const char *mac_addr, char *ip_out,
                                    size_t ip_buf_len) {
  FILE *arp_file = fopen("/proc/net/arp", "r");
  if (!arp_file) {
    return FALSE;
  }

  char line[256];
  gboolean found = FALSE;

  // Skip the header line ("IP address       HW type     Flags...")
  if (!fgets(line, sizeof(line), arp_file)) {
    fclose(arp_file);
    return FALSE;
  }

  while (fgets(line, sizeof(line), arp_file)) {
    char ip[64], hw_type[16], flags[16], hw_addr[18], mask[16], dev[32];

    // Parse the standard Linux ARP table format
    if (sscanf(line, "%63s %15s %15s %17s %15s %31s", ip, hw_type, flags,
               hw_addr, mask, dev) != 6) {
      continue;
    }

    // Case-insensitive comparison of the MAC address
    if (g_ascii_strcasecmp(hw_addr, mac_addr) == 0) {
      g_strlcpy(ip_out, ip, ip_buf_len);
      found = TRUE;
      break;
    }
  }

  fclose(arp_file);
  return found;
}

int fetch_mesh_neighbors(MeshNeighbor neighbors_out[]) {
  // Path to the kernel's originator debugging table
  const char *batman_originators_path =
      "/sys/kernel/debug/batman_adv/bat0/originators";

  FILE *f = fopen(batman_originators_path, "r");
  if (!f) {
    g_printerr(
        "[Mesh] Error: Cannot open batman-adv originators file. Is bat0 up?\n");
    return 0;
  }

  char line[512];
  int count = 0;

  // Skip the first two header rows printed by the batman kernel module
  if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
    fclose(f);
    return 0;
  }

  // Process each originator line by line
  while (fgets(line, sizeof(line), f)) {
    char orig_mac[18], nexthop_mac[18], iface[32];
    float age_s;
    int tq;

    // Parse standard batman-adv originator row structure
    if (sscanf(line, "%17s %fs (%d) %17s [%31s]", orig_mac, &age_s, &tq,
               nexthop_mac, iface) != 5) {
      continue;
    }

    /* * CRITICAL MESH FILTER:
     * A node is a direct "neighbor" ONLY if the destination Originator MAC
     * matches the Next Hop MAC. If they don't match, the node is distant
     * and requires routing through an intermediary peer.
     */
    if (g_ascii_strcasecmp(orig_mac, nexthop_mac) != 0) {
      continue;
    }

    MeshNeighbor *neighbor = &neighbors_out[count];
    g_strlcpy(neighbor->mac, orig_mac, sizeof(neighbor->mac));
    neighbor->link_quality = tq;

    // Attempt to cross-reference the MAC against the system ARP table
    if (!resolve_ip_from_arp(orig_mac, neighbor->ip, sizeof(neighbor->ip))) {
      g_strlcpy(neighbor->ip, "IP_UNRESOLVED", sizeof(neighbor->ip));
    }

    count++;
  }

  fclose(f);
  return count;
}

int get_local_ip(const char *interface, char *ip) {
  struct ifaddrs *ifap, *ifa;
  struct sockaddr_in *sa;

  getifaddrs(&ifap);
  for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
      if (strcmp(interface, ifa->ifa_name) == 0) {
        sa = (struct sockaddr_in *)ifa->ifa_addr;
        ip = inet_ntoa(sa->sin_addr);
        freeifaddrs(ifap);
        return 1;
      }
    }
  }
  freeifaddrs(ifap);
  return 0;
}
