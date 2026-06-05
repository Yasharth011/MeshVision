#pragma once
#include <glib.h>

#define MESH_MAX_NEIGHBORS 64

typedef struct {
  char mac[18];
  char ip[64];
  int link_quality; // TQ value (0-255)
} MeshNeighbor;

// Parses batman-adv originator data and resolves direct neighbor IPs.
int fetch_mesh_neighbors(MeshNeighbor neighbors_out[]);

int get_local_ip(const char *interface, char *ip);
