extern "C" {
#include "dataplaneconnector-c.h"
}
#include <DataplaneConnection.hpp>
#include "eh_wrapper.hpp"
#include <seanerbus/msg/flux_lifecycle_event_v1.hpp>

using namespace Flux::cplusplus_wrappers;

struct fdpc_handle_s {
    flux_t *h;
    dataplane_connection::DataPlaneConnection c;
};

static int fdpc_connection_new_safe(flux_t *h, const char *address, const char *port, fdpc_handle_t **handle) {
    *handle = new fdpc_handle_s { h, dataplane_connection::DataPlaneConnection { h, address, port }};
    return 0;
}

fdpc_handle_t *fdpc_connection_new(flux_t *h, const char *address, const char *port) {
    eh_wrapper_t exception_safe_wrapper;
    fdpc_handle_t *handle;
    exception_safe_wrapper (fdpc_connection_new_safe, h, address, port, &handle);
    if (exception_safe_wrapper.bad ()) {
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
        return nullptr;
    }
    return handle;
}

void fdpc_connection_set_error_cb (fdpc_handle_t *handle, fdpc_error_f cb, void *arg)
{
    handle->c.set_error_cb([cb, handle, arg](int errnum){
        cb(handle, errnum, arg);
    });
}

static void fdpc_publish_lifecycle_event_safe(fdpc_handle_t *handle, const char *publish_to_addr, const char *payload) {
    auto addr = seanerbus::Address{ seanerbus::Uuid{ publish_to_addr } };
    auto event = seanerbus::msg::FluxLifecycleEventV1 { payload };
    handle->c.seanerbus_connection.publish(addr, event.to_capnp());
}

int fdpc_publish_lifecycle_event(fdpc_handle_t *handle, const char *publish_to_addr, const char *payload) {
    flux_t *h = handle->h;
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (fdpc_publish_lifecycle_event_safe, handle, publish_to_addr, payload);
    if (exception_safe_wrapper.bad ()) {
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
        return -1;
    }
    return 0;
}

void fdpc_connection_free(fdpc_handle_t *handle) {
    delete handle;
}