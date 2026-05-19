#include "eh_wrapper.hpp"
#include "DataplaneConnection.hpp"

using namespace Flux::cplusplus_wrappers;
using namespace dataplane_connection;

static void fd_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg)
{
    DataPlaneConnection *client = static_cast<DataPlaneConnection *> (arg);

    if ((revents & FLUX_POLLERR)) {
        // TODO: handle errors
    }

    if ((revents & FLUX_POLLIN)) {
        client->seanerbus_connection
            .try_read ();  // this will (try to) dispatch the connections incoming messages
        if (client->seanerbus_connection.is_shutdown ()) {
            // Handle connection shutdown, somehow
            // I guess we could crash everything with an exception?
            return;
        }
    }
}

// Flux reactor Callbacks
static void fd_watcher_c_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (fd_watcher_cb, r, w, revents, arg);
    if (exception_safe_wrapper.bad ()) {
        flux_t *h = flux_handle_watcher_get_flux (w);
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
    }
}

DataPlaneConnection::DataPlaneConnection (flux_t *h, std::string address, std::string port)
    : flux_handle_(h), seanerbus_connection (address, port)
{
    flux_reactor_t *r = flux_get_reactor(h);
    int fd = seanerbus_connection.get_fd();
    // actually create th watcher...
    fd_watcher_ = flux_fd_watcher_create(r, fd, FLUX_POLLIN | FLUX_POLLERR, fd_watcher_c_cb, this);
}
