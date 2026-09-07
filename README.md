# Operating System Model — *Plug & Pray*

A fully distributed operating system simulation built in C, implementing core OS concepts across seven independent modules that communicate via TCP sockets. Developed as the capstone project for the Operating Systems course at UTN FRBA (1C 2026).

---

## Overview

The system simulates a real OS architecture: a Kernel Scheduler orchestrates process lifecycle and scheduling, a Kernel Memory manages segmented memory across hot-pluggable Memory Sticks, one or more CPUs execute a custom instruction set, a SWAP module handles process suspension, and IO modules handle user interaction. All modules run as separate processes, potentially on different machines, and communicate over the network.

<img width="592" height="423" alt="image" src="https://github.com/user-attachments/assets/ec8ccd2a-1edd-435a-999c-90d5d00da5cc" />

---

## Modules

### Kernel Scheduler
The central coordinator of the system. Implements a full **7-state process model** (NEW → READY → EXEC → BLOCK → SUSP.BLOCK → SUSP.READY → EXIT) with three scheduling tiers:

- **Long-term scheduler**: admits processes from NEW to READY.
- **Medium-term scheduler**: manages BLOCK ↔ SUSP.BLOCK and SUSP.READY ↔ READY transitions based on a configurable suspension timeout, with priority-ordered swap-in when memory becomes available.
- **Short-term scheduler**: dispatches processes to CPUs using one of three algorithms:
  - **FIFO**: non-preemptive, arrival order.
  - **Round Robin (RR)**: preemptive with configurable quantum.
  - **Multilevel Queue (CMN)**: non-feedback, configurable algorithm per level (FIFO or RR), with optional preemption between queues.

Also manages **mutex syscalls** with a FIFO waiting queue and **priority inheritance** to prevent priority inversion: when a higher-priority process blocks on a mutex held by a lower-priority one, the owner temporarily inherits the waiter's priority until it releases the lock.

Handles **compaction-triggered eviction**: when Kernel Memory requests compaction, the scheduler evicts all CPUs, waits for confirmation, and places evicted processes at the *front* of their respective ready queues.

### Kernel Memory
Manages two distinct memory spaces:

- **Instruction & context memory**: stores process programs (custom pseudocode files) and CPU execution contexts (registers + segment table per PID). Serves instruction fetch and context transfer requests from CPUs.
- **User/data memory**: implements **pure segmentation** across one or more Memory Sticks. Supports Best Fit and Worst Fit allocation strategies. Handles compaction when free space exists but is non-contiguous — reorganizes all segments to the beginning of memory and updates all segment tables.
- Coordinates with SWAP for process suspension/resumption (segment-by-segment transfer).
- Notifies the Kernel Scheduler on memory events: free space updates, new Memory Stick connections, and memory corruption on disconnection.

### CPU
Executes a **4-stage instruction cycle** (Fetch → Decode → Execute → Check Interrupt) for a custom ISA. Implements:

- 10 registers: `PC`, `AX`/`BX`/`CX`/`DX` (8-bit), `EAX`/`EBX`/`ECX`/`EDX` (32-bit), `SI`/`DI` (logical address registers).
- An **MMU** that translates logical addresses to physical addresses using the segment table: `segment = ⌊addr / max_segment_size⌋`, `offset = addr % max_segment_size`. Raises SEGFAULT if access exceeds segment bounds.
- Support for cross-Memory-Stick reads/writes, splitting and consolidating requests transparently.
- Async interrupt handling: the Kernel Scheduler can inject a preemption interrupt at any point; the CPU finishes the current instruction and checks for it at the end of the cycle before the next fetch.
- Syscall handling: saves context to Kernel Memory and yields control back to the Kernel Scheduler, which handles the syscall and re-dispatches when done.

**Instruction set includes:**
`NOOP`, `SET`, `MOV_IN`, `MOV_OUT`, `SUM`, `SUB`, `JNZ`, `COPY_MEM`, `MEM_ALLOC`, `MEM_FREE`, `MUTEX_CREATE`, `MUTEX_LOCK`, `MUTEX_UNLOCK`, `SLEEP`, `STDIN`, `STDOUT`, `INIT_PROC`, `EXIT`.

### Memory Stick
Simulates a physical RAM chip. Allocates its capacity via `malloc()` at startup and serves read/write requests from CPUs and Kernel Memory. Hot-pluggable: new sticks can connect at runtime, expanding total addressable memory. On disconnection, triggers a BSOD across the system.

### IO Module
Three types, each running as a single-threaded process:

- **SLEEP**: executes `usleep()` for the requested duration.
- **STDIN**: reads user keyboard input and writes it to Kernel Memory (pads with `\0` if too short, truncates if too long).
- **STDOUT**: reads data from Kernel Memory and prints it to screen and log.

### SWAP
Persists suspended process segments to a binary file divided into fixed-size blocks. Block allocation is managed entirely by Kernel Memory; SWAP only reads and writes individual blocks on request.

### Utils
Shared library used across all modules: serialization/deserialization, socket communication primitives, package framing, and common data structures.

---

## Stack

- **Language:** C (C11)
- **Concurrency:** pthreads, POSIX semaphores
- **Communication:** TCP sockets (custom binary framing protocol)
- **Libraries:** [so-commons-library](https://github.com/sisoputnfrba/so-commons-library) (UTN FRBA) — lists, logging, config parsing
- **Build:** GNU Make (per-module Makefiles)
- **Platform:** Ubuntu 24.04 LTS

---

## Getting Started

### Prerequisites

Install the shared commons library:

```bash
git clone https://github.com/sisoputnfrba/so-commons-library
cd so-commons-library
make debug
make install
```

### Building

Each module compiles independently:

```bash
cd kernel_scheduler && make
cd kernel_memory && make
cd cpu && make
cd memory_stick && make
cd io && make
cd swap && make
```

### Running

Start modules in dependency order. Each takes a config file and optionally extra arguments:

```bash
# 1. Kernel Memory (no dependencies)
./kernel_memory/bin/kernel_memory kernel_memory/config.cfg

# 2. Memory Stick(s) — connect to Kernel Memory
./memory_stick/bin/memory_stick memory_stick/config.cfg 64   # 64-byte stick

# 3. SWAP
./swap/bin/swap swap/config.cfg

# 4. Kernel Scheduler — connect to Kernel Memory, then wait for CPUs
./kernel_scheduler/bin/kernel_scheduler kernel_scheduler/config.cfg process0.prc

# 5. CPU(s)
./cpu/bin/cpu cpu/config.cfg 0   # CPU with ID 0

# 6. IO module(s)
./io/bin/io io/config.cfg SLEEP
./io/bin/io io/config.cfg STDIN
./io/bin/io io/config.cfg STDOUT
```

### Configuration

Key parameters per module (all configurable without recompiling):

| Module | Key parameters |
|---|---|
| `kernel_scheduler` | `PLANIFICATION_ALGORITHM` (FIFO/RR/CMN), `QUEUES_ALGORITHMS`, `RR_QUANTUM`, `QUEUE_PREEMPTION`, `SUSPENSION_TIMEOUT` |
| `kernel_memory` | `ALLOCATION_STRATEGY` (BEST/WORST), `SEGMENT_MAX_SIZE`, `INSTRUCTION_DELAY`, `COMPACTION_DELAY`, `SCRIPTS_BASEPATH` |
| `memory_stick` | `MEMORY_DELAY` |
| `swap` | `SWAP_FILE_PATH`, `SWAP_FILE_SIZE`, `BLOCK_SIZE` |

---

## Pseudocode ISA

Processes are defined as plain text files with one instruction per line. Example:

```
MUTEX_CREATE LOCK_1
MEM_ALLOC 0 64
MUTEX_LOCK LOCK_1
SET AX 10
SET BX 5
SUM AX BX
MUTEX_UNLOCK LOCK_1
MEM_FREE 0
EXIT
```

`INIT_PROC` can spawn child processes from within a running process, enabling multi-process test scenarios:

```
INIT_PROC child_a.prc 1   # spawn child with priority 1
INIT_PROC child_b.prc 3   # spawn child with priority 3
EXIT
```

---

## Academic Context

This project was developed as the final assignment (*Trabajo Práctico Cuatrimestral*) for the Operating Systems course at UTN FRBA, 2026 Q1. The system was designed and implemented as a team project over the full semester, with iterative deliveries at three checkpoints.

The spec intentionally simplifies or alters real OS concepts for didactic purposes (e.g., decimal-based MMU addressing, named mutex syscalls, single-file SWAP). All simplifications are documented in the original course spec.

The full project specification (in Spanish) is: https://docs.google.com/document/d/1T0CglFoVRDHWXKHvNNXl62RryVkhWG9rmGyb7NDilCc/edit?pli=1&tab=t.0
