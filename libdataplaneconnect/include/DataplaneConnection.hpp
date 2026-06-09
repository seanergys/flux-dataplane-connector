#pragma once

#include <functional>
#include <optional>
#include <seanerbus/client/connection.hpp>

extern "C" {
#include <flux/core.h>
}

namespace dataplane_connection {

class FluxDataPlaneDriver {
    flux_t *flux_handle_;
    flux_watcher_t *fd_watcher_{nullptr};
    std::optional<std::function<void (int)>> error_cb_;
    seanerbus::client::Connection connection_;
    void handle_error (int errnum);

public:
    FluxDataPlaneDriver(flux_t *h, std::string address, std::string port);
    ~FluxDataPlaneDriver();
    seanerbus::client::Connection *connection() {
        return &connection_;
    }
    flux_t *flux_handle()  const {
        return flux_handle_;
    }
    void on_error(std::function<void (int)> error_cb) {
        error_cb_ = error_cb;
    }
    static void fd_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg);
};

}  // namespace dataplane_connection