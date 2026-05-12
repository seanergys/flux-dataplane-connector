#pragma once

#include <seanerbus/client/connection.hpp>

extern "C" {
#include <flux/core.h>
}

namespace dataplane_connection
{

class DataPlaneConnection {
    flux_watcher_t *fd_watcher_{nullptr};
public:
    seanerbus::client::Connection seanerbus_connection;
    DataPlaneConnection(flux_reactor_t *r, std::string address, std::string port);
};

}