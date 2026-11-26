#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: LGPL-3.0-only
# SPDX-License-Identifier: MIT

import seanerbus.client
from seanerbus.msg import HelloWorldResV1
import capnp
from time import sleep
import asyncio
import uuid
import sys

SERVICE_UUID = uuid.UUID("f1c30d2a-62c3-4935-90d1-b95a85e81e4b")

async def main():
    # Connect to the SeanerBUS
    c = await seanerbus.client.Connection.connect("localhost", 5398)

    time = 0
    while True:
        # Publish a TimeValueV1 to the UUID given on the command line
        await c.publish(SERVICE_UUID, HelloWorldResV1(f"Hello World Publishing Service at Timestep {time}").to_capnp())
        time = time + 2
        sleep(2)

asyncio.run(capnp.run(main()))

