### Example Application Code

#### CPP

Example code for a self-terminating application.

```cpp
#include <iostream>
#include <score/lcm/lifecycle_client.h>

int main(int argc, char* argv[]){

    score::Result<std::monostate> result = score::lcm::LifecycleClient{}.ReportExecutionState(score::lcm::ExecutionState::kRunning);

    if(!result.has_value()){
        std::cerr << "Failed to report kRunning to LCM! Terminating" << std::endl;
        return 1; 
    }

    // do some work

    return 0;
}
```


Example of a non-self-terminating application.

```cpp
#include <iostream>
#include <unistd.h>
#include <score/lcm/lifecycle_client.h>

volatile bool exitRequested = false;

void sigTermHandler(int)
{
    exitRequested = true;
}

int main(int argc, char* argv[]){

    score::Result<std::monostate> result = score::lcm::LifecycleClient{}.ReportExecutionState(score::lcm::ExecutionState::kRunning);

    if(!result.has_value()){
        std::cerr << "Failed to report kRunning to LCM! Terminating" << std::endl;
        return 1; 
    }

    // register signal handler so that lcm can request a termination
    signal(SIGTERM, sigTermHandler);

    // do some work

    while (!exitRequested) {
        pause();
    }

    return 0;
}
```


#### Rust

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {

    let stop = Arc::new(AtomicBool::new(false));
    flag::register(signal_hook::consts::SIGTERM, Arc::clone(&stop))?;

    lifecycle_client_rs::report_execution_state_running() :

    while !stop.load(Ordering::Relaxed) {
        // do work
    }

    Ok(());
}
```

#### C

```c
#include <score/lcm/lifecycle_client.h>
#include <unistd.h>
#include <stdio.h>

volatile bool exitRequested = false;

void sigTermHandler(int)
{
    exitRequested = true;
}

int main(int argc, char* argv[]){

    int result = score_lcm_ReportExecutionStateRunning();

    if(result != 0){
        printf("Failed to report kRunning to LCM! Terminating");
        return 1; 
    }

    // register signal handler so that lcm can request a termination
    signal(SIGTERM, sigTermHandler);

    // do some work

    while (!exitRequested) {
        pause();
    }

    return 0;
}
```
