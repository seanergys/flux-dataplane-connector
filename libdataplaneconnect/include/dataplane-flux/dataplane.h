/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef DATAPLANE_H
#define DATAPLANE_H

#include <flux/core.h>
#include <seanerbus/c/seanerbus.h>

struct dataplane_context;
typedef struct dataplane_context dataplane_context_t;

dataplane_context_t *dataplane_context_create_from_connection (flux_t *h,
                                                               seanerbus_connection_t *conn);

dataplane_context_t *dataplane_context_new (flux_t *h,
                                            const char *pub_sub_endpoint,
                                            const char *req_res_endpoint,
                                            seanerbus_error_t **error);

flux_t *dataplane_context_get_flux (dataplane_context_t *ctx);

seanerbus_connection_t *dataplane_context_get_connection (dataplane_context_t *ctx);

void dataplane_context_destroy (dataplane_context_t *ctx);

#endif /*DATAPLANE_H*/