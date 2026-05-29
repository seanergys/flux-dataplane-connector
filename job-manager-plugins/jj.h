/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef HAVE_JJ_H
#define HAVE_JJ_H 1

#include <jansson.h>
#include <stdbool.h>

#define JJ_ERROR_TEXT_LENGTH 256

struct jj_counts {
    int nnodes;    /* total number of nodes requested */
    int nslots;    /* total number of slots requested */
    int slot_size; /* number of cores per slot        */
    int slot_gpus; /* number of gpus per slot        */

    bool exclusive;  /* enable node exclusive allocation if available */

    double duration; /* attributes.system.duration if set */

    char error[JJ_ERROR_TEXT_LENGTH]; /* On error, contains error description */
};

/*  Identical to jj_get_counts, but take json_t  */
int jj_get_counts_json (json_t *jobspec, struct jj_counts *counts);

#endif /* !HAVE_JJ_H */
