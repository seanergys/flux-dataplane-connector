#include <flux/core.h>
#include <flux/jobtap.h>
#include "../libdataplaneconnect/include/dataplaneconnector-c.h"
#include <stdio.h>
#include <inttypes.h>
#include <jansson.h>

const char *service_uuid = "f50de671-c6c4-49a5-b684-22d2c321f605";

static void dataplane_destroy (fdpc_handle_t *handle)
{
    if (!handle)
        return;
    fdpc_connection_free(handle);
}

static int create_cb (flux_plugin_t *p, const char *topic, flux_plugin_arg_t *args, void *arg)
{
    flux_t *h = flux_jobtap_get_flux (p);
    flux_jobid_t job_id;
    fdpc_handle_t *dataplane = flux_plugin_aux_get(p, "flux::publish-jobs-dataplane::dataplane_handle");

    if (!h || !dataplane)
        return -1;

    if (flux_plugin_arg_unpack (args, FLUX_PLUGIN_ARG_IN, "{s:I}", "id", &job_id) < 0) {
        flux_jobtap_raise_exception (p,
                                     FLUX_JOBTAP_CURRENT_JOB,
                                     "alloc",
                                     0,
                                     "delegate: unpack: %s",
                                     flux_plugin_arg_strerror (args));
        flux_log_error (h, "flux_plugin_arg_unpack");
        return -1;
    }
    json_t *payload_json = json_pack("{s: s, s: i}", "event", "create", "jobid", job_id);
    char *payload_str = json_dumps(payload_json, JSON_COMPACT); 
    fdpc_publish_lifecycle_event(dataplane, service_uuid, payload_str);
    free(payload_str);
    json_decref(payload_json);
    printf("MR: JOPTOP TEST PLUGIN: NEW JOB %" PRIu64 "\n", job_id);
    return 0;
}


static const struct flux_plugin_handler tab[] = {
    {"job.create", create_cb, NULL},
    {0},
};

int flux_plugin_init (flux_plugin_t *p)
{
    flux_t *h = flux_jobtap_get_flux (p);
    fdpc_handle_t *dataplane_handle;

    if (!h
        || flux_plugin_register (p, "publish-jobs-dataplane", tab) < 0
        || !(dataplane_handle = fdpc_connection_new(h, "localhost", "5398"))
        || flux_plugin_aux_set (p, "flux::publish-jobs-dataplane::dataplane_handle", dataplane_handle, (flux_free_f)dataplane_destroy) < 0){
        return -1;
    }
    return 0;
}