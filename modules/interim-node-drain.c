#include <flux/core.h>
#include <jansson.h>
#include "../libdataplaneconnect/include/dataplaneconnector-c.h"

#define ANOMALY_SERVICE_UUID "8795ef1e-e299-4f3d-b709-b2777cd5aced"

static void drain_request_continue(flux_future_t *f, void *arg) {
    if (flux_future_has_error(f)) {
        flux_t *h = flux_future_get_flux(f);
        flux_log_error(h, "Draining nodes failed: %s", flux_future_error_string(f));
    }
    printf("MR: CONTINUED\n");
    fflush(0);
}

static void sub_cb (fdpc_handle_t *dataplane, const char *msg, void *arg)
{
    flux_t *h = fdpc_connection_get_flux (dataplane);
    json_error_t jerr;

    json_t *payload = json_loads (msg, 0, &jerr);
    if (!payload) {
        flux_log_error (h, "Could not load json payload %s", jerr.text);
        return;
    }

    const char *targets = NULL;
    const char *reason = NULL;

    if (json_unpack (payload, "{s:s, s?s}", "targets", &targets, "reason", &reason) < 0) {
        flux_log_error (h, "Could not decode json payload");
        return;
    }

    if (!reason) {
        reason = "(drained by interim-drain-module, no reason specified)";
    }

    flux_log (h, LOG_INFO, "Trying to drain rank(s) %s for reason: '%s'", targets, reason);

    flux_future_t *f = flux_rpc_pack (h,
                                      "resource.drain",
                                      0,
                                      0,
                                      "{s:s, s:s, s:s}",
                                      "targets",
                                      targets,
                                      "mode",
                                      "update",
                                      "reason",
                                      reason);
    // TODO Handle error from rpc
    flux_future_then(f, -1., drain_request_continue, NULL);
}

extern int mod_main (flux_t *h, int argc, char **argv)
{
    int rc = 0;

    fdpc_handle_t *dataplane_conn;

    dataplane_conn =
        fdpc_connection_new (h, "demobus://localhost:5398", "demobus://localhost:5398");
    if (!dataplane_conn) {
        flux_log_error (h, "%s: fdpc_connection_new", __FUNCTION__);
        return -1;
    }

    fdpc_subscribe_hello_world (dataplane_conn, ANOMALY_SERVICE_UUID, sub_cb, NULL);

    if ((rc = flux_module_set_running (h)) < 0) {
        flux_log_error (h, "%s: flux_module_set_running", __FUNCTION__);
        goto finish;
    }

    if ((rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0) {
        goto finish;
    }

finish:
    fdpc_connection_free (dataplane_conn);
    return rc;
}

MOD_NAME ("interim-node-drain");