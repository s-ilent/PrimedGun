#pragma once

namespace PrimedGun {
struct SharedState;
}

namespace PrimedGun::Hook::VulkanHooks {

bool InstallIfAvailable();
void PollRuntimeControls(const SharedState* shared = nullptr);
void Shutdown();

} // namespace PrimedGun::Hook::VulkanHooks
