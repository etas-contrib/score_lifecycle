# User Guide

## Introduction

The Lifecycle Manager (LCM) is responsible for system execution on an embedded
target.
This includes platform initialization, start-up and shutdown of applications,
and setting the correct resource policies for any given process.

Applications interact with LCM by linking against the `lifecycle_client` library
and using the `LifecycleClient`.
This opens a communication channel to report execution state back to LCM.
Each process must create an instance of `LifecycleClient` and call
`ReportExecutionState(ExecutionState::kRunning)` once the application has
initialized.


### Handling Termiantions

Ending the application can be done in 2 ways:
- Self-terminating -- The application will terminate whenever it wants.
- Non-self-termianting -- The application will be alive until LCM requests
    the application to end using a `SIGTERM` signal.


#### Startup state
`Startup`
