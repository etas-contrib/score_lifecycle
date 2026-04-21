# Memory Comparison: Custom IPC vs mw::com

## Test Setup

- Both scenarios run the same smoketest with two processes: **LaunchManager** (LM) and **StateManager** (SM)
- Both mw::com and custom IPC are configured to reserve space for up to 10 requests.
- For mw::com: Single service instance offered by launch manager
- Launch Manager thread pool configured to 12 worker threads for both scenarios
- Scenario 1: Custom IPC implementation for inter-process communication
- Scenario 2: mw::com middleware for inter-process communication
- Thread stack size set to 64 kB (`ulimit -s 64`) to isolate heap usage from thread stack reservations in VmData
- Data captured from `/proc/<PID>/status` at steady state

## Per-Process Breakdown

| Metric | Custom IPC LM | mw::com LM | Custom IPC SM | mw::com SM |
|--------|--------------|------------|---------------|------------|
| **VmRSS** (physical RAM) | 4,000 kB | 6,836 kB | 3,640 kB | 6,716 kB |
| RssAnon (heap/stack) | 464 kB | 564 kB | 180 kB | 312 kB |
| RssFile (mapped files/libs) | 3,532 kB | 6,272 kB | 3,456 kB | 6,392 kB |
| RssShmem (shared memory) | 4 kB | 0 kB ?? | 4 kB | 12 kB |
| **VmSize** (virtual) | 204,084 kB | 274,040 kB | 71,864 kB | 75,900 kB |
| VmPeak (peak virtual) | 204,088 kB | 339,580 kB | 137,400 kB | 141,288 kB |
| VmData (heap) | 1,756 kB | 2,292 kB | 456 kB | 808 kB |
| VmStk (main thread stack) | 64 kB | 64 kB | 64 kB | 64 kB |
| VmExe (code segment) | 1,484 kB | 3,508 kB | 1,484 kB | 3,364 kB |
| VmLib (shared libs) | 2,304 kB | 2,304 kB | 2,304 kB | 2,304 kB |
| Threads | 15 | 19 | 2 | 5 |

## Combined Totals (LM + SM)

| Metric | Custom IPC | mw::com | Delta | Overhead |
|--------|-----------|---------|-------|----------|
| **VmRSS** | **7,640 kB** | **13,552 kB** | **+5,912 kB** | **+77%** |
| RssAnon | 644 kB | 876 kB | +232 kB | +36% |
| RssFile | 6,988 kB | 12,664 kB | +5,676 kB | +81% |
| RssShmem | 8 kB | 12 kB | +4 kB | +50% |
| **VmSize** | 275,948 kB | 349,940 kB | +73,992 kB | +27% |
| VmData (heap only) | 2,212 kB | 3,100 kB | +888 kB | +40% |
| VmExe | 2,968 kB | 6,872 kB | +3,904 kB | +132% |
| Threads | 17 | 24 | +7 | +41% |

## Analysis

### Physical memory (VmRSS): +5.9 MB (+77%)

The mw::com implementation uses 13.2 MB of physical RAM compared to 7.5 MB for the custom IPC implementation.

### RssFile dominates the overhead: +5.7 MB (96% of total RSS delta)

The mw::com middleware libraries mapped into each process account for nearly all the physical memory increase. Both LM and SM each load ~2.8 MB of additional library pages. These pages are **shareable** across all mw::com-using processes via the kernel page cache — on a system with N mw::com processes, this cost is paid once, not N times.

### RssAnon (private heap/stack): +232 kB (+36%)

Negligible overhead. mw::com's internal runtime state adds only ~100 kB per process in private heap at steady state. This is the true non-shareable per-process cost.

### VmData (heap, with thread stacks eliminated): +888 kB (+40%)

With `ulimit -s 64`, VmData reflects actual heap reservations rather than thread stack virtual memory. mw::com reserves ~888 kB more heap across both processes — modest and proportional to the additional proxy/skeleton data structures.

### Binary size (VmExe): +3.9 MB (+132%)

Both binaries more than double in code size due to mw::com generated proxy/skeleton code. This is a fixed per-binary cost.

### Threads: +7 (+41%)

LM: 15 -> 19 (+4), SM: 2 -> 5 (+3). Additional threads are used for mw::com's I/O reactor, service discovery, and event dispatching.

### VmSize (virtual): +74 MB (+27%)

Mostly mmap'd regions from mw::com shared libraries' virtual address reservations. These are not backed by physical RAM and do not represent actual memory consumption.

## Note on VmData and Thread Stack Size

With the default stack size (`ulimit -s 8192`, i.e. 8 MB per thread), VmData was dominated by thread stack reservations:

| Process | Extra Threads | Stack Reservation (kB) | VmData Delta (kB) |
|---------|--------------|------------------------|--------------------|
| LaunchManager | +4 | 32,768 | ~33,048 |
| StateManager | +3 | 24,576 | ~24,288 |

With `ulimit -s 64`, VmData dropped by ~285 MB (custom IPC) and ~343 MB (mw::com), confirming VmData was almost entirely virtual thread stack space. The remaining VmData values (2.2 MB vs 3.1 MB) represent actual heap usage.

## Summary

| | Custom IPC | mw::com | Delta |
|---|---|---|---|
| **Total RSS (physical)** | 7.5 MB | 13.2 MB | **+5.8 MB** |
| - Private heap (RssAnon) | 0.6 MB | 0.9 MB | +0.2 MB (non-shareable) |
| - Mapped libs (RssFile) | 6.8 MB | 12.4 MB | +5.6 MB (shareable) |
| **Heap reserved (VmData)** | 2.2 MB | 3.0 MB | +0.9 MB |
| **Binary size (VmExe)** | 2.9 MB | 6.7 MB | +3.9 MB |
| **Threads** | 17 | 24 | +7 |

The mw::com overhead at steady state is ~5.9 MB RSS, of which 96% is shareable library mappings (RssFile). The actual non-shareable per-process cost is ~230 kB of private heap. On a system where multiple processes already use mw::com, the incremental cost of adding it to these two processes would be close to just the +230 kB private heap, +3.9 MB binary size, and +7 threads.
