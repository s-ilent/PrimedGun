#include "GraphicsHooks.h"
#include "VulkanHooks.h"

#include <windows.h>

#include <array>
#include <string>
#include <string_view>

namespace PrimedGun::Hook {
void Log(std::wstring_view message);
}

namespace PrimedGun::Hook::GraphicsHooks {
namespace {

struct BackendModule {
    const wchar_t* moduleName;
    const wchar_t* label;
    bool seen = false;
};

std::array<BackendModule, 5> g_backends{{
    {L"vulkan-1.dll", L"Vulkan loader"},
    {L"d3d11.dll", L"D3D11"},
    {L"d3d12.dll", L"D3D12"},
    {L"dxgi.dll", L"DXGI"},
    {L"openxr_loader.dll", L"OpenXR loader"},
}};

} // namespace

bool Install() {
    Log(L"GraphicsHooks::Install: runtime initialized. Backend-specific detours are pending.");
    PollBackendModules();
    return true;
}

void PollBackendModules(const SharedState* shared) {
    for (BackendModule& backend : g_backends) {
        if (!backend.seen && GetModuleHandleW(backend.moduleName)) {
            backend.seen = true;
            Log(std::wstring(L"Detected ") + backend.label + L" module: " + backend.moduleName);
        }
    }

    VulkanHooks::InstallIfAvailable();
    VulkanHooks::PollRuntimeControls(shared);
}

void Shutdown() {
    VulkanHooks::Shutdown();
    Log(L"GraphicsHooks::Shutdown");
}

} // namespace PrimedGun::Hook::GraphicsHooks
