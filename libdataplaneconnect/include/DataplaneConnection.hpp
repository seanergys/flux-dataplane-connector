#pragma once

#include <seanerbus/client/connection.hpp>

extern "C" {
#include <flux/core.h>
}

namespace dataplane_connection
{

class DataPlaneConnection {
    flux_t *flux_handle_;
    flux_watcher_t *fd_watcher_{nullptr};
public:
    seanerbus::client::Connection seanerbus_connection;
    DataPlaneConnection(flux_t *h, std::string address, std::string port);
    flux_t *get_flux_handle() const {
        return flux_handle_;
    }
};

}