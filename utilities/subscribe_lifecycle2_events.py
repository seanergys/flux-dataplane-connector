#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: LGPL-3.0-only

import seanerbus.client
from seanerbus.msg import msg_capnp
import capnp
import asyncio
import uuid
import sys
import json

async def main():
    # Connect to the SeanerBUS
    c = seanerbus.client.Connection(
        "demobus://localhost:5398", "demobus://localhost:5398"
    )

    # Subscribe to the UUID given on the command line
    next_message = await c.subscribe("834d23e1-fd27-4537-b467-9702ef9ca8a2")


    while True:
        # read incoming messages from the bus, interpreting them as TimeValueV1's 
        msg = await next_message()
        with msg_capnp.FluxLifecycleEventV2.from_bytes(msg.payload()) as payload:
            payload = payload.to_dict()
            print("```json")
            print(json.dumps(payload, indent=4))
            print("```")

asyncio.run(capnp.run(main()))
