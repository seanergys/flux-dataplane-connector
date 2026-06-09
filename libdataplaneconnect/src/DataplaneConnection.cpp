#include "eh_wrapper.hpp"
#include "DataplaneConnection.hpp"

using namespace Flux::cplusplus_wrappers;
using namespace dataplane_connection;

// Flux reactor Callbacks
static void fd_watcher_c_cb2 (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (dataplane_connection::FluxDataPlaneDriver::fd_watcher_cb,
                            r,
                            w,
                            revents,
                            arg);
    if (exception_safe_wrapper.bad ()) {
        flux_t *h = flux_handle_watcher_get_flux (w);
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
    }
}

void dataplane_connection::FluxDataPlaneDriver::handle_error (int errnum)
{
    flux_watcher_stop(fd_watcher_);
    if (error_cb_) {
        (*error_cb_)(errnum);
    }
}

dataplane_connection::FluxDataPlaneDriver::FluxDataPlaneDriver (flux_t *h,
                                                                std::string address,
                                                                std::string port)
    : flux_handle_ (h), connection_ (address, port)
{
    flux_reactor_t *r = flux_get_reactor(h);
    int fd = connection_.get_fd ();
    fd_watcher_ = flux_fd_watcher_create(r, fd, FLUX_POLLIN | FLUX_POLLERR, fd_watcher_c_cb2, this);
    flux_watcher_start(fd_watcher_);
}

dataplane_connection::FluxDataPlaneDriver::~FluxDataPlaneDriver ()
{
    flux_watcher_stop (fd_watcher_);
    flux_watcher_destroy (fd_watcher_);
}

void dataplane_connection::FluxDataPlaneDriver::fd_watcher_cb (flux_reactor_t *r,
                                                               flux_watcher_t *w,
                                                               int revents,
                                                               void *arg)
{
    FluxDataPlaneDriver *driver = static_cast<FluxDataPlaneDriver *> (arg);

    if ((revents & FLUX_POLLERR)) {
        driver->handle_error (EIO);
    }

    if ((revents & FLUX_POLLIN)) {
        // this will (try to) dispatch the connections incoming messages
        driver->connection()->try_read ();

        // Handle connection shutdown, somehow
        // I guess we could crash everything with an exception?
        if (driver->connection()->is_shutdown ()) {
            return;
        }
    }
}
