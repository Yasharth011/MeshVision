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

static gboolean resolve_ip_from_mac(const char *mac_addr, char *ip_out,
                                    size_t ip_buf_len) {
  FILE *fp = popen("sudo batctl dc", "r");
  if (!fp) {
    perror("Failed to execute batctl dc");
    return FALSE;
  }

  char line[256];
  gboolean found = FALSE;
  int line_index = 0;

  while (fgets(line, sizeof(line), fp)) {

    line_index++;

    if (line_index <= 3)
      continue;

    char token1[32] = {0};
    char token2[32] = {0};
    char token3[32] = {0};
    char token4[32] = {0};

    if (sscanf(line, "%31s %31s %31s %31s", token1, token2, token3, token4) >=
        3) {
      char *ip_ptr = token1;
      char *mac_ptr = token2;
      if (strcmp(token1, "*") == 0) {
        ip_ptr = token2;
        mac_ptr = token3;
      }
      if (strlen(mac_ptr) == 17 && mac_ptr[2] == ':') {
        if (g_ascii_strcasecmp(mac_ptr, mac_addr) == 0) {
          g_strlcpy(ip_out, ip_ptr, ip_buf_len);
          found = TRUE;
          break;
        }
      }
    }
  }

  pclose(fp);
  return found;
}

int fetch_mesh_neighbors(MeshNeighbor neighbors_out[]) {
  FILE *fp;
  char line[256];
  int count = 0;

  fp = popen("sudo batctl o", "r");
  if (fp == NULL) {
    perror("Failed to execute batctl command");
    return 0;
  }

  int line_index = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {

    line_index++;

    if (line_index <= 2)
      continue; // Skip header lines safely

    char orig_mac[18] = {0};
    char nexthop_mac[18] = {0};
    int tq = 0;
    char token1[32] = {0};
    char token2[32] = {0};
    char token3[32] = {0};
    char token4[32] = {0};

    if (sscanf(line, "%31s %31s %31s %31s %17s", token1, token2, token3, token4,
               nexthop_mac) >= 4) {
      char *mac_ptr = token1;
      char *tq_ptr = token3;

      if (strcmp(token1, "*") == 0) {
        mac_ptr = token2;
        tq_ptr = token4;
      }

      if (strlen(mac_ptr) == 17 && mac_ptr[2] == ':') {

        // check for duplicate nodes
        int found = 0;
        for (int i = 0; i < count; i++) {
          if (strcmp(mac_ptr, neighbors_out[i].mac) == 0){
            found = 1;
	    break;
	  }
        }
        if (found == 1) continue;

        g_strlcpy(orig_mac, mac_ptr, sizeof(orig_mac));

        char *clean_tq = g_strdelimit(tq_ptr, "()", ' ');
        g_strstrip(clean_tq);
        tq = atoi(clean_tq);

        if (tq == 0)
          tq = 255;

        if (count >= MESH_MAX_NEIGHBORS)
          break;

        g_strlcpy(neighbors_out[count].mac, orig_mac,
                  sizeof(neighbors_out[count].mac));
        neighbors_out[count].tq = tq;

        if (!resolve_ip_from_mac(orig_mac, neighbors_out[count].ip,
                                 sizeof(neighbors_out[count].ip))) {
          g_strlcpy(neighbors_out[count].ip, orig_mac,
                    sizeof(neighbors_out[count].ip));
        }
        count++;
      }
    }
  }
  pclose(fp);
  return count;
}

int on_new_bat_node(struct nl_msg *msg, void *arg) {
  GtkData *gtk_data = (GtkData *)arg;
  MeshNeighbor *neighbor = g_new0(MeshNeighbor, 1);
  ;
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
    neighbor->tq = nla_get_u8(attrs[BATADV_ATTR_TQ]);
    g_print("                Link Metric Quality (TQ): %d/255\n", neighbor->tq);
  } else {
    neighbor->tq = 0; // Default fallback if unavailable
  }

  if (!resolve_ip_from_mac(neighbor->mac, neighbor->ip, sizeof(neighbor->ip))) {
    g_strlcpy(neighbor->ip, "IP_UNRESOLVED", sizeof(neighbor->ip));
    return NL_SKIP;
  }

  add_button(gtk_data, neighbor->ip);

  g_free(neighbor);

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
        strcpy(ip, inet_ntoa(sa->sin_addr));
        freeifaddrs(ifap);
        return 1;
      }
    }
  }
  freeifaddrs(ifap);
  return 0;
}
