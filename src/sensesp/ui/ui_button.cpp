#include "ui_button.h"

namespace sensesp {

std::map<String, std::shared_ptr<UIButton>> UIButton::ui_buttons_;

void UIButton::clear_registry() { ui_buttons_.clear(); }

}  // namespace sensesp
