#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: LGPL-3.0-only
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
    await c.subscribe(uuid.UUID("f50de671-c6c4-49a5-b684-22d2c321f605"))

    while True:
        # read incoming messages from the bus, interpreting them as TimeValueV1's
        ev = FluxLifecycleEventV1.from_capnp(await c.read_msg())
        payload_str = "".join(ev.data)
        payload = json.loads(payload_str)
        print("```json")
        print(json.dumps(payload, indent=4))
        print("```")

asyncio.run(capnp.run(main()))
