#include "sensesp_base_app.h"

#include "sensesp/signalk/signalk_emitter.h"
#include "sensesp/transforms/transform.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/ui/status_page_item.h"
#include "sensesp/ui/ui_button.h"

namespace sensesp {

std::shared_ptr<SensESPBaseApp> SensESPBaseApp::instance_ = nullptr;

void SensESPBaseApp::clear_registries() {
  // Release shared_ptr-owning registries first so the objects they own are
  // destroyed; each object's destructor then unregisters itself from the
  // raw-pointer registries, which the clears below tidy up.
  ConfigItemBase::clear_registry();
  UIButton::clear_registry();
  SKEmitter::clear_registry();
  TransformBase::clear_registry();
  StatusPageItemBase::clear_registry();
}

}  // namespace sensesp
