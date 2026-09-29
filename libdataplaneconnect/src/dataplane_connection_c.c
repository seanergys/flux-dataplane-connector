#include "dataplaneconnect/dataplane.h"

struct dataplane_context {
    flux_t *h;
    seanerbus_connection_t *connection;
    flux_watcher_t *fd_watcher;
};

typedef struct dataplane_context dataplane_context_t;

static void fd_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg)
{
    seanerbus_status_t status;
    seanerbus_error_t *error = NULL;
    dataplane_context_t *ctx = arg;

    if ((revents & FLUX_POLLERR)) {
        // TODO: handle error
    }

    if ((revents & FLUX_POLLIN)) {
        status = seanerbus_connection_handle (ctx->connection, &error);
        if (status != SEANERBUS_STATUS_OK) {
            // handle error
            flux_log_error (ctx->h,
                            "Seanerbus handling error: %s",
                            seanerbus_error_message (error));
            seanerbus_error_destroy (error);
            return;
        }

        bool connection_shutdown = false;
        seanerbus_connection_is_shutdown (ctx->connection, &connection_shutdown, NULL);
        if (connection_shutdown) {
            // do something
            return;
        }
    }
}

flux_t *dataplane_context_get_flux (dataplane_context_t *ctx)
{
    return ctx->h;
}

seanerbus_connection_t *dataplane_context_get_connection (dataplane_context_t *ctx)
{
    return ctx->connection;
}

dataplane_context_t *dataplane_context_create_from_connection (flux_t *h,
                                                               seanerbus_connection_t *conn)
{
    flux_reactor_t *r = flux_get_reactor (h);
    dataplane_context_t *ctx = calloc (sizeof (ctx), 1);
    seanerbus_status_t rc;
    seanerbus_error_t *error;

    int fd;
    rc = seanerbus_connection_get_fd (conn, &fd, &error);

    if (rc != SEANERBUS_STATUS_OK) {
        seanerbus_error_destroy (error);
        return NULL;
    }

    flux_watcher_t *fd_watcher = flux_fd_watcher_create (r,
                                                         fd,
                                                         FLUX_POLLIN | FLUX_POLLERR | FLUX_POLLOUT,
                                                         fd_watcher_cb,
                                                         ctx);
    ctx->h = h;
    ctx->connection = conn;
    ctx->fd_watcher = fd_watcher;
    flux_watcher_start (fd_watcher);
    return ctx;
}

void dataplane_context_destroy (dataplane_context_t *ctx)
{
    if (ctx->connection) {
        seanerbus_connection_shutdown (ctx->connection, NULL);
    }

    flux_watcher_stop (ctx->fd_watcher);
    flux_watcher_destroy (ctx->fd_watcher);
    free (ctx);
}