// KI-061 (M10-004): Vulkan device for the launcher-selected GPU.
//
// The launcher passes its GPU choice only as --d3d12_adapter=<raw
// IDXGIFactory1::EnumAdapters1 ordinal> (the SDK's D3D12 provider uses the same
// ordinal). The SDK's Vulkan provider ignores it and, with the default
// vulkan_device=-1, takes physical device 0: on hybrid laptops that is the
// integrated GPU. Here the ordinal is resolved to the adapter's LUID and PCI
// ids through DXGI, the Vulkan physical devices are listed through a private
// instance of the system Vulkan loader (vulkan-1.dll loaded at run time; no
// link dependency) and the pure rule in mcla_vulkan_device_select.h picks the
// index written to vulkan_device. The loader module stays loaded: the SDK
// loads the same module moments later, and some implicit layers do not
// tolerate a loader unload and reload in one process.

#include "mcla_vulkan_device_win.h"

#include "mcla_logging.h"
#include "mcla_vulkan_device_select.h"

#include <rex/cvar.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <charconv>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<vulkan/vulkan_core.h>)
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#define MCLA_VULKAN_DEVICE_PROBE 1
#else
#define MCLA_VULKAN_DEVICE_PROBE 0
#endif

#pragma comment(lib, "dxgi.lib")

namespace mcla {
namespace {

namespace vd = vulkan_device;

// Cross-DLL cvar read; nullopt when the cvar is not registered or unparsable
// (rex::cvar::Query returns 0 for a missing cvar, which is a valid index).
std::optional<int32_t> QueryInt32(std::string_view name) {
  const auto *info = rex::cvar::GetFlagInfo(name);
  if (!info || !info->getter) return std::nullopt;
  const std::string text = info->getter();
  int32_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
  return value;
}

std::optional<vd::Adapter> LauncherAdapter(int32_t ordinal) {
  if (ordinal < 0) return std::nullopt;  // -1 automatic, -2 WARP
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return std::nullopt;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  if (factory->EnumAdapters1(static_cast<UINT>(ordinal), &adapter) != S_OK) return std::nullopt;
  DXGI_ADAPTER_DESC1 desc{};
  if (FAILED(adapter->GetDesc1(&desc))) return std::nullopt;
  if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) return std::nullopt;
  vd::Adapter result;
  static_assert(sizeof(desc.AdapterLuid) == sizeof(result.luid));
  std::memcpy(result.luid.data(), &desc.AdapterLuid, result.luid.size());
  result.vendor_id = desc.VendorId;
  result.device_id = desc.DeviceId;
  return result;
}

#if MCLA_VULKAN_DEVICE_PROBE
template <typename T>
T Proc(PFN_vkVoidFunction function) {
  return reinterpret_cast<T>(function);
}

// Lists the physical devices in the loader's enumeration order. Returns an
// empty vector and a reason when the loader or an instance is unavailable.
std::vector<vd::Device> ListVulkanDevices(std::string &failure) {
  std::vector<vd::Device> devices;
  HMODULE loader = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!loader) {
    failure = "vulkan-1.dll not loadable";
    return devices;
  }
  const auto get_instance_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      reinterpret_cast<void *>(GetProcAddress(loader, "vkGetInstanceProcAddr")));
  if (!get_instance_proc) {
    failure = "no vkGetInstanceProcAddr";
    return devices;
  }
  const auto create_instance =
      Proc<PFN_vkCreateInstance>(get_instance_proc(nullptr, "vkCreateInstance"));
  const auto enumerate_version =
      Proc<PFN_vkEnumerateInstanceVersion>(get_instance_proc(nullptr, "vkEnumerateInstanceVersion"));
  if (!create_instance) {
    failure = "no vkCreateInstance";
    return devices;
  }
  uint32_t loader_version = VK_API_VERSION_1_0;
  if (enumerate_version && enumerate_version(&loader_version) != VK_SUCCESS)
    loader_version = VK_API_VERSION_1_0;
  const bool have_1_1 = loader_version >= VK_API_VERSION_1_1;

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "mcla vulkan device probe";
  app.apiVersion = have_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
  VkInstanceCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create.pApplicationInfo = &app;
  VkInstance instance = VK_NULL_HANDLE;
  if (create_instance(&create, nullptr, &instance) != VK_SUCCESS || !instance) {
    failure = "vkCreateInstance failed";
    return devices;
  }
  const auto load = [&](const char *name) { return get_instance_proc(instance, name); };
  const auto destroy_instance = Proc<PFN_vkDestroyInstance>(load("vkDestroyInstance"));
  const auto enumerate = Proc<PFN_vkEnumeratePhysicalDevices>(load("vkEnumeratePhysicalDevices"));
  const auto properties =
      Proc<PFN_vkGetPhysicalDeviceProperties>(load("vkGetPhysicalDeviceProperties"));
  const auto properties2 =
      have_1_1 ? Proc<PFN_vkGetPhysicalDeviceProperties2>(load("vkGetPhysicalDeviceProperties2"))
               : nullptr;
  const auto memory =
      Proc<PFN_vkGetPhysicalDeviceMemoryProperties>(load("vkGetPhysicalDeviceMemoryProperties"));
  if (enumerate && properties && memory) {
    uint32_t count = 0;
    std::vector<VkPhysicalDevice> handles;
    VkResult result = VK_INCOMPLETE;
    for (int attempt = 0; attempt < 4 && result == VK_INCOMPLETE; ++attempt) {
      count = 0;
      if (enumerate(instance, &count, nullptr) != VK_SUCCESS) {
        result = VK_ERROR_INITIALIZATION_FAILED;
        break;
      }
      handles.resize(count);
      result = enumerate(instance, &count, handles.data());
    }
    if (result == VK_SUCCESS) {
      handles.resize(count);
      for (VkPhysicalDevice handle : handles) {
        vd::Device device;
        VkPhysicalDeviceProperties props{};
        if (properties2) {
          VkPhysicalDeviceIDProperties ids{};
          ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
          VkPhysicalDeviceProperties2 props2{};
          props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
          props2.pNext = &ids;
          properties2(handle, &props2);
          props = props2.properties;
          device.luid_valid = ids.deviceLUIDValid == VK_TRUE;
          static_assert(sizeof(ids.deviceLUID) == sizeof(device.luid));
          std::memcpy(device.luid.data(), ids.deviceLUID, device.luid.size());
        } else {
          properties(handle, &props);
        }
        device.name.assign(props.deviceName, strnlen(props.deviceName, sizeof(props.deviceName)));
        device.vendor_id = props.vendorID;
        device.device_id = props.deviceID;
        device.discrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        VkPhysicalDeviceMemoryProperties heaps{};
        memory(handle, &heaps);
        for (uint32_t h = 0; h < heaps.memoryHeapCount && h < VK_MAX_MEMORY_HEAPS; ++h)
          if (heaps.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            device.device_local_bytes += heaps.memoryHeaps[h].size;
        devices.push_back(std::move(device));
      }
    } else {
      failure = "vkEnumeratePhysicalDevices failed";
    }
  } else {
    failure = "instance functions missing";
  }
  if (destroy_instance) destroy_instance(instance, nullptr);
  return devices;
}
#endif

}  // namespace

void SelectVulkanDeviceForLauncherAdapter() {
  const std::optional<int32_t> configured = QueryInt32("vulkan_device");
  if (!configured) {
    MCLA_GPU_INFO("MCLA gpu backend: vulkan device not mapped: vulkan_device cvar unavailable");
    return;
  }
  const int32_t launcher_ordinal = QueryInt32("d3d12_adapter").value_or(-1);
  if (*configured >= 0) {
    MCLA_GPU_INFO(
        "MCLA gpu backend: vulkan device {} (explicit vulkan_device kept); "
        "launcher d3d12_adapter={}",
        *configured, launcher_ordinal);
    return;
  }
  std::string failure;
#if MCLA_VULKAN_DEVICE_PROBE
  const std::vector<vd::Device> devices = ListVulkanDevices(failure);
#else
  const std::vector<vd::Device> devices;
  failure = "built without Vulkan headers";
#endif
  const std::optional<vd::Adapter> adapter = LauncherAdapter(launcher_ordinal);
  const vd::Choice choice = vd::Select(*configured, adapter, devices);
  std::string name = "SDK automatic";
  if (choice.index >= 0 && static_cast<size_t>(choice.index) < devices.size())
    name = devices[static_cast<size_t>(choice.index)].name;
  bool applied = true;
  if (choice.set_cvar)
    applied = rex::cvar::SetFlagByName("vulkan_device", std::to_string(choice.index));
  MCLA_GPU_INFO(
      "MCLA gpu backend: vulkan device {} '{}' (matched by {}){}; launcher "
      "d3d12_adapter={}{}; {} Vulkan device(s){}{}",
      choice.index, name, vd::ReasonName(choice.reason),
      choice.set_cvar ? (applied ? "; vulkan_device set" : "; vulkan_device set FAILED") : "",
      launcher_ordinal, adapter ? "" : " (unresolved)", devices.size(),
      failure.empty() ? "" : "; probe: ", failure);
}

}  // namespace mcla
