#include <flux/core.h>
#include <flux/jobtap.h>
#include "../libdataplaneconnect/include/dataplaneconnector-c.h"
#include <stdio.h>
#include <inttypes.h>
#include <jansson.h>
#include "jj.h"

const char *service_uuid = "f50de671-c6c4-49a5-b684-22d2c321f605";

static void dataplane_destroy (fdpc_handle_t *handle)
{
    if (!handle)
        return;
    fdpc_connection_free (handle);
}

static int post_lifecycle_event (fdpc_handle_t *dataplane, json_t *payload)
{
    char *payload_str = json_dumps (payload, JSON_COMPACT);
    int rc = fdpc_publish_lifecycle_event (dataplane, service_uuid, payload_str);
    free (payload_str);
    return rc;
}

static json_t *process_finish_event (flux_plugin_t *p,
                                     flux_plugin_arg_t *args,
                                     const char *event_key)
{
    const char *event_name;
    json_t *event_context;
    double timestamp;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s{s:f s:s, s:o}}",
                                event_key,
                                "timestamp",
                                &timestamp,
                                "name",
                                &event_name,
                                "context",
                                &event_context)
        < 0) {
        flux_jobtap_raise_exception (p,
                                     FLUX_JOBTAP_CURRENT_JOB,
                                     "alloc",
                                     1,
                                     "delegate: unpack: %s",
                                     flux_plugin_arg_strerror (args));
        return NULL;
    }

    if (strcmp (event_name, "finish") == 0) {
        return json_pack ("{s:s, s:f}", "rc_state", "COMPLETED", "finish_time", timestamp);
    } else if (strcmp (event_name, "exception") == 0) {
        const char *type;
        const char *rc_state;
        json_unpack (event_context, "{s:s}", "type", &type);
        if (strcmp (type, "cancel") == 0) {
            rc_state = "CANCELLED";
        } else if (strcmp (type, "timelimit") == 0) {
            rc_state = "TIMEOUT";
        } else {
            rc_state = "FAILED";
        }
        return json_pack ("{s:s, s:O, s:f}",
                          "rc_state",
                          rc_state,
                          "exception",
                          event_context,
                          "finish_time",
                          timestamp);
    }
    return NULL;
}

static int build_run_info (flux_t *h, flux_plugin_t *p, flux_plugin_arg_t *args, json_t **run_info)
{
    json_t *jobspec;
    json_t *R;
    json_t *nodelist;
    json_t *event;
    double timestamp;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:o, s{s:F}}",
                                "R",
                                &R,
                                "entry",
                                "timestamp",
                                &timestamp)
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

    json_unpack (R, "{s{s:O}}", "execution", "nodelist", &nodelist);
    *run_info = json_pack ("{s:o, s:f}", "nodelist", nodelist, "start_time", timestamp);
    return 0;
}

static int build_submit_info (json_t *jobspec,
                              int user_id,
                              double submit_time,
                              double duration,
                              json_t **submit_info)
{
    const char *bank = "";
    const char *queue = "";
    const char *job_name = "";

    json_unpack (jobspec,
                 "{s:{s:{s:s, s{s:s}}}}",
                 "attributes",
                 "system",
                 "bank",
                 &bank,
                 "job",
                 "name",
                 &job_name);

    *submit_info = json_pack ("{s:i, s:s, s:s, s:s, s:f, s:f}",
                              "owner",
                              user_id,
                              "jobname",
                              job_name,
                              "budget",
                              bank,
                              "queue",
                              queue,
                              "submittime",
                              submit_time,
                              "walltime",
                              duration);

    return 0;
}

static int state_change_cb (flux_plugin_t *p, const char *topic, flux_plugin_arg_t *args, void *arg)
{
    flux_t *h = flux_jobtap_get_flux (p);
    flux_jobid_t job_id;
    char *job_id_f58;
    flux_job_state_t state;
    flux_job_state_t prev_state;
    json_int_t priority;
    json_t *resources = NULL;
    json_t *submit_info = NULL;
    json_t *finish_info = NULL;
    json_t *run_info = NULL;
    json_t *sched_info = NULL;

    fdpc_handle_t *dataplane =
        flux_plugin_aux_get (p, "flux::publish-jobs-dataplane::dataplane_handle");

    if (!h || !dataplane)
        return -1;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:I, s:i, s:i, s:I}",
                                "id",
                                &job_id,
                                "state",
                                &state,
                                "prev_state",
                                &prev_state,
                                "priority",
                                &priority)
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

    job_id_f58 = malloc (32);
    flux_job_id_encode (job_id, "f58plain", job_id_f58, 32);

    if (state == FLUX_JOB_STATE_DEPEND || state == FLUX_JOB_STATE_PRIORITY
        || state == FLUX_JOB_STATE_RUN || state == FLUX_JOB_STATE_SCHED) {
        json_t *jobspec;
        int user_id;
        double submit_time;

        if (flux_plugin_arg_unpack (args,
                                    FLUX_PLUGIN_ARG_IN,
                                    "{s:o, s:i, s:f}",
                                    "jobspec",
                                    &jobspec,
                                    "userid",
                                    &user_id,
                                    "t_submit",
                                    &submit_time)
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

        // stitch together `resources`
        struct jj_counts resource_counts;
        jj_get_counts_json (jobspec, &resource_counts);
        int ngpus = resource_counts.nslots * resource_counts.slot_gpus;
        int ncpus = resource_counts.nslots * resource_counts.slot_size;
        resources = json_pack ("{s:i, s:i, s:i, s:i}",
                               "nslots",
                               resource_counts.nslots,
                               "nnodes",
                               resource_counts.nnodes,
                               "ncpus",
                               ncpus,
                               "ngpus",
                               ngpus);

        build_submit_info (jobspec, user_id, submit_time, resource_counts.duration, &submit_info);

        if (state == FLUX_JOB_STATE_RUN) {
            if (build_run_info (h, p, args, &run_info) != 0) {
                return -1;
            }
        }
    }

    if (state == FLUX_JOB_STATE_SCHED) {
        sched_info = json_pack ("{s:I}", "priority", priority);
    }

    if (state == FLUX_JOB_STATE_CLEANUP) {
        finish_info = process_finish_event (p, args, "entry");
    } else if (state == FLUX_JOB_STATE_INACTIVE) {
        finish_info = process_finish_event (p, args, "end_event");
    }

    json_t *payload = json_pack ("{s:i, s:I, s:s, s:s, s:s, s:o*, s:o*, s:o*, s:o*, s:o*}",
                                 "v",
                                 0,
                                 "jobid",
                                 (json_int_t)job_id,
                                 "jobid_f58",
                                 job_id_f58,
                                 "state",
                                 flux_job_statetostr (state, "l"),
                                 "prev_state",
                                 flux_job_statetostr (prev_state, "l"),
                                 "resources",
                                 resources,
                                 "submit_info",
                                 submit_info,
                                 "sched_info",
                                 sched_info,
                                 "run_info",
                                 run_info,
                                 "finish_info",
                                 finish_info);

    post_lifecycle_event (dataplane, payload);
    json_decref (payload);

    return 0;
}

static const struct flux_plugin_handler tab[] = {
    {"job.state.depend", state_change_cb, NULL},
    {"job.state.priority", state_change_cb, NULL},
    {"job.state.sched", state_change_cb, NULL},
    {"job.state.run", state_change_cb, NULL},
    {"job.state.cleanup", state_change_cb, NULL},
    {"job.state.inactive", state_change_cb, NULL},
    {"job.state.running", state_change_cb, NULL},
    {0},
};

int flux_plugin_init (flux_plugin_t *p)
{
    flux_t *h = flux_jobtap_get_flux (p);
    fdpc_handle_t *dataplane_handle;

    if (!h || flux_plugin_register (p, "publish-jobs-dataplane", tab) < 0
        || !(dataplane_handle = fdpc_connection_new (h, "localhost", "5398"))
        || flux_plugin_aux_set (p,
                                "flux::publish-jobs-dataplane::dataplane_handle",
                                dataplane_handle,
                                (flux_free_f)dataplane_destroy)
               < 0) {
        return -1;
    }
    return 0;
}
