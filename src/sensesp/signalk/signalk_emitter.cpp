#include "signalk_emitter.h"

#include <algorithm>

namespace sensesp {

std::vector<SKEmitter*> SKEmitter::sources_;

SKEmitter::SKEmitter(const String& sk_path) : sk_path_{sk_path} {
  sources_.push_back(this);
}

SKEmitter::~SKEmitter() {
  sources_.erase(std::remove(sources_.begin(), sources_.end(), this),
                 sources_.end());
}

void SKEmitter::clear_registry() { sources_.clear(); }

void SKEmitter::add_metadata(JsonArray& meta) {
  SKMetadata* my_meta = this->get_metadata();
  if (my_meta != NULL) {
    my_meta->add_entry(this->get_sk_path(), meta);
  }
}

}  // namespace sensesp
