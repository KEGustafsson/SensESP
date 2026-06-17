#include "status_page_item.h"

namespace sensesp {

std::map<String, StatusPageItemBase*> StatusPageItemBase::status_page_items_;

StatusPageItemBase::~StatusPageItemBase() {
  // Only erase if this item is the one registered under its name. Duplicate
  // names keep the first registration, so a later duplicate must not remove
  // the original's entry.
  auto it = status_page_items_.find(name_);
  if (it != status_page_items_.end() && it->second == this) {
    status_page_items_.erase(it);
  }
}

void StatusPageItemBase::clear_registry() { status_page_items_.clear(); }

}  // namespace sensesp
