#include "signalk_put_request_listener.h"

#include <algorithm>

#include "signalk_listener.h"

namespace sensesp {

std::vector<SKPutListener*> SKPutListener::listeners_;

SKPutListener::SKPutListener(const String& sk_path) : sk_path{sk_path} {
  listeners_.push_back(this);
}

SKPutListener::~SKPutListener() {
  // The WS client iterates this registry while holding the SKListener
  // semaphore, so mutate it under the same lock.
  SKListener::take_semaphore();
  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), this),
                   listeners_.end());
  SKListener::release_semaphore();
}

void SKPutListener::clear_registry() {
  SKListener::take_semaphore();
  listeners_.clear();
  SKListener::release_semaphore();
}

}  // namespace sensesp
