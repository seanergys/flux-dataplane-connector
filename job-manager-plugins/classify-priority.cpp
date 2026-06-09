extern "C" {
#include <flux/core.h>
#include <flux/jobtap.h>
#include <jansson.h>
}
#include "../libdataplaneconnect/include/DataplaneConnection.hpp"
#include <seanerbus/msg/flux_classify_job_req_v1.hpp>
#include <seanerbus/msg/flux_classify_job_res_v1.hpp>

using dataplane_connection::FluxDataPlaneDriver;

static int priority_cb (flux_plugin_t *p, const char *topic, flux_plugin_arg_t *args, void *data)

{
    flux_jobid_t job_id;
    FluxDataPlaneDriver *dataplane;
    int urgency;
    json_int_t priority;
    json_t *jobspec;
    flux_t *h = flux_jobtap_get_flux (p);
    dataplane = (FluxDataPlaneDriver *)flux_plugin_aux_get (p, "dataplane");

    if (!h || !dataplane) {
        return -1;
    }

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:I, s:i, s:I, s:o}",
                                "id",
                                &job_id,
                                "urgency",
                                &urgency,
                                "priority",
                                &priority,
                                "jobspec",
                                &jobspec)
        < 0) {
        flux_jobtap_raise_exception (p,
                                     FLUX_JOBTAP_CURRENT_JOB,
                                     "alloc",
                                     0,
                                     "delegate: unpack: %s",
                                     flux_plugin_arg_strerror (args));
        flux_log_error (h, "flux_plugin_arg_unpack");
        return -1;
    }

    // flux_jobtap_reprioritize_job(p, job_id, FLUX_JOB_PRIORITY_MIN); // this should hold the job

    const char *jobspec_str = json_dumps (jobspec, JSON_COMPACT);
    auto payload = seanerbus::msg::FluxClassifyJobReqV1{jobspec_str}.to_capnp ();
    free ((void *)jobspec_str);
    dataplane->connection()->request (seanerbus::Address{seanerbus::Uuid{"70bad6fe-3bdb-4926-b3d6-ef6422c25588"}},
                        payload,
                        [p, job_id] (seanerbus::client::Connection *conn, seanerbus::Message msg) {
                            if (msg.payload.first != (uint64_t)::MessageTypes::FLUX_CLASSIFY_JOB_RES_V1)
                            {
                                // :shrug:
                            }
                            else
                            {
                                seanerbus::msg::FluxClassifyJobResV1 res(msg.payload);
                                if (res.str_result == "good") {
                                    flux_jobtap_reprioritize_job(p, job_id, 32);
                                } else {
                                    flux_jobtap_reprioritize_job(p, job_id, 16);
                                }
                            }
                        });

    return 0;
}

static const struct flux_plugin_handler tab[] = {
    {"job.state.priority", priority_cb, NULL},
    {"job.priority.get", priority_cb, NULL},
    {0},
};

static void dataplane_destroy (FluxDataPlaneDriver *dataplane)
{
    if (!dataplane)
        return;
    delete dataplane;
}

extern "C" int flux_plugin_init (flux_plugin_t *p)
{
    flux_t *h = flux_jobtap_get_flux (p);
    auto *dataplane = new FluxDataPlaneDriver (h, "localhost", "5398");

    // load active jobs in reverse state in order to re-establish any potential
    // flux-accounting dependencies issued on active jobs
    if (flux_jobtap_set_load_sort_order (p, "-state") < 0) {
        flux_log (h,
                  LOG_ERR,
                  "error loading sort order of active jobs: %s",
                  flux_plugin_strerror (p));
        return -1;
    }

    if (!h || flux_plugin_register (p, "classify-priority", tab) < 0
        || flux_plugin_aux_set (p, "dataplane", dataplane, (flux_free_f)dataplane_destroy) < 0) {
        return -1;
    }

    return 0;
}
