#pragma once

namespace Autostart {
bool isEnabled();
void setEnabled(bool enabled);
// Rewrite the Run-key command if autostart is already on (path + --autostart).
void sync();
}
