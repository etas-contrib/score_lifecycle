#ifndef GRAPH_CONTROL_HPP
#define GRAPH_CONTROL_HPP

#include <score/lcm/identifier_hash.hpp>
#include "score/concurrency/future/interruptible_future.h"

namespace score {

namespace lcm {

namespace internal {

class IGraphControl {
  public:
    virtual ~IGraphControl() = default;

    /// @brief Trigger activation of the given RunTarget
    /// @param target_id The RunTarget to activate
    /// @return A Future that will be set once the activation process completes
    virtual score::concurrency::InterruptibleFuture<void> ActivateRunTarget(IdentifierHash target_id) = 0;
};

}  // namespace internal

}  // namespace lcm

}  // namespace score

#endif  // GRAPH_CONTROL_HPP
