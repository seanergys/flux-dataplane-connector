#include <flux/core.h>
#include "../libdataplaneconnect/include/dataplaneconnector-c.h"

#define MESSAGE_SERVICE_UUID "f1c30d2a-62c3-4935-90d1-b95a85e81e4b"

void message_cb (fdpc_handle_t *dataplane, const char *msg, void *arg)
{
    flux_t *h = fdpc_connection_get_flux (dataplane);
    printf ("MR: new message %s\n", msg);
    fflush (0);
    flux_log (h, LOG_NOTICE, "Dataplane Message: %s", msg);
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

    fdpc_subscribe_hello_world (dataplane_conn, MESSAGE_SERVICE_UUID, message_cb, NULL);

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

MOD_NAME ("seanergys-dataplane-log");