<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: LGPL-3.0-only
SPDX-License-Identifier: MIT
-->

# Flux Dataplane Connector

Work-in-progress integration of the SeanerBUS client for the Flux event loop.
A SeanerBUS connection's socket fd is registered with the Flux reactor, so
Flux's event loop drives SeanerBUS I/O without extra threads.

The repo provides a shared library (`libdataplaneconnect`), job-manager jobtap
plugins that publish job lifecycle events to SeanerBUS (`job-manager-plugins/`),
and Flux modules that consume SeanerBUS messages (`modules/`). Everything is
loaded into a running Flux broker; there is no daemon or server binary here.

## Prerequisites

Both `flux-core` and `seanerbus` needs to be installed and available via
`pkg-config` (a development container are provided, see
(`.devcontainer/Dockerfile`).

## Build

The `job-manager-plugins/` and `modules/` directories are independent top-level
CMake projects, each embedding `libdataplaneconnect` as a static library.

Job-manager plugins:

```sh
cmake -S job-manager-plugins -B job-manager-plugins/build
cmake --build job-manager-plugins/build
```

Produces `job-manager-plugins/build/publish-jobs-dataplane2.so` (current) and
`publish-jobs-dataplane.so` (legacy v1).

Example modules:

```sh
cmake -S modules -B modules/build
cmake --build modules/build
```

Produces `modules/build/seanergys-dataplane-log.so` (logs messages on a
hello-world topic).

## Load and use

A demobus server must be running; all plugins, modules, and utilities connect
to the hardcoded endpoint `demobus://localhost:5398`.

```sh
flux start
flux jobtap load ./job-manager-plugins/build/publish-jobs-dataplane2.so
flux submit hostname
```

In another shell, watch the lifecycle events the plugin publishes:

```sh
utilities/subscribe_lifecycle2_events.py
```

To remove them again: `flux jobtap remove publish-jobs-dataplane2`.

## Tests

Sharness-based tests (job-manager-plugins project only) require a `demobus`
binary:

```sh
DEMOBUS_BIN=/path/to/demobus ctest --test-dir job-manager-plugins/build --output-on-failure
```
