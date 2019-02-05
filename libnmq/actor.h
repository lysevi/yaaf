#pragma once

#include <memory>

namespace nmq {

struct actor {};

using actor_ptr = std::shared_ptr<actor>;
using actor_weak = std::weak_ptr<actor>;
} // namespace nmq