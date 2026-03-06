## Control Client

The Control Client is the application that requests whichever run target 
shall be active and so which applications shall be started or stopped.

```cpp
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <iostream>

#include <score/lcm/lifecycle_client.h>
#include <score/lcm/control_client.h>
#include "ipc_dropin/socket.hpp"
#include "control.hpp"

std::atomic<bool> exitRequested{false};
void signalHandler(int) {
    exitRequested = true;
}

int main(int argc, char** argv) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    score::lcm::LifecycleClient{}.ReportExecutionState(score::lcm::ExecutionState::kRunning);
    score::lcm::ControlClient client;

    std::string runTargetName{info.runTargetName};
    client.ActivateRunTarget(runTargetName);

    return EXIT_SUCCESS;
}

```
