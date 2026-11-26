#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: MIT

test_description="test basic setup"

. `dirname $0`/sharness.sh
JOB_LIFECYCLE_PLUGIN=${SHARNESS_BUILD_DIRECTORY}/publish-jobs-dataplane2.so

test_under_flux 1
test_start_seanerbroker "seanerbroker://localhost:5398"


test_expect_success 'job lifecycle plugin is loaded and connecting to the data plane' '
    flux jobtap load ${JOB_LIFECYCLE_PLUGIN}
'

test_done
