#pragma once

#include <functional>
#include <optional>
#include <seanerbus/client/connection.hpp>

extern "C" {
#include <flux/core.h>
}

namespace dataplane_connection {

class DataPlaneConnection : public seanerbus::client::Connection {
    flux_t *flux_handle_;
    flux_watcher_t *fd_watcher_{nullptr};
    std::optional<std::function<void (int)>> error_cb_;
    void handle_error (int errnum);

   public:
    // seanerbus::client::Connection seanerbus_connection;
    DataPlaneConnection (flux_t *h, std::string address, std::string port);
    ~DataPlaneConnection ();
    flux_t *flux_handle () const
    {
        return flux_handle_;
    }
    void on_error (std::function<void (int)> error_cb)
    {
        error_cb_ = error_cb;
    }
    static void fd_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg);
};

}  // namespace dataplane_connection