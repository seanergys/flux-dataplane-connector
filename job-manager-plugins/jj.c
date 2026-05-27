/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <errno.h>
#include <string.h>
#include <jansson.h>

#include "jj.h"

static int jj_read_level (json_t *o, int level, struct jj_counts *jj, int nodefactor);

static int jj_read_vertex (json_t *o, int level, struct jj_counts *jj, int nodefactor)
{
    int count;
    const char *type = NULL;
    json_t *with = NULL;
    json_error_t error;
    int exclusive = 0;

    if (json_unpack_ex (o, &error, 0, "{ s:s s:i s?b s?o }",
                       "type", &type,
                       "count", &count,
                       "exclusive", &exclusive,
                       "with", &with) < 0) {
        snprintf (jj->error, sizeof (jj->error) - 1,
                  "level %d: %s", level, error.text);
        errno = EINVAL;
        return -1;
    }
    if (count <= 0) {
        sprintf (jj->error, "Invalid count %d for type '%s'",
                            count, type);
        errno = EINVAL;
        return -1;
    }
    nodefactor = nodefactor * count;
    if (strcmp (type, "node") == 0) {
        jj->nnodes = nodefactor;
        if (exclusive)
            jj->exclusive = true;
    }
    else if (strcmp (type, "slot") == 0)
        jj->nslots = count;
    else if (strcmp (type, "core") == 0)
        jj->slot_size = count;
    else if (strcmp (type, "gpu") == 0)
        jj->slot_gpus = count;
    // ignore unknown resources
    if (with)
        return jj_read_level (with, level+1, jj, nodefactor);
    return 0;

}

static int jj_read_level (json_t *o, int level, struct jj_counts *jj, int nodefactor)
{
    int i;
    json_t *v = NULL;

    if (!json_is_array (o)) {
        snprintf (jj->error, sizeof (jj->error) - 1,
                  "level %d: must be an array", level);
        errno = EINVAL;
        return -1;
    }
    json_array_foreach (o, i, v) {
        if (jj_read_vertex (v, level, jj, nodefactor) < 0)
            return -1;
    }
    return 0;
}

int jj_get_counts_json (json_t *jobspec, struct jj_counts *jj)
{
    int version;
    json_t *resources = NULL;
    json_error_t error;

    if (!jj) {
        errno = EINVAL;
        return -1;
    }
    memset (jj, 0, sizeof (*jj));

    if (json_unpack_ex (jobspec, &error, 0, "{s:i s:o}",
                        "version", &version,
                        "resources", &resources) < 0) {
        snprintf (jj->error, sizeof (jj->error) - 1,
                  "at top level: %s", error.text);
        errno = EINVAL;
        return -1;
    }
    /* jobspec version check omitted as discussed in #6632 and #6682
     * N.B. attributes.system is generally optional, but
     * attributes.system.duration is required in jobspec version 1 */
    if (json_unpack_ex (jobspec, &error, 0, "{s:{s:{s:F}}}",
                        "attributes",
                          "system",
                            "duration", &jj->duration) < 0) {
        snprintf (jj->error, sizeof (jj->error) - 1,
                  "at top level: getting duration: %s", error.text);
        errno = EINVAL;
        return -1;
    }
    if (jj_read_level (resources, 0, jj, 1) < 0)
        return -1;

    if (jj->nslots <= 0) {
        snprintf (jj->error, sizeof (jj->error) - 1,
                 "Unable to determine slot count");
        errno = EINVAL;
        return -1;
    }
    if (jj->slot_size <= 0) {
        snprintf (jj->error, sizeof (jj->error) - 1,
                 "Unable to determine slot size");
        errno = EINVAL;
        return -1;
    }
    if (jj->nnodes)
        jj->nslots *= jj->nnodes;
    return 0;
}

/* vi: ts=4 sw=4 expandtab
 */
