#include "netlink/handlers.h"
#include <arpa/inet.h>
#include <batman.h>
#include <ifaddrs.h>
#include <linux/batman_adv.h>
#include <netinet/in.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <view.h>

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

  if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
    fclose(f);
    return 0;
  }

  while (fgets(line, sizeof(line), f)) {
    char orig_mac[18], nexthop_mac[18], iface[32];
    float age_s;
    int tq;

    if (sscanf(line, "%17s %fs (%d) %17s [%31s]", orig_mac, &age_s, &tq,
               nexthop_mac, iface) != 5) {
      continue;
    }

    if (g_ascii_strcasecmp(orig_mac, nexthop_mac) != 0) {
      continue;
    }

    MeshNeighbor *neighbor = &neighbors_out[count];
    g_strlcpy(neighbor->mac, orig_mac, sizeof(neighbor->mac));
    neighbor->link_quality = tq;

    if (!resolve_ip_from_arp(orig_mac, neighbor->ip, sizeof(neighbor->ip))) {
      g_strlcpy(neighbor->ip, "IP_UNRESOLVED", sizeof(neighbor->ip));
    }

    count++;
  }

  fclose(f);
  return count;
}

int on_new_bat_node(struct nl_msg *msg, void *arg) {
  GtkData* gtk_data = (GtkData *) arg;
  MeshNeighbor *neighbor;
  struct nlmsghdr *nlh = nlmsg_hdr(msg);

  struct nlattr *attrs[BATADV_ATTR_MAX + 1];

  if (genlmsg_parse(nlh, 0, attrs, BATADV_ATTR_MAX, NULL) < 0) {
    g_printerr("[Netlink] Error: Failed to parse kernel message attributes.\n");
    return NL_SKIP;
  }

  if (attrs[BATADV_ATTR_ORIG_ADDRESS]) {
    uint8_t *mac_bytes = (uint8_t *)nla_data(attrs[BATADV_ATTR_ORIG_ADDRESS]);

    snprintf(neighbor->mac, sizeof(neighbor->mac),
             "%02x:%02x:%02x:%02x:%02x:%02x", mac_bytes[0], mac_bytes[1],
             mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);

  } else {
    return NL_SKIP;
  }

  if (attrs[BATADV_ATTR_TQ]) {
    neighbor->link_quality = nla_get_u8(attrs[BATADV_ATTR_TQ]);
    g_print("                Link Metric Quality (TQ): %d/255\n",
            neighbor->link_quality);
  } else {
    neighbor->link_quality = 0; // Default fallback if unavailable
  }

  if (!resolve_ip_from_arp(neighbor->mac, neighbor->ip, sizeof(neighbor->ip))) {
    g_strlcpy(neighbor->ip, "IP_UNRESOLVED", sizeof(neighbor->ip));
    return NL_SKIP;
  }

  add_button(gtk_data, neighbor->ip);

  return NL_OK;
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
