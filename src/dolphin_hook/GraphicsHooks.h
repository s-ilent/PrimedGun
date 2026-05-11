#pragma once

namespace PrimedGun {
struct SharedState;
}

namespace PrimedGun::Hook::GraphicsHooks {

bool Install();
void PollBackendModules(const SharedState* shared = nullptr);
void Shutdown();

} // namespace PrimedGun::Hook::GraphicsHooks
