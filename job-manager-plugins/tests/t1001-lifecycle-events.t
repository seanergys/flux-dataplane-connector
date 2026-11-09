#!/usr/bin/env bash

test_description="test the job lifecycle plugin publishes lifecycle events to the data plane"

. `dirname $0`/sharness.sh

JOB_LIFECYCLE_PLUGIN=${SHARNESS_BUILD_DIRECTORY}/publish-jobs-dataplane2.so
# The lifecycle plugins hardcode this endpoint, so demobus and the sink have to
# use exactly it (see sharness.d/lifecycle-sink.sh).
DATA_PLANE_URL=demobus://localhost:5398
JOB_EVENTS_UUID=834d23e1-fd27-4537-b467-9702ef9ca8a2
EVENTS=lifecycle_events.ndjson

# flux-sharness.sh unsets this for all tests, so re-export it. It reaches the
# jobtap plugin through the broker's environment, which makes the plain f58 form
# the plugin publish use only [0-9A-Za-z] (see fluid_encode), so the sink output
# stays readable and free of shell metacharacters.
export FLUX_F58_FORCE_ASCII=1

# These three are what the tests need beyond flux itself; skip rather than fail
# when the data plane pieces are not installed.
if test -z "$DEMOBUS_BIN"; then
    skip_all='DEMOBUS_BIN not set, skipping data plane lifecycle tests'
    test_done
fi

if ! test_have_prereq SEANERBUS_PY; then
    skip_all="cannot import seanerbus.client with '$SEANERBUS_PYTHON', \
skipping data plane lifecycle tests"
    test_done
fi

# Two ranks so the -N2 job below is schedulable with one core per rank.
test_under_flux 2
test_start_demobus "$DATA_PLANE_URL"
test_start_lifecycle_sink "$JOB_EVENTS_UUID" "$DATA_PLANE_URL" "$EVENTS"

cleanup 'flux cancel --all || true'
cleanup 'flux jobtap remove publish-jobs-dataplane2 || true'

# Events are correlated by *decimal* job id, which is what flux submit prints
# here (flux job id normalizes either form). It cannot be correlated on the f58
# string: the plugin publishes the plain form (flux_job_id_encode "f58plain"),
# which differs from the compressed form the CLI prints.
test_expect_success 'lifecycle plugin publishes one event per job state transition' '
    flux jobtap load $JOB_LIFECYCLE_PLUGIN &&
    jid=$(flux submit -N2 -t 30s hostname) &&
    flux job id "$jid" >job_a.id &&
    id=$(cat job_a.id) &&
    wait_for_lifecycle_state $EVENTS $id inactive &&
    test $(lifecycle_count $EVENTS $id) = 6 &&
    test "$(lifecycle_states $EVENTS $id)" = \
        "depend priority sched run cleanup inactive" &&
    test "$(lifecycle_prev_states $EVENTS $id)" = \
        "new depend priority sched run cleanup"
'

test_expect_success 'lifecycle events carry jobspec, user, and allocation data' '
    id=$(cat job_a.id) &&
    test "$(lifecycle_get $EVENTS $id sched n_slots)" = 2 &&
    test "$(lifecycle_get $EVENTS $id sched n_cpus)" = 2 &&
    test "$(lifecycle_get $EVENTS $id sched job_display_name)" = hostname &&
    test "$(lifecycle_get $EVENTS $id sched owner)" = $(id -u) &&
    test "$(lifecycle_get $EVENTS $id sched owner_name)" = $(id -un) &&
    test "$(lifecycle_get $EVENTS $id sched requested_walltime_us)" = 30000000 &&
    # The fake test instance gives both ranks the same hostname, so R comes back
    # rank-collapsed: one nodelist entry and one R_lite entry covering ranks 0-1.
    test -n "$(lifecycle_get $EVENTS $id run node_list.0)" &&
    test "$(lifecycle_get $EVENTS $id run r_lite.0.ranks)" = "0-1" &&
    test -n "$(lifecycle_get $EVENTS $id run r_lite.0.child_cores)" &&
    start=$(lifecycle_get $EVENTS $id run start_time_us) &&
    test $start -gt 0 &&
    test "$(lifecycle_get $EVENTS $id cleanup start_time_us)" = $start &&
    test $(lifecycle_get $EVENTS $id cleanup finish_time_us) -gt 0
'

test_expect_success 'finish events map the job outcome to rc_state and err' '
    id=$(flux job id "$(flux submit hostname)") &&
    wait_for_lifecycle_state $EVENTS $id cleanup &&
    test "$(lifecycle_get $EVENTS $id cleanup rc_state)" = completed &&
    jid=$(flux submit -t 30s sleep 300) &&
    id=$(flux job id "$jid") &&
    flux cancel "$jid" &&
    wait_for_lifecycle_state $EVENTS $id cleanup &&
    test "$(lifecycle_get $EVENTS $id cleanup rc_state)" = cancelled &&
    jid=$(flux submit -t 30s sleep 300) &&
    id=$(flux job id "$jid") &&
    flux job raise --severity=0 --type=user "$jid" -- "injected by t1001" &&
    wait_for_lifecycle_state $EVENTS $id cleanup &&
    test "$(lifecycle_get $EVENTS $id cleanup rc_state)" = failed &&
    test "$(lifecycle_get $EVENTS $id cleanup .err.type)" = user
'

test_done
