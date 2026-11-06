// M10-007 census: Windows D3DKMT perf data for the MCLA_VRAM line, and the
// M11-002 / KI-067 CPU sources of the MCLA_CPU line (see
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
#include <pdh.h>
#include <wbemidl.h>

#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wbemuuid.lib")

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

// --- M11-002 / KI-067 CPU census ------------------------------------------------
namespace {

using PdhOpenQueryWFn = LONG(WINAPI *)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
using PdhAddEnglishCounterWFn = LONG(WINAPI *)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
using PdhCollectQueryDataFn = LONG(WINAPI *)(PDH_HQUERY);
using PdhGetFormattedCounterValueFn = LONG(WINAPI *)(PDH_HCOUNTER, DWORD, LPDWORD,
                                                      PPDH_FMT_COUNTERVALUE);
using PdhCloseQueryFn = LONG(WINAPI *)(PDH_HQUERY);
using CallNtPowerInformationFn = LONG(WINAPI *)(POWER_INFORMATION_LEVEL, PVOID, ULONG, PVOID,
                                                ULONG);

// PROCESSOR_POWER_INFORMATION (ntpoapi.h; not in the SDK user-mode headers).
struct ProcessorPowerInformation {
  ULONG Number;
  ULONG MaxMhz;
  ULONG CurrentMhz;
  ULONG MhzLimit;
  ULONG MaxIdleState;
  ULONG CurrentIdleState;
};

uint64_t FileTime100ns(const FILETIME &ft) {
  return (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

HMODULE LoadSystemLibrary(const wchar_t *name) {
  HMODULE module = GetModuleHandleW(name);
  return module ? module : LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

class SystemCpuApi final : public CpuApi {
 public:
  SystemCpuApi() {
    QueryPerformanceFrequency(&qpc_frequency_);
    if (HMODULE pdh = LoadSystemLibrary(L"pdh.dll")) {
      open_query_ = Resolve<PdhOpenQueryWFn>(pdh, "PdhOpenQueryW");
      add_counter_ = Resolve<PdhAddEnglishCounterWFn>(pdh, "PdhAddEnglishCounterW");
      collect_ = Resolve<PdhCollectQueryDataFn>(pdh, "PdhCollectQueryData");
      format_ = Resolve<PdhGetFormattedCounterValueFn>(pdh, "PdhGetFormattedCounterValue");
      close_query_ = Resolve<PdhCloseQueryFn>(pdh, "PdhCloseQuery");
    }
    if (HMODULE powrprof = LoadSystemLibrary(L"powrprof.dll")) {
      power_ = Resolve<CallNtPowerInformationFn>(powrprof, "CallNtPowerInformation");
    }
  }
  ~SystemCpuApi() override {
    ClosePdh();
    CloseTemperature();
  }

  uint32_t LogicalProcessors() override {
    return static_cast<uint32_t>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
  }

  bool Times(CpuTimes &out) override {
    FILETIME idle{}, kernel{}, user{}, created{}, exited{}, pkernel{}, puser{};
    LARGE_INTEGER now{};
    if (!GetSystemTimes(&idle, &kernel, &user) ||
        !GetProcessTimes(GetCurrentProcess(), &created, &exited, &pkernel, &puser) ||
        !QueryPerformanceCounter(&now) || qpc_frequency_.QuadPart <= 0) {
      return false;
    }
    out.idle = FileTime100ns(idle);
    out.kernel = FileTime100ns(kernel);
    out.user = FileTime100ns(user);
    out.process_kernel = FileTime100ns(pkernel);
    out.process_user = FileTime100ns(puser);
    const uint64_t ticks = uint64_t(now.QuadPart);
    const uint64_t freq = uint64_t(qpc_frequency_.QuadPart);
    out.wall = (ticks / freq) * 10000000ull + (ticks % freq) * 10000000ull / freq;
    return true;
  }

  bool OpenPdh() override {
    ClosePdh();
    if (!open_query_ || !add_counter_ || !collect_ || !format_ || !close_query_) {
      return false;
    }
    if (open_query_(nullptr, 0, &query_) != ERROR_SUCCESS) {
      query_ = nullptr;
      return false;
    }
    if (add_counter_(query_, L"\\Processor Information(_Total)\\% Processor Performance", 0,
                     &performance_) != ERROR_SUCCESS ||
        add_counter_(query_, L"\\Processor Information(_Total)\\Processor Frequency", 0,
                     &frequency_) != ERROR_SUCCESS ||
        collect_(query_) != ERROR_SUCCESS) {
      ClosePdh();
      return false;
    }
    return true;
  }

  bool QueryPdh(double &performance_pct, double &nominal_mhz) override {
    if (!query_ || collect_(query_) != ERROR_SUCCESS) {
      return false;
    }
    PDH_FMT_COUNTERVALUE perf{}, freq{};
    if (format_(performance_, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, nullptr, &perf) !=
            ERROR_SUCCESS ||
        format_(frequency_, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, nullptr, &freq) !=
            ERROR_SUCCESS) {
      return false;
    }
    performance_pct = perf.doubleValue;
    nominal_mhz = freq.doubleValue;
    return true;
  }

  void ClosePdh() override {
    if (query_) {
      close_query_(query_);
      query_ = nullptr;
    }
  }

  bool QueryPower(PowerInfo &out) override {
    const uint32_t count = LogicalProcessors();
    if (!power_ || count == 0 || count > 1024) {
      return false;
    }
    std::vector<ProcessorPowerInformation> info(count);
    if (power_(ProcessorInformation, nullptr, 0, info.data(),
               static_cast<ULONG>(info.size() * sizeof(info[0]))) != 0) {
      return false;
    }
    double current = 0, max = 0, limit = 0;
    for (const auto &p : info) {
      current += p.CurrentMhz;
      max += p.MaxMhz;
      limit += p.MhzLimit;
    }
    out.current_mhz = current / count;
    out.max_mhz = max / count;
    out.limit_mhz = limit / count;
    return true;
  }

  // WMI, tried once at start on the monitor thread (COM is initialised for
  // that thread only and released in CloseTemperature on the same thread).
  bool OpenTemperature(uint32_t &status) override {
    CloseTemperature();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
      status = static_cast<uint32_t>(hr);
      return false;
    }
    com_initialized_ = true;
    IWbemLocator *locator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                          reinterpret_cast<void **>(&locator));
    if (SUCCEEDED(hr)) {
      BSTR ns = SysAllocString(L"ROOT\\WMI");
      hr = ns ? locator->ConnectServer(ns, nullptr, nullptr, nullptr,
                                       WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, nullptr,
                                       &services_)
              : E_OUTOFMEMORY;
      SysFreeString(ns);
      locator->Release();
    }
    if (SUCCEEDED(hr)) {
      hr = CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                             RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                             EOAC_NONE);
    }
    if (FAILED(hr)) {
      status = static_cast<uint32_t>(hr);
      CloseTemperature();
      return false;
    }
    status = 0;
    return true;
  }

  bool QueryTemperature(double &celsius) override {
    if (!services_) {
      return false;
    }
    BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature");
    IEnumWbemClassObject *rows = nullptr;
    HRESULT hr = language && query
                     ? services_->ExecQuery(language, query,
                                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                            nullptr, &rows)
                     : E_OUTOFMEMORY;
    SysFreeString(language);
    SysFreeString(query);
    if (FAILED(hr) || !rows) {
      return false;
    }
    double best = kNoTemperature;
    for (int i = 0; i < 16; ++i) {
      IWbemClassObject *row = nullptr;
      ULONG returned = 0;
      if (rows->Next(2000, 1, &row, &returned) != WBEM_S_NO_ERROR || returned == 0 || !row) {
        break;
      }
      VARIANT value;
      VariantInit(&value);
      if (SUCCEEDED(row->Get(L"CurrentTemperature", 0, &value, nullptr, nullptr)) &&
          (value.vt == VT_I4 || value.vt == VT_UI4)) {
        const double c = ThermalZoneCelsius(static_cast<uint32_t>(value.ulVal));
        best = c > best ? c : best;
      }
      VariantClear(&value);
      row->Release();
    }
    rows->Release();
    if (!(best > kNoTemperature)) {
      return false;
    }
    celsius = best;
    return true;
  }

  void CloseTemperature() override {
    if (services_) {
      services_->Release();
      services_ = nullptr;
    }
    if (com_initialized_) {
      CoUninitialize();
      com_initialized_ = false;
    }
  }

 private:
  LARGE_INTEGER qpc_frequency_{};
  PdhOpenQueryWFn open_query_ = nullptr;
  PdhAddEnglishCounterWFn add_counter_ = nullptr;
  PdhCollectQueryDataFn collect_ = nullptr;
  PdhGetFormattedCounterValueFn format_ = nullptr;
  PdhCloseQueryFn close_query_ = nullptr;
  CallNtPowerInformationFn power_ = nullptr;
  PDH_HQUERY query_ = nullptr;
  PDH_HCOUNTER performance_ = nullptr, frequency_ = nullptr;
  IWbemServices *services_ = nullptr;
  bool com_initialized_ = false;
};

}  // namespace

std::unique_ptr<CpuApi> CreateSystemCpuApi() {
  return std::make_unique<SystemCpuApi>();
}

}  // namespace mcla::vram
