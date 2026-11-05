/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#pragma once

#include <flux/core.h>

struct fdpc_handle_s;
typedef struct fdpc_handle_s fdpc_handle_t;

typedef void (*fdpc_error_f) (fdpc_handle_t *handle, int errnum, void *arg);

fdpc_handle_t *fdpc_connection_new (flux_t *h, const char *address, const char *port);
flux_t *fdpc_connection_get_flux (fdpc_handle_t *handle);
void fdpc_connection_set_error_cb (fdpc_handle_t *handle, fdpc_error_f cb, void *arg);
void fdpc_connection_free (fdpc_handle_t *handle);
int fdpc_publish_lifecycle_event (fdpc_handle_t *handle,
                                  const char *publish_to_addr,
                                  const char *payload);
typedef void (*fdpc_hello_world_event_f) (fdpc_handle_t *handle, const char *payload, void *arg);
int fdpc_subscribe_hello_world (fdpc_handle_t *handle,
                                const char *subscribe_to_addr,
                                fdpc_hello_world_event_f cb,
                                void *arg);