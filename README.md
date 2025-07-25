<br />

**Scaler provides a simple, efficient, and reliable way to perform distributed computing** using a centralized scheduler,
with a stable and language-agnostic protocol for client and worker communications.

```python
import math
from scaler import Client

with Client(address="tcp://127.0.0.1:2345") as client:
    # Compute a single task using `.submit()`
    future = client.submit(math.sqrt, 16)
    print(future.result())  # 4

    # Submit multiple tasks with `.map()`
    results = client.map(math.sqrt, [(i,) for i in range(100)])
    print(sum(results))  # 661.46
```

Scaler is a suitable Dask replacement, offering significantly better scheduling performance for jobs with a large number
of lightweight tasks while improving on load balancing, messaging, and deadlocks.

## Features

- Distributed computing across **multiple cores and multiple servers**
- **Python** reference implementation, with **language-agnostic messaging protocol** built on top of
  [Cap'n Proto](https://capnproto.org/) and [ZeroMQ](https://zeromq.org)
- **Graph** scheduling, which supports [Dask](https://www.dask.org)-like graph computing, with optional [GraphBLAS](https://graphblas.org)
  support for very large graph tasks
- **Automated load balancing**, which automatically balances load from busy workers to idle workers, ensuring uniform utilization across workers
- **Automated task recovery** from worker-related hardware, OS, or network failures
- Support for **nested tasks**, allowing tasks to submit new tasks
- `top`-like **monitoring tools**
- GUI monitoring tool

## Installation

Scaler is available on PyPI and can be installed using any compatible package manager.

```bash
$ pip install scaler

# or with graphblas and uvloop support
$ pip install scaler[graphblas,uvloop]
```

## Quick Start

The official documentation.

Scaler has 3 main components:

- A **scheduler**, responsible for routing tasks to available computing resources.
- A set of **workers** that form a _cluster_. Workers are independent computing units, each capable of executing a single task.
- **Clients** running inside applications, responsible for submitting tasks to the scheduler.

### Start local scheduler and cluster programmatically in code

A local scheduler and a local set of workers can be conveniently started using `SchedulerClusterCombo`:

```python
from scaler import SchedulerClusterCombo

cluster = SchedulerClusterCombo(address="tcp://127.0.0.1:2345", n_workers=4)

...

cluster.shutdown()
```

This will start a scheduler with 4 workers on port `2345`.

### Setting up a computing cluster from the CLI

The scheduler and workers can also be started from the command line with `scaler_scheduler` and `scaler_cluster`.

First, start the Scaler scheduler:

```bash
$ scaler_scheduler tcp://127.0.0.1:2345
[INFO]2023-03-19 12:16:10-0400: logging to ('/dev/stdout',)
[INFO]2023-03-19 12:16:10-0400: use event loop: 2
[INFO]2023-03-19 12:16:10-0400: Scheduler: monitor address is ipc:///tmp/127.0.0.1_2345_monitor
...
```

Then, start a set of workers (a.k.a. a Scaler _cluster_) that connects to the previously started scheduler:

```bash
$ scaler_cluster -n 4 tcp://127.0.0.1:2345
[INFO]2023-03-19 12:19:19-0400: logging to ('/dev/stdout',)
[INFO]2023-03-19 12:19:19-0400: ClusterProcess: starting 4 workers, heartbeat_interval_seconds=2, object_retention_seconds=3600
[INFO]2023-03-19 12:19:19-0400: Worker[0] started
[INFO]2023-03-19 12:19:19-0400: Worker[1] started
[INFO]2023-03-19 12:19:19-0400: Worker[2] started
[INFO]2023-03-19 12:19:19-0400: Worker[3] started
...
```

Multiple Scaler clusters can be connected to the same scheduler, providing distributed computation over multiple servers.

`-h` lists the available options for the scheduler and the cluster executables:

```bash
$ scaler_scheduler -h
$ scaler_cluster -h
```

### Submitting Python tasks using the Scaler client

Knowing the scheduler address, you can connect and submit tasks from a client in your Python code:

```python
from scaler import Client


def square(value: int):
    return value * value


with Client(address="tcp://127.0.0.1:2345") as client:
    future = client.submit(square, 4) # submits a single task
    print(future.result()) # 16
```

`Client.submit()` returns a standard Python future.

## Graph computations

Scaler also supports graph tasks, for example:

```python
from scaler import Client


def inc(i):
    return i + 1


def add(a, b):
    return a + b


def minus(a, b):
    return a - b


graph = {
    "a": 2,
    "b": 2,

    # the input to task c is the output of task a
    "c": (inc, "a"),  # c = a + 1 = 2 + 1 = 3
    "d": (add, "a", "b"),  # d = a + b = 2 + 2 = 4
    "e": (minus, "d", "c")  # e = d - c = 4 - 3 = 1
}

with Client(address="tcp://127.0.0.1:2345") as client:
    result = client.get(graph, keys=["e"])
    print(result)  # {"e": 1}
```

## Nested computations

Scaler allows tasks to submit new tasks while being executed. Scaler also supports recursive task calls.

```python
from scaler import Client


def fibonacci(client: Client, n: int):
    if n == 0:
        return 0
    elif n == 1:
        return 1
    else:
        a = client.submit(fibonacci, client, n - 1)
        b = client.submit(fibonacci, client, n - 2)
        return a.result() + b.result()


with Client(address="tcp://127.0.0.1:2345") as client:
    future = client.submit(fibonacci, client, 8)
    print(future.result()) # 21
```

## IBM Spectrum Symphony integration

A Scaler scheduler can interface with IBM Spectrum Symphony to provide distributed computing across Symphony clusters.

```bash
$ scaler_symphony_cluster tcp://127.0.0.1:2345 ScalerService --base-concurrency 4
```

This will start a Scaler worker that connects to the Scaler scheduler at `tcp://127.0.0.1:2345` and uses the Symphony
service `ScalerService` to submit tasks.

### Symphony service

A service must be deployed in Symphony to handle the task submission.

<details>

<summary>Here is an example of a service that can be used</summary>

```python
class Message(soamapi.Message):
    def __init__(self, payload: bytes = b""):
        self.__payload = payload

    def set_payload(self, payload: bytes):
        self.__payload = payload

    def get_payload(self) -> bytes:
        return self.__payload

    def on_serialize(self, stream):
        payload_array = array.array("b", self.get_payload())
        stream.write_byte_array(payload_array, 0, len(payload_array))

    def on_deserialize(self, stream):
        self.set_payload(stream.read_byte_array("b"))

class ServiceContainer(soamapi.ServiceContainer):
    def on_create_service(self, service_context):
        return

    def on_session_enter(self, session_context):
        return

    def on_invoke(self, task_context):
        input_message = Message()
        task_context.populate_task_input(input_message)

        fn, *args = cloudpickle.loads(input_message.get_payload())
        output_payload = cloudpickle.dumps(fn(*args))

        output_message = Message(output_payload)
        task_context.set_task_output(output_message)

    def on_session_leave(self):
        return

    def on_destroy_service(self):
        return
```

</details>

### Nested tasks

Nested task originating from Symphony workers must be able to reach the Scaler scheduler. This might require
modifications to the network configuration.

Nested tasks can also have unpredictable resource usage and runtimes, which can cause Symphony to prematurely kill
tasks. It is recommended to be conservative when provisioning resources and limits, and monitor the cluster status
closely for any abnormalities.

### Base concurrency

Base concurrency is the maximum number of unnested tasks that can be executed concurrently. It is possible to surpass
this limit by submitting nested tasks which carry a higher priority. **Important**: If your workload contains nested
tasks the base concurrency should be set to a value less to the number of cores available on the Symphony worker or else
deadlocks may occur.

A good heuristic for setting the base concurrency is to use the following formula:

```
base_concurrency = number_of_cores - deepest_nesting_level
```

where `deepest_nesting_level` is the deepest nesting level a task has in your workload. For instance, if you have a workload that has
a base task that calls a nested task that calls another nested task, then the deepest nesting level is 2.

## Performance

### uvloop

By default, Scaler uses Python's built-in `asyncio` event loop.
For better async performance, you can install uvloop (`pip install uvloop`) and supply `uvloop` for the CLI argument
`--event-loop` or as a keyword argument for `event_loop` in Python code when initializing the scheduler.

```bash
scaler_scheduler --event-loop uvloop tcp://127.0.0.1:2345
```

```python
from scaler import SchedulerClusterCombo

scheduler = SchedulerClusterCombo(address="tcp://127.0.0.1:2345", event_loop="uvloop", n_workers=4)
```

## Monitoring

### From the CLI

Use `scaler_top` to connect to the scheduler's monitor address (printed by the scheduler on startup) to see
diagnostics/metrics information about the scheduler and its workers.

```bash
$ scaler_top ipc:///tmp/127.0.0.1_2345_monitor
```

It will look similar to `top`, but provides information about the current Scaler setup:

```bash
scheduler          | task_manager         |   scheduler_sent         | scheduler_received
      cpu     0.0% |   unassigned       0 |   ObjectResponse      24 |          Heartbeat 183,109
      rss 37.1 MiB |      running       0 |         TaskEcho 200,000 |    ObjectRequest      24
                   |      success 200,000 |             Task 200,000 |               Task 200,000
                   |       failed       0 |       TaskResult 200,000 |         TaskResult 200,000
                   |     canceled       0 |   BalanceRequest       4 |    BalanceResponse       4
--------------------------------------------------------------------------------------------------
Shortcuts: worker[n] cpu[c] rss[m] free[f] working[w] queued[q]

Total 10 worker(s)
                 worker agt_cpu agt_rss [cpu]   rss free sent queued | object_id_to_tasks
W|Linux|15940|3c9409c0+    0.0%   32.7m  0.0% 28.4m 1000    0      0 |
W|Linux|15946|d6450641+    0.0%   30.7m  0.0% 28.2m 1000    0      0 |
W|Linux|15942|3ed56e89+    0.0%   34.8m  0.0% 30.4m 1000    0      0 |
W|Linux|15944|6e7d5b99+    0.0%   30.8m  0.0% 28.2m 1000    0      0 |
W|Linux|15945|33106447+    0.0%   31.1m  0.0% 28.1m 1000    0      0 |
W|Linux|15937|b031ce9a+    0.0%   31.0m  0.0% 30.3m 1000    0      0 |
W|Linux|15941|c4dcc2f3+    0.0%   30.5m  0.0% 28.2m 1000    0      0 |
W|Linux|15939|e1ab4340+    0.0%   31.0m  0.0% 28.1m 1000    0      0 |
W|Linux|15938|ed582770+    0.0%   31.1m  0.0% 28.1m 1000    0      0 |
W|Linux|15943|a7fe8b5e+    0.0%   30.7m  0.0% 28.3m 1000    0      0 |
```

- **scheduler** section shows scheduler resource usage
- **task_manager** section shows count for each task status
- **scheduler_sent** section shows count for each type of messages scheduler sent
- **scheduler_received** section shows count for each type of messages scheduler received
- **function_id_to_tasks** section shows task count for each function used
- **worker** section shows worker details, , you can use shortcuts to sort by columns, and the \* in the column header shows
  which column is being used for sorting - `agt_cpu/agt_rss` means cpu/memory usage of worker agent - `cpu/rss` means cpu/memory usage of worker - `free` means number of free task slots for this worker - `sent` means how many tasks scheduler sent to the worker - `queued` means how many tasks worker received and queued

### From the web UI

`scaler_ui` provides a web monitoring interface for Scaler.

```bash
$ scaler_ui ipc:///tmp/127.0.0.1_2345_monitor --port 8081
```

This will open a web server on port `8081`.
