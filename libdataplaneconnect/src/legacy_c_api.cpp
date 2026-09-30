extern "C" {
#include "dataplaneconnector-c.h"
}
#include <DataplaneConnection.hpp>
#include "eh_wrapper.hpp"

using namespace Flux::cplusplus_wrappers;

struct fdpc_handle_s {
    flux_t *h;
    dataplane_connection::FluxDataPlaneDriver d;
};

static int fdpc_connection_new_safe (flux_t *h,
                                     const char *pub_sub_endpoint,
                                     const char *req_res_endpoint,
                                     fdpc_handle_t **handle)
{
    *handle = new fdpc_handle_s{h,
                                dataplane_connection::FluxDataPlaneDriver{h,
                                                                          pub_sub_endpoint,
                                                                          req_res_endpoint}};
    return 0;
}

fdpc_handle_t *fdpc_connection_new (flux_t *h, const char *address, const char *port)
{
    eh_wrapper_t exception_safe_wrapper;
    fdpc_handle_t *handle;
    exception_safe_wrapper (fdpc_connection_new_safe, h, address, port, &handle);
    if (exception_safe_wrapper.bad ()) {
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
        return nullptr;
    }
    return handle;
}

flux_t *fdpc_connection_get_flux (fdpc_handle_t *handle)
{
    return handle->h;
}

void fdpc_connection_set_error_cb (fdpc_handle_t *handle, fdpc_error_f cb, void *arg)
{
    handle->d.on_error ([cb, handle, arg] (int errnum) { cb (handle, errnum, arg); });
}

static void fdpc_publish_lifecycle_event_safe (fdpc_handle_t *handle,
                                               const char *publish_to_addr,
                                               const char *payload)
{
    auto addr = seanerbus::msg::Entity{seanerbus::msg::Uuid (publish_to_addr)};
    seanerbus::msg::MessageBuilder<FluxLifecycleEventV1> msg_builder;
    msg_builder.builder ().setData (payload);
    handle->d.connection ()->publish (msg_builder.to_message (addr));  // TODO: Error callback
}

int fdpc_publish_lifecycle_event (fdpc_handle_t *handle,
                                  const char *publish_to_addr,
                                  const char *payload)
{
    flux_t *h = handle->h;
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (fdpc_publish_lifecycle_event_safe, handle, publish_to_addr, payload);
    if (exception_safe_wrapper.bad ()) {
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
        return -1;
    }
    return 0;
}

static int _fdpc_subscribe_hello_world (fdpc_handle_t *handle,
                                        const char *subscribe_to_addr,
                                        fdpc_hello_world_event_f cb,
                                        void *arg)
{
    auto addr = seanerbus::msg::Entity (seanerbus::msg::Uuid (subscribe_to_addr));
    handle->d.connection ()
        ->subscribe (addr,
                     [handle,
                      cb,
                      arg] (seanerbus::client::Connection &conn,
                            seanerbus::Expected<seanerbus::msg::Message, seanerbus::Error> result) {
                         if (!result.ok ()) {
                             // Handle error
                             return;
                         }

                         seanerbus::msg::Message msg = result.unwrap_ok ();

                         if (msg.payload_type () != HelloWorldResV1::TYPE) {
                             // Handle error
                             return;
                         }

                         auto reader =
                             seanerbus::msg::MessageReader<HelloWorldResV1>::from_msg (msg)
                                 .unwrap_ok ();
                         const char *payload = reader.reader ().getMsg ().cStr ();
                         cb (handle, payload, arg);
                     });
    return 0;
}

int fdpc_subscribe_hello_world (fdpc_handle_t *handle,
                                const char *subscribe_to_addr,
                                fdpc_hello_world_event_f cb,
                                void *arg)
{
    flux_t *h = handle->h;
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (_fdpc_subscribe_hello_world, handle, subscribe_to_addr, cb, arg);
    if (exception_safe_wrapper.bad ()) {
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
        return -1;
    }
    return 0;
}

void fdpc_connection_free (fdpc_handle_t *handle)
{
    delete handle;
}
