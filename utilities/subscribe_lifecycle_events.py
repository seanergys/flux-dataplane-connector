#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: MIT

import seanerbus.client
from seanerbus.msg import FluxLifecycleEventV1
import capnp
import asyncio
import uuid
import sys
import json

async def main():
    # Connect to the SeanerBUS
    c = await seanerbus.client.Connection.connect("localhost", 5398)

    # Subscribe to the UUID given on the command line
    await c.subscribe(uuid.UUID(sys.argv[1]))

    while True:
        # read incoming messages from the bus, interpreting them as TimeValueV1's
        ev = FluxLifecycleEventV1.from_capnp(await c.read_msg())
        payload_str = "".join(ev.data)
        payload = json.loads(payload_str)
        print("```json")
        print(json.dumps(payload, indent=4))
        print("```")


if len(sys.argv) != 2:
    print("subscribe_lifecycle_events.py UUID")
    exit(-1)

asyncio.run(capnp.run(main()))
