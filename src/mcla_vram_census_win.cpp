// M10-007 census: Windows D3DKMT perf data for the MCLA_VRAM line (see
// mcla_vram_census_win.h). The gdi32 entry points are resolved at run time, so
// a system or Wine prefix without them only reports kmt=off; every failure is
// a false return, never an exception or a failed start.

#include "mcla_vram_census_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winternl.h>
#include <d3dkmthk.h>

namespace mcla::vram {
namespace {

using OpenAdapterFromLuidFn = NTSTATUS(APIENTRY *)(const D3DKMT_OPENADAPTERFROMLUID *);
using QueryAdapterInfoFn = NTSTATUS(APIENTRY *)(const D3DKMT_QUERYADAPTERINFO *);
using CloseAdapterFn = NTSTATUS(APIENTRY *)(const D3DKMT_CLOSEADAPTER *);

template <typename Fn>
Fn Resolve(HMODULE module, const char *name) {
  return reinterpret_cast<Fn>(reinterpret_cast<void *>(GetProcAddress(module, name)));
}

class SystemKmtApi final : public KmtApi {
 public:
  SystemKmtApi(OpenAdapterFromLuidFn open, QueryAdapterInfoFn query, CloseAdapterFn close)
      : open_(open), query_(query), close_(close) {}

  bool Open(uint32_t luid_low, int32_t luid_high) override {
    D3DKMT_OPENADAPTERFROMLUID args{};
    args.AdapterLuid.LowPart = luid_low;
    args.AdapterLuid.HighPart = luid_high;
    if (open_(&args) < 0 || args.hAdapter == 0) {
      return false;
    }
    adapter_ = args.hAdapter;
    return true;
  }

  bool NodeIs3D(uint32_t node, bool &is_3d) override {
    D3DKMT_NODEMETADATA data{};
    data.NodeOrdinalAndAdapterIndex = node & 0xFFFF;
    if (!Query(KMTQAITYPE_NODEMETADATA, &data, sizeof(data))) {
      return false;
    }
    is_3d = data.NodeData.EngineType == DXGK_ENGINE_TYPE_3D;
    return true;
  }

  bool QueryNode(uint32_t node, NodePerf &out) override {
    D3DKMT_NODE_PERFDATA data{};
    data.NodeOrdinal = node;
    data.PhysicalAdapterIndex = 0;
    if (!Query(KMTQAITYPE_NODEPERFDATA, &data, sizeof(data))) {
      return false;
    }
    out.frequency_hz = data.Frequency;
    out.max_frequency_hz = data.MaxFrequency;
    return true;
  }

  bool QueryAdapter(AdapterPerf &out) override {
    D3DKMT_ADAPTER_PERFDATA data{};
    data.PhysicalAdapterIndex = 0;
    if (!Query(KMTQAITYPE_ADAPTERPERFDATA, &data, sizeof(data))) {
      return false;
    }
    out.memory_frequency_hz = data.MemoryFrequency;
    out.power_tenth_pct = data.Power;
    out.temperature_deci_c = data.Temperature;
    out.fan_rpm = data.FanRPM;
    return true;
  }

  void Close() override {
    if (adapter_) {
      D3DKMT_CLOSEADAPTER args{};
      args.hAdapter = adapter_;
      close_(&args);
      adapter_ = 0;
    }
  }

 private:
  bool Query(KMTQUERYADAPTERINFOTYPE type, void *data, size_t size) {
    if (!adapter_) {
      return false;
    }
    D3DKMT_QUERYADAPTERINFO args{};
    args.hAdapter = adapter_;
    args.Type = type;
    args.pPrivateDriverData = data;
    args.PrivateDriverDataSize = static_cast<UINT>(size);
    return query_(&args) >= 0;
  }

  OpenAdapterFromLuidFn open_;
  QueryAdapterInfoFn query_;
  CloseAdapterFn close_;
  D3DKMT_HANDLE adapter_ = 0;
};

}  // namespace

std::unique_ptr<KmtApi> CreateSystemKmtApi() {
  HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
  if (!gdi) {
    gdi = LoadLibraryExW(L"gdi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  }
  if (!gdi) {
    return nullptr;
  }
  const auto open = Resolve<OpenAdapterFromLuidFn>(gdi, "D3DKMTOpenAdapterFromLuid");
  const auto query = Resolve<QueryAdapterInfoFn>(gdi, "D3DKMTQueryAdapterInfo");
  const auto close = Resolve<CloseAdapterFn>(gdi, "D3DKMTCloseAdapter");
  if (!open || !query || !close) {
    return nullptr;
  }
  return std::make_unique<SystemKmtApi>(open, query, close);
}

}  // namespace mcla::vram
