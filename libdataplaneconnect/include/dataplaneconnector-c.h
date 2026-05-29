#pragma once

#include <flux/core.h>

struct fdpc_handle_s;
typedef struct fdpc_handle_s fdpc_handle_t;

typedef void (*fdpc_error_f)(fdpc_handle_t *handle, int errnum, void *arg);

fdpc_handle_t *fdpc_connection_new(flux_t *h, const char *address, const char *port);
void fdpc_connection_set_error_cb(fdpc_handle_t *handle, fdpc_error_f cb, void *arg);
void fdpc_connection_free(fdpc_handle_t *handle);
int fdpc_publish_lifecycle_event(fdpc_handle_t *handle, const char *publish_to_addr, const char *payload);