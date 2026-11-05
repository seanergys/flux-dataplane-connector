/*
 * SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include <flux/core.h>
#include <flux/jobtap.h>
#include <dataplane-flux/dataplane.h>
#include <stdio.h>
#include <inttypes.h>
#include <jansson.h>
#include "jj.h"

#include <sys/types.h>
#include <pwd.h>

#define streq(a, b) (strcmp ((a), (b)) == 0)

const char *service_uuid = "834d23e1-fd27-4537-b467-9702ef9ca8a2";

static void on_publish_cb (seanerbus_connection_t *connection,
                           seanerbus_status_t status,
                           const seanerbus_error_t *error,
                           void *user_data)
{
    flux_t *h = user_data;

    if (status != SEANERBUS_STATUS_OK) {
        flux_log (h,
                  LOG_ERR,
                  "%s:%s",
                  "Publishing job lifecycle failed",
                  seanerbus_error_message (error));
    }
}

static enum FLUX_LIFECYCLE_STATE state2seanerbus (flux_job_state_t state)
{
    switch (state) {
        case FLUX_JOB_STATE_NEW:
            return FLUX_LIFECYCLE_STATE_NEW;
        case FLUX_JOB_STATE_DEPEND:
            return FLUX_LIFECYCLE_STATE_DEPEND;
        case FLUX_JOB_STATE_PRIORITY:
            return FLUX_LIFECYCLE_STATE_PRIORITY;
        case FLUX_JOB_STATE_SCHED:
            return FLUX_LIFECYCLE_STATE_SCHED;
        case FLUX_JOB_STATE_RUN:
            return FLUX_LIFECYCLE_STATE_RUN;
        case FLUX_JOB_STATE_CLEANUP:
            return FLUX_LIFECYCLE_STATE_CLEANUP;
    }
    return FLUX_LIFECYCLE_STATE_INACTIVE;
}

const char *extract_job_displayname (json_t *jobspec)
{
    json_t *tasks;
    json_t *command = NULL;
    json_t *arg0;
    const char *path;
    json_unpack (jobspec, "{s:o}", "tasks", &tasks);
    json_unpack (tasks, "[{s:o}]", "command", &command);
    arg0 = json_array_get (command, 0);
    path = json_string_value (arg0);

    const char *p = strchr (path, '/');
    if (p) {
        p++;
        if (*p == '\0') {
            return path;
        }
        return p;
    }
    return path;
}

struct job_run_info {
    double start_time;
};

static void dataplane_destroy (dataplane_context_t *ctx)
{
    if (!ctx)
        return;
    dataplane_context_destroy (ctx);
}

static int process_finish_event (flux_plugin_t *p,
                                 flux_plugin_arg_t *args,
                                 const char *event_key,
                                 seanerbus_FluxLifecycleEventV2_builder_t *builder)
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
        return -1;
    }

    seanerbus_FluxLifecycleEventV2_set_FinishTime (builder, timestamp * 1000 * 1000);

    if (strcmp (event_name, "finish") == 0) {
        seanerbus_FluxLifecycleEventV2_set_RcState (builder, FLUX_FINISH_STATE_COMPLETED);
        return 0;
    } else if (strcmp (event_name, "exception") == 0) {
        const char *type;
        int severity;
        const char *note = NULL;

        json_unpack (event_context,
                     "{s:s, s:i, s?s}",
                     "type",
                     &type,
                     "severity",
                     &severity,
                     "note",
                     &note);

        if (streq (type, "cancel")) {
            seanerbus_FluxLifecycleEventV2_set_RcState (builder, FLUX_FINISH_STATE_CANCELLED);
        } else if (streq (type, "timelimit")) {
            seanerbus_FluxLifecycleEventV2_set_RcState (builder, FLUX_FINISH_STATE_TIMEOUT);
        } else {
            seanerbus_FluxLifecycleEventV2_set_RcState (builder, FLUX_FINISH_STATE_FAILED);
        }

        seanerbus_FluxError_builder_t *err_builder;
        seanerbus_FluxLifecycleEventV2_get_ErrObj_builder (builder, &err_builder, NULL);

        seanerbus_FluxError_set_Type (err_builder, type);
        seanerbus_FluxError_set_Severity (err_builder, severity);
        seanerbus_FluxError_set_Note (err_builder, note);
        return 0;
    }
    return 0;
}

static void build_start_time (flux_plugin_t *p,
                              flux_job_state_t state,
                              double event_timestamp,
                              seanerbus_FluxLifecycleEventV2_builder_t *builder)
{
    double start_time = 0.0;
    struct job_run_info *jri = NULL;

    if (state == FLUX_JOB_STATE_RUN) {
        jri = malloc (sizeof (struct job_run_info));
        jri->start_time = event_timestamp;
        flux_jobtap_job_aux_set (p, FLUX_JOBTAP_CURRENT_JOB, "pjd2::start_time", jri, free);
        start_time = event_timestamp;
    } else {
        jri = flux_jobtap_job_aux_get (p, FLUX_JOBTAP_CURRENT_JOB, "pjd::job_run_info");
        if (jri) {
            start_time = jri->start_time;
        }
    }
    seanerbus_FluxLifecycleEventV2_set_StartTime (builder, start_time * 1000 * 1000);
}

static int populate_from_jobspec (json_t *jobspec,
                                  seanerbus_FluxLifecycleEventV2_builder_t *builder)
{
    const char *bank = "";
    const char *queue = "";
    const char *username = "";
    const char *job_name = "";
    const char *job_displayname = "";
    json_t *job_attr = NULL;
    struct jj_counts resource_counts;

    json_unpack (jobspec,
                 "{s{s{s?s, s?s, s?o}}}",
                 "attributes",
                 "system",
                 "queue",
                 &queue,
                 "bank",
                 &bank,
                 "job",
                 &job_attr);

    if (job_attr) {  // man, jansson is weird
        json_unpack (job_attr, "{s?s}", "name", &job_name);
    }
    if (strlen (job_name) != 0) {
        job_displayname = job_name;
    } else {
        job_displayname = extract_job_displayname (jobspec);
    }

    jj_get_counts_json (jobspec, &resource_counts);
    int ngpus = resource_counts.nslots * resource_counts.slot_gpus;
    int ncpus = resource_counts.nslots * resource_counts.slot_size;

    seanerbus_FluxLifecycleEventV2_set_NSlots (builder, resource_counts.nslots);
    seanerbus_FluxLifecycleEventV2_set_NNodes (builder, resource_counts.nnodes);
    seanerbus_FluxLifecycleEventV2_set_NCpus (builder, ncpus);
    seanerbus_FluxLifecycleEventV2_set_NGpus (builder, ngpus);
    seanerbus_FluxLifecycleEventV2_set_OwnerName (builder, username);
    seanerbus_FluxLifecycleEventV2_set_JobName (builder, job_name);
    seanerbus_FluxLifecycleEventV2_set_JobDisplayName (builder, job_displayname);
    seanerbus_FluxLifecycleEventV2_set_Budget (builder, bank);
    seanerbus_FluxLifecycleEventV2_set_Queue (builder, queue);
    seanerbus_FluxLifecycleEventV2_set_RequestedWalltime (builder,
                                                          (resource_counts.duration * 1000 * 1000));

    return 0;
}

static int populate_from_R (flux_plugin_t *p,
                            flux_plugin_arg_t *args,
                            seanerbus_FluxLifecycleEventV2_builder_t *builder)
{
    json_t *nodelist;
    json_t *R_lite;
    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s{s{s:o, s:o}}}",
                                "R",
                                "execution",
                                "nodelist",
                                &nodelist,
                                "R_lite",
                                &R_lite)
        < 0) {
        flux_jobtap_raise_exception (p,
                                     FLUX_JOBTAP_CURRENT_JOB,
                                     "alloc",
                                     0,
                                     "delegate: unpack %s",
                                     flux_plugin_arg_strerror (args));
        return -1;
    }

    size_t nnodes = json_array_size (nodelist);
    seanerbus_FluxLifecycleEventV2_set_NNodes (builder, nnodes);

    seanerbus_FluxLifecycleEventV2_init_NodeList (builder, nnodes);
    size_t idx;
    json_t *entry;
    json_array_foreach (nodelist, idx, entry) {
        seanerbus_FluxLifecycleEventV2_set_NodeList (builder, idx, json_string_value (entry));
    }

    seanerbus_FluxLifecycleEventV2_init_RLite (builder, json_array_size (R_lite), NULL);
    json_array_foreach (R_lite, idx, entry) {
        seanerbus_FluxLifecycleRLiteNode_builder_t *rlite_builder;
        const char *rank;
        const char *core;
        const char *gpu = NULL;
        json_unpack (entry,
                     "{s:s, s:{s:s, s?s}}",
                     "rank",
                     &rank,
                     "children",
                     "core",
                     &core,
                     "gpu",
                     &gpu);
        seanerbus_FluxLifecycleEventV2_get_RLite_builder (builder, idx, &rlite_builder, NULL);
        seanerbus_FluxLifecycleRLiteNode_set_Ranks (rlite_builder, rank);
        seanerbus_FluxLifecycleRLiteNode_set_ChildCores (rlite_builder, core);
        if (gpu) {
            seanerbus_FluxLifecycleRLiteNode_set_ChildGpus (rlite_builder, gpu);
        }
    }

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
    int user_id;
    double submit_time;
    double event_timestamp;
    json_t *jobspec;

    dataplane_context_t *dataplane_ctx =
        flux_plugin_aux_get (p, "flux::publish-jobs-dataplane2::dataplane_handle");

    if (!h || !dataplane_ctx)
        return -1;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:I, s:i, s:i, s:I, s:i, s:f, s:o, s{s:f}}",
                                "id",
                                &job_id,
                                "state",
                                &state,
                                "prev_state",
                                &prev_state,
                                "priority",
                                &priority,
                                "userid",
                                &user_id,
                                "t_submit",
                                &submit_time,
                                "jobspec",
                                &jobspec,
                                "entry",
                                "timestamp",
                                &event_timestamp)
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

    seanerbus_FluxLifecycleEventV2_builder_t *builder;

    seanerbus_FluxLifecycleEventV2_builder_create (&builder, NULL);

    seanerbus_FluxLifecycleEventV2_set_State (builder, state2seanerbus (state));
    seanerbus_FluxLifecycleEventV2_set_PrevState (builder, state2seanerbus (prev_state));
    seanerbus_FluxLifecycleEventV2_set_Jobid (builder, job_id);
    seanerbus_FluxLifecycleEventV2_set_Owner (builder, user_id);
    seanerbus_FluxLifecycleEventV2_set_SubmitTime (builder, (submit_time * 1000 * 1000));

    job_id_f58 = malloc (32);
    flux_job_id_encode (job_id, "f58plain", job_id_f58, 32);
    seanerbus_FluxLifecycleEventV2_set_JobidF58 (builder, job_id_f58);

    struct passwd *pw = getpwuid (user_id);  // thread safety goes brrrrrrrr
    if (pw) {
        seanerbus_FluxLifecycleEventV2_set_OwnerName (builder, pw->pw_name);
    }

    populate_from_jobspec (jobspec, builder);

    if (state == FLUX_JOB_STATE_RUN || state == FLUX_JOB_STATE_CLEANUP
        || state == FLUX_JOB_STATE_INACTIVE) {
        if (populate_from_R (p, args, builder) < 0) {
            return -1;
        }
    }

    build_start_time (p, state, event_timestamp, builder);

    if (state == FLUX_JOB_STATE_SCHED) {
        seanerbus_FluxLifecycleEventV2_set_Priority (builder, priority);
    }

    if (state == FLUX_JOB_STATE_CLEANUP) {
        process_finish_event (p, args, "entry", builder);
    } else if (state == FLUX_JOB_STATE_INACTIVE) {
        process_finish_event (p, args, "end_event", builder);
    }

    seanerbus_entity_t *entity;
    seanerbus_message_t *seanerbus_msg;
    seanerbus_entity_create_from_string (service_uuid, &entity, NULL);
    seanerbus_FluxLifecycleEventV2_builder_to_message (builder, entity, &seanerbus_msg, NULL);
    seanerbus_connection_publish (dataplane_context_get_connection (dataplane_ctx),
                                  seanerbus_msg,
                                  on_publish_cb,
                                  h,
                                  NULL);

    free (job_id_f58);
    seanerbus_entity_destroy (entity);

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

    if (!h) {
        return -1;
    }

    seanerbus_error_t *seanerbus_error;
    dataplane_context_t *dp_ctx;

    dp_ctx = dataplane_context_new (h,
                                    "demobus://localhost:5398",
                                    "demobus://localhost:5398",
                                    &seanerbus_error);

    if (!dp_ctx) {
        flux_log (h,
                  LOG_ERR,
                  "Could not connect to seanerbus: %s",
                  seanerbus_error_message (seanerbus_error));
        return -1;
    }

    if (flux_plugin_register (p, "publish-jobs-dataplane2", tab) < 0) {
        dataplane_context_destroy (dp_ctx);
        return -1;
    }

    if (flux_plugin_aux_set (p,
                             "flux::publish-jobs-dataplane2::dataplane_handle",
                             dp_ctx,
                             (flux_free_f)dataplane_destroy)
        < 0) {
        dataplane_context_destroy (dp_ctx);
        return -1;
    }
    return 0;
}
