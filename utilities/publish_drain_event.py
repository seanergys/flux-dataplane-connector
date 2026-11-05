#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: MIT

import seanerbus.client
from seanerbus.msg import HelloWorldResV1
import capnp
from time import sleep
import asyncio
import uuid
import sys
import json

SERVICE_UUID = uuid.UUID("8795ef1e-e299-4f3d-b709-b2777cd5aced")

async def main():
    # Connect to the SeanerBUS
    c = await seanerbus.client.Connection.connect("localhost", 5398)
    print("Publishing drain event for rank=3")
    actual_payload = {
        "targets": "3",
        "reason": "test drain"
    }
    await c.publish(SERVICE_UUID, HelloWorldResV1(json.dumps(actual_payload)).to_capnp())


asyncio.run(capnp.run(main()))

