#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: LGPL-3.0-only
# SPDX-License-Identifier: MIT

import seanerbus.client
from seanerbus.msg import FluxClassifyJobReqV1, FluxClassifyJobResV1
import capnp
import asyncio
import uuid
import sys
import random


async def main():
    # Connect to the SeanerBUS
    c = await seanerbus.client.Connection.connect("localhost", 5398)

    # Registers as a request-responder with the broker
    await c.register(uuid.UUID("70bad6fe-3bdb-4926-b3d6-ef6422c25588"))

    while True:
        # Wait for a message
        msg = await c.read_msg()

        # Try to decode its payload as a HelloWorldReqV1
        payload = FluxClassifyJobReqV1.from_capnp(msg)

        print(f"Got a JobClassificationReqV1: {payload.jobspec}")
        # Respond to it with a HelloWorldResV1

        classification = random.random()

        resp_payload = f'{{"classification": {classification}}}'

        await c.respond_to(
                msg, FluxClassifyJobResV1(resp_payload).to_capnp()
        )

asyncio.run(capnp.run(main()))
