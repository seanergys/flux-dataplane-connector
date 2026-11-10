#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: LGPL-3.0-only

"""Print every FluxLifecycleEventV2 on a SeanerBUS topic as one JSON line.

This is the receiving end for the job lifecycle plugin tests: it subscribes to
a topic and writes one normalized JSON object per message to stdout, so a
sharness test can grep for the event it is waiting for.

    python3 lifecycle_sink.py <uuid> <endpoint> [--timeout SECONDS]
    python3 lifecycle_sink.py 834d23e1-fd27-4537-b467-9702ef9ca8a2 demobus://localhost:5398

Must run under the *system* python3, which is where the seanerbus python
bindings are installed (not flux python's interpreter).

Field names are the schema's, rewritten to snake_case, with a _us suffix added
to the timestamp fields (they are microseconds on the wire) so that tests read
well. Prints "subscribed" on stderr once the subscription is live, which is how
test_start_lifecycle_sink knows the sink is ready.
"""

import argparse
import asyncio
import json
import sys

import capnp
import seanerbus.client
from seanerbus.msg import msg_capnp

# Schema name -> name used in the JSON output. Anything not listed is passed
# through unchanged, so a new schema field still shows up in the sink output.
RENAMES = {
    "jobidF58": "jobid_f58",
    "prevState": "prev_state",
    "nSlots": "n_slots",
    "nNodes": "n_nodes",
    "nGpus": "n_gpus",
    "nCpus": "n_cpus",
    "ownerName": "owner_name",
    "jobName": "job_name",
    "jobDisplayName": "job_display_name",
    "submitTime": "submit_time_us",
    "requestedWalltime": "requested_walltime_us",
    "nodeList": "node_list",
    "rLite": "r_lite",
    "childCores": "child_cores",
    "childGpus": "child_gpus",
    "startTime": "start_time_us",
    "rcState": "rc_state",
    "finishTime": "finish_time_us",
    "errObj": "err",
}


def normalize(value):
    """Rewrite a decoded capnp value into plain JSON-serializable types."""
    if isinstance(value, dict):
        return {
            RENAMES.get(key, key): normalize(val) for key, val in value.items()
        }
    if isinstance(value, (list, tuple)):
        return [normalize(entry) for entry in value]
    if isinstance(value, (bool, int, float)) or value is None:
        return value
    # Text and enum values are capnp wrappers around the schema's strings.
    return str(value)


async def drain(handle):
    while True:
        msg = await handle()
        if msg.payload_type_as_int() != msg_capnp.MessageTypes.fluxLifecycleEventV2:
            print(
                f"lifecycle_sink: ignoring message of payload type"
                f" {msg.payload_type_as_int()}",
                file=sys.stderr,
                flush=True,
            )
            continue
        with msg_capnp.FluxLifecycleEventV2.from_bytes(msg.payload()) as payload:
            print(json.dumps(normalize(payload.to_dict())), flush=True)


async def main(args):
    connection = seanerbus.client.Connection(args.endpoint, args.endpoint)
    handle = await connection.subscribe(args.uuid)
    print("subscribed", file=sys.stderr, flush=True)
    try:
        await asyncio.wait_for(drain(handle), timeout=args.timeout)
    except asyncio.TimeoutError:
        print(f"lifecycle_sink: {args.timeout}s timeout reached", file=sys.stderr, flush=True)
        return 0
    return 0


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("uuid", help="topic to subscribe to")
parser.add_argument("endpoint", help="seanerbus endpoint, e.g. demobus://localhost:5398")
parser.add_argument(
    "--timeout",
    type=float,
    default=300.0,
    help="exit after this many seconds (default 300) so a leaked sink cannot outlive a test run",
)

try:
    sys.exit(asyncio.run(capnp.run(main(parser.parse_args()))))
except Exception as exc:  # noqa: BLE001 - report, do not dump a traceback into a TAP log
    print(f"lifecycle_sink: {exc}", file=sys.stderr, flush=True)
    sys.exit(1)
