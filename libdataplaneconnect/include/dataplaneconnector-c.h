#pragma once

#include <flux/core.h>

struct fdpc_handle;
typedef struct fdpc_handle fdpc_handle_t;

fdpc_handle_t *fdpc_connection_new(flux_t *h, const char *address, const char *port);
void fdpc_connection_free(fdpc_handle_t *handle);
int fdpc_publish_lifecycle_event(fdpc_handle_t *handle, const char *publish_to_addr, const char *payload);