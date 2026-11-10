#
#  Project-local sharness helpers for receiving FluxLifecycleEventV2 messages
#  from the data plane.
#
#
#  Usage:
#
#      test_start_lifecycle_sink <uuid> <url> <file>   # receive into <file>
#      wait_for_lifecycle_state  <file> <jobid> <state>
#      lifecycle_states          <file> <jobid>          # deduplicated, in order
#      lifecycle_prev_states     <file> <jobid>
#      lifecycle_count           <file> <jobid>
#      lifecycle_get             <file> <jobid> <state> <path>
#      lifecycle_len             <file> <jobid> <state> <path>
#
#  <jobid> is the *decimal* job id ('flux submit' prints f58; convert it with
#  'flux job id <f58>'). <state> is one of the lowercase schema enumerant names:
#  depend, priority, sched, run, cleanup, inactive.
#
#  <path> is a dotted path into the event, e.g. rc_state, n_cpus, .err.type or
#  r_lite.0.child_cores. lifecycle_get prints a scalar, lifecycle_len prints the
#  length of a list field.
#
#  The receiving end is scripts/lifecycle_sink.py. It has to run under the
#  system python3, where the seanerbus python bindings are installed, so
#  'flux python' is deliberately not used here; set SEANERBUS_PYTHON to
#  override. The url is passed to the sink as-is (twice: pub/sub and req/res),
#  so it must be the endpoint the plugin under test connects to - the lifecycle
#  plugins hardcode seanerbroker://localhost:5398, so there is no free port choice.

LIFECYCLE_SINK_SCRIPT=${SHARNESS_TEST_SRCDIR}/scripts/lifecycle_sink.py
SEANERBUS_PYTHON=${SEANERBUS_PYTHON:-python3}
LIFECYCLE_SINK_OUT=lifecycle_sink.out
LIFECYCLE_SINK_PID=lifecycle_sink.pid

#
#  Prereq for tests that read the data plane: a python3 that can import the
#  seanerbus client. Set here (like flux-sharness.sh sets its own prereqs) so
#  tests can gate on it or skip_all.
#
if "$SEANERBUS_PYTHON" -c 'import seanerbus.client' >/dev/null 2>&1; then
    test_set_prereq SEANERBUS_PY
fi

#
#  Kill the sink started by test_start_lifecycle_sink and remove its pid file.
#  Idempotent and safe to call when no sink is running.
#
test_stop_lifecycle_sink() {
    rc=$?
    if test -f "$LIFECYCLE_SINK_PID"; then
        kill "$(cat "$LIFECYCLE_SINK_PID")" 2>/dev/null
        rm -f "$LIFECYCLE_SINK_PID"
    fi
    return "$rc"
}

#
#  Start the lifecycle sink in the background, wait until it reports its
#  subscription is live, and arrange for it to be killed.
#
#  Reaping: cleanup() covers the passing path. sharness' test_done does not run
#  cleanup() when a test failed, so the EXIT trap covers that one. POSIX sh has
#  no 'trap -p', so the trap installed by test_start_seanerbroker cannot be read
#  back and is re-stated here explicitly (test_stop_seanerbroker is idempotent,
#  so it is safe even when no broker was started). As a last resort the sink
#  exits by itself after its --timeout, so it can never outlive the test run.
#
#  $1 - topic uuid to subscribe to
#  $2 - seanerbus url to connect to (seanerbroker://host:port)
#  $3 - file to write one JSON line per received event to
#
test_start_lifecycle_sink() {
    uuid=$1
    url=$2
    out=$3

    if test -z "$uuid" || test -z "$url" || test -z "$out"; then
        error "usage: test_start_lifecycle_sink <uuid> <url> <file> \
(see sharness.d/lifecycle-sink.sh)"
    fi

    if ! test -r "$LIFECYCLE_SINK_SCRIPT"; then
        error "lifecycle sink script not found: $LIFECYCLE_SINK_SCRIPT"
    fi

    : >"$out"
    : >"$LIFECYCLE_SINK_OUT"

    # Sink stdout is the event file, stderr is its log, stdin is /dev/null, and
    # the harness' fds 3-6 are closed: fd 5 in particular is a copy of the
    # test's stdout (see sharness.sh), so an unredirected one would let a
    # leaked daemon keep a pipe open that makes the harness or ctest wait for
    # EOF after the test process has exited.
    "$SEANERBUS_PYTHON" "$LIFECYCLE_SINK_SCRIPT" "$uuid" "$url" \
        >"$out" 2>"$LIFECYCLE_SINK_OUT" </dev/null 3>&- 4>&- 5>&- 6>&- &
    echo $! >"$LIFECYCLE_SINK_PID"
    echo "# test_start_lifecycle_sink: started sink pid $(cat "$LIFECYCLE_SINK_PID") on $url" >&5

    cleanup test_stop_lifecycle_sink

    # Reap both daemons on exit. 'die' is sharness' own EXIT trap and must
    # stay last; it needs the status test_stop_seanerbroker returns, which is
    # the one captured when the test process died.
    trap 'test_stop_lifecycle_sink; test_stop_seanerbroker; die' EXIT

    for _ in $(seq 1 100); do
        if grep -q '^subscribed$' "$LIFECYCLE_SINK_OUT" 2>/dev/null; then
            return 0
        fi
        if ! kill -0 "$(cat "$LIFECYCLE_SINK_PID")" 2>/dev/null; then
            error "lifecycle sink exited before subscribing; \
$LIFECYCLE_SINK_OUT says:$(sed 's/^/    /' "$LIFECYCLE_SINK_OUT")"
        fi
        sleep 0.1
    done

    error "lifecycle sink did not subscribe within ~10s; \
$LIFECYCLE_SINK_OUT says:$(sed 's/^/    /' "$LIFECYCLE_SINK_OUT")"
}

#
#  Block until the sink has recorded an event for <jobid> in <state>, or fail
#  after ${TEST_LIFECYCLE_TIMEOUT:-15} seconds. Needed because the plugin
#  publishes fire-and-forget: an event can still be in flight after the flux
#  command that caused it has returned.
#
#  $1 - sink file, $2 - decimal jobid, $3 - state
#
wait_for_lifecycle_state() {
    file=$1
    id=$2
    want=$3

    for _ in $(seq 1 $(( ${TEST_LIFECYCLE_TIMEOUT:-15} * 10 ))); do
        got=$(jq -es --argjson id "$id" --arg state "$want" \
            'any(.[]; .jobid == $id and .state == $state)' "$file" 2>/dev/null)
        test "$got" = "true" && return 0
        sleep 0.1
    done

    echo "# wait_for_lifecycle_state: no state=$want for jobid=$id in $file \
after ${TEST_LIFECYCLE_TIMEOUT:-15}s; captured:"
    jq -es -r '.[] | "#   jobid=\(.jobid) state=\(.state) prev=\(.prev_state) rc=\(.rc_state)"' \
        "$file" 2>/dev/null || sed -e 's/^/#   /' "$file" 2>/dev/null
    return 1
}

#
#  The states recorded for <jobid>, de-duplicated but in arrival order, space
#  separated. De-duplication matters because the plugin also handles the
#  ephemeral 'running' callback, which publishes a second state=run event.
#
#  $1 - sink file, $2 - decimal jobid
#
lifecycle_states() {
    jq -es -r --argjson id "$2" '.[] | select(.jobid == $id) | .state' "$1" |
        awk '!seen[$0]++' | paste -sd' ' -
}

#
#  Like lifecycle_states, for the prev_state of each recorded state.
#
#  $1 - sink file, $2 - decimal jobid
#
lifecycle_prev_states() {
    jq -es -r --argjson id "$2" '.[] | select(.jobid == $id) | .prev_state' "$1" |
        awk '!seen[$0]++' | paste -sd' ' -
}

#
#  How many events were recorded for <jobid>.
#
#  $1 - sink file, $2 - decimal jobid
#
lifecycle_count() {
    jq -es --argjson id "$2" '[.[] | select(.jobid == $id)] | length' "$1"
}

#
#  Split a dotted path into a jq path array. Digit-only components become
#  numbers so that list indices (r_lite.0) work.
#
#  $1 - dotted path
#
lifecycle_path() {
    jq -rn --arg path "$1" \
        '$path | ltrimstr(".") | split(".") | map(if test("^[0-9]+$") then tonumber else . end)'
}

#
#  Read one field of the first event recorded for <jobid> in <state>.
#
#  $1 - sink file, $2 - decimal jobid, $3 - state, $4 - dotted path
#
lifecycle_get() {
    jq -es -r --argjson id "$2" --arg state "$3" \
        --argjson path "$(lifecycle_path "$4")" \
        '[.[] | select(.jobid == $id and .state == $state)]
         | first | getpath($path)' "$1"
}

#
#  Like lifecycle_get, but report the length of the field (for the list fields
#  node_list and r_lite). Missing/null reads as 0.
#
#  $1 - sink file, $2 - decimal jobid, $3 - state, $4 - dotted path
#
lifecycle_len() {
    jq -es --argjson id "$2" --arg state "$3" \
        --argjson path "$(lifecycle_path "$4")" \
        '[.[] | select(.jobid == $id and .state == $state)]
         | first | getpath($path) // [] | length' "$1"
}
