// Headless synthetic regression: execute the SDK's actual compiled downscale
// shader on WARP. No title data, windows, profiles, shader compilation or files.
// Exit 0 = all checks pass, 1 = shader/oracle mismatch, 2 = setup/runtime error.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace sdk_bytecode {
#include <resolve_downscale_cs.h>
}
#if defined(MCLA_DOWNSCALE_BASELINE_HEADER)
// Primary supplies the separately sealed baseline header at compile time.
// Never load shader bytes from a runtime path or substitute a CPU model.
namespace baseline_bytecode {
#include MCLA_DOWNSCALE_BASELINE_HEADER
}
#endif

namespace {
using Microsoft::WRL::ComPtr;
constexpr uint32_t kTiles = 2, kPixels = kTiles * 1024, kGuardWords = 1024;
constexpr uint64_t kResourceBudget = 16 * 1024 * 1024;
constexpr uint32_t kSentinel = 0xDEADBEEF;
constexpr uint32_t kMaxAxis = 7;
constexpr uint64_t kRunMilliseconds = 55000;
constexpr std::array<uint32_t, 3> kSourceOffsets{0, 16, 4096};
constexpr std::array<std::array<uint32_t, 2>, 6> kDifferentialScales{{
    {{1, 1}}, {{2, 2}}, {{3, 3}}, {{2, 3}}, {{3, 2}}, {{7, 7}}}};
struct Counts {
  uint64_t checks = 0, failures = 0;
  void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; if (failures <= 16) std::cerr << "FAIL " << label << '\n'; }
  }
};
void Require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
void Hr(HRESULT hr, const char* message) {
  if (FAILED(hr)) {
    std::cerr << "HRESULT 0x" << std::hex << uint32_t(hr) << std::dec << '\n';
    throw std::runtime_error(message);
  }
}

// Physical 32-bit pixel ordinal, NOT a linear guest x/y coordinate. The real
// scaled writer preserves 16-byte (uint4) groups: host X is split into vector
// number and lane BEFORE sample Y. This intentionally does not repeat the
// downscale shader's pixel*scale_xy source-address formula.
uint32_t SourceWord(uint32_t pixel, uint32_t scale_x, uint32_t scale_y, uint32_t sample_x,
                    uint32_t sample_y) {
  Require(pixel < kPixels && scale_x >= 1 && scale_x <= kMaxAxis &&
              scale_y >= 1 && scale_y <= kMaxAxis && sample_x < scale_x &&
              sample_y < scale_y, "invalid oracle arguments");
  const uint32_t group = pixel / 4, lane = pixel % 4;
  const uint32_t host_x = lane * scale_x + sample_x;
  return group * 4 * scale_x * scale_y + (host_x / 4 * scale_y + sample_y) * 4 + host_x % 4;
}
uint32_t Label(uint32_t source_word) {
  // Odd multiplication is bijective modulo 2^32: every source word is distinct.
  return 0xA583C719u ^ (source_word * 2654435761u);
}
void PureTests(Counts& c) {
  c.Check(SourceWord(1, 2, 2, 0, 0) * 4 == 8, "scale2 pixel1 corner byte8");
  c.Check(SourceWord(1, 2, 2, 1, 1) * 4 == 28, "scale2 pixel1 center byte28");
  c.Check(SourceWord(0, 2, 2, 1, 1) * 4 == 20, "scale2 pixel0 center byte20");
  c.Check(SourceWord(2, 2, 2, 1, 1) * 4 == 52, "scale2 pixel2 center byte52");
  c.Check(SourceWord(2, 2, 3, 1, 1) * 4 == 68, "2x3 pixel2 center byte68");
  c.Check(SourceWord(1, 3, 2, 1, 1) * 4 == 48, "3x2 pixel1 center byte48");
  for (uint32_t sx = 1; sx <= kMaxAxis; ++sx) for (uint32_t sy = 1; sy <= kMaxAxis; ++sy) {
    std::vector<bool> seen(kPixels * sx * sy);
    for (uint32_t p = 0; p < kPixels; ++p)
      for (uint32_t y = 0; y < sy; ++y)
        for (uint32_t x = 0; x < sx; ++x) {
          const auto word = SourceWord(p, sx, sy, x, y);
          c.Check(word < seen.size(), "source range");
          Require(word < seen.size(), "oracle range violation");
          c.Check(!seen[word], "source mapping bijective");
          seen[word] = true;
          if (sx == 1 && sy == 1) c.Check(word == p, "scale1 identity");
        }
    c.Check(SourceWord(1024, sx, sy, 0, 0) == 1024 * sx * sy, "second tile offset");
  }
  for (const auto v : std::array<std::array<uint32_t, 5>, 7>{{
           {{kPixels, 1, 1, 0, 0}}, {{0, 0, 1, 0, 0}}, {{0, 8, 1, 0, 0}},
           {{0, 1, 0, 0, 0}}, {{0, 1, 8, 0, 0}},
           {{0, 2, 3, 2, 0}}, {{0, 2, 3, 0, 3}}}}) {
    bool rejected = false;
    try { (void)SourceWord(v[0], v[1], v[2], v[3], v[4]); }
    catch (const std::runtime_error&) { rejected = true; }
    c.Check(rejected, "invalid oracle inputs rejected");
  }
}

struct Event {
  HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  Event() { Require(handle != nullptr, "fence event creation failed"); }
  ~Event() { CloseHandle(handle); }
  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
};
struct Fixture {
  ComPtr<IDXGIFactory4> factory;
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12RootSignature> root;
  ComPtr<ID3D12PipelineState> pipeline;
  ComPtr<ID3D12PipelineState> baseline_pipeline;
  ComPtr<ID3D12Fence> fence;
  Event event;
  uint64_t next_fence = 0;
  const uint64_t started = GetTickCount64();
  explicit Fixture(bool differential = false) {
    Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "DXGI factory failed");
    Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "WARP unavailable; no hardware fallback");
    DXGI_ADAPTER_DESC1 desc{};
    Hr(adapter->GetDesc1(&desc), "WARP identity unavailable");
    Require((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0, "adapter is not software");
    Hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                        IID_PPV_ARGS(&device)), "D3D12 WARP unavailable");
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Hr(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)), "queue creation failed");
    Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence creation failed");
    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    for (auto& range : ranges) range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER params[3]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants.Num32BitValues = 5;
    for (unsigned i = 1; i < 3; ++i) {
      params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      params[i].DescriptorTable.NumDescriptorRanges = 1;
      params[i].DescriptorTable.pDescriptorRanges = &ranges[i - 1];
    }
    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.NumParameters = 3; root_desc.pParameters = params;
    ComPtr<ID3DBlob> blob, error;
    Hr(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                  &blob, &error), "root signature serialization failed");
    Hr(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                  IID_PPV_ARGS(&root)), "root signature creation failed");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};
    pipeline_desc.pRootSignature = root.Get();
    pipeline_desc.CS = {sdk_bytecode::resolve_downscale_cs,
                        sizeof(sdk_bytecode::resolve_downscale_cs)};
    Hr(device->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline)),
       "actual SDK bytecode pipeline creation failed");
#if defined(MCLA_DOWNSCALE_BASELINE_HEADER)
    if (differential) {
      Require(sizeof(baseline_bytecode::resolve_downscale_cs) != sizeof(sdk_bytecode::resolve_downscale_cs) ||
                  std::memcmp(baseline_bytecode::resolve_downscale_cs, sdk_bytecode::resolve_downscale_cs,
                              sizeof(sdk_bytecode::resolve_downscale_cs)) != 0,
              "differential requires distinct baseline and candidate bytecode");
      pipeline_desc.CS = {baseline_bytecode::resolve_downscale_cs,
                          sizeof(baseline_bytecode::resolve_downscale_cs)};
      Hr(device->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&baseline_pipeline)),
         "sealed baseline bytecode pipeline creation failed");
      std::cout << "BASELINE_COMPILED_HEADER shader_bytes=" << sizeof(baseline_bytecode::resolve_downscale_cs)
                << " identity_requires_external_build_seal\n";
    }
#else
    Require(!differential, "differential requires compile-time sealed baseline header");
#endif
    std::cout << "WARP_ONLY shader_bytes=" << sizeof(sdk_bytecode::resolve_downscale_cs)
              << " scope=" << (differential ? "other_formats_baseline_equivalence_not_correctness" :
                  "synthetic_32bpp_uint4_layout_not_gameplay") << " deadline_ms=" << kRunMilliseconds << '\n';
  }
  void Deadline() const {
    Require(GetTickCount64() - started < kRunMilliseconds, "overall WARP deadline exceeded");
  }
  ComPtr<ID3D12Resource> Buffer(uint64_t bytes, D3D12_HEAP_TYPE type,
      D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    Require(bytes > 0 && bytes < kResourceBudget && bytes % 4 == 0, "buffer size rejected");
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = type;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes; desc.Height = 1; desc.DepthOrArraySize = 1;
    desc.MipLevels = 1; desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; desc.Flags = flags;
    ComPtr<ID3D12Resource> result;
    Hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state,
        nullptr, IID_PPV_ARGS(&result)), "bounded buffer creation failed");
    return result;
  }
  void Wait() {
    const auto value = ++next_fence;
    Hr(queue->Signal(fence.Get(), value), "fence signal failed");
    Hr(fence->SetEventOnCompletion(value, event.handle), "fence event failed");
    if (WaitForSingleObject(event.handle, 5000) != WAIT_OBJECT_0) {
      // Do not destroy resources while their work is in flight or wait forever.
      // The owning process only terminates; an outer watchdog is still advised.
      std::cerr << "ERROR WARP fence did not complete within 5000ms\n" << std::flush;
      std::_Exit(2);
    }
    Require(fence->GetCompletedValue() != UINT64_MAX, "WARP device removed");
    Hr(device->GetDeviceRemovedReason(), "WARP device failure");
    Deadline();
  }
};
void Upload(ID3D12Resource* resource, const std::vector<uint32_t>& words) {
  void* mapped = nullptr; const D3D12_RANGE no_read{0, 0};
  Hr(resource->Map(0, &no_read, &mapped), "upload map failed");
  std::memcpy(mapped, words.data(), words.size() * sizeof(uint32_t));
  const D3D12_RANGE written{0, words.size() * sizeof(uint32_t)};
  resource->Unmap(0, &written);
}
void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
  list->ResourceBarrier(1, &barrier);
}
std::vector<uint32_t> ExecuteCase(Fixture& f, uint32_t sx, uint32_t sy, bool center,
                                uint32_t pixel_log2, uint32_t source_offset, bool baseline) {
  f.Deadline();
  Require(sx >= 1 && sx <= kMaxAxis && sy >= 1 && sy <= kMaxAxis && pixel_log2 <= 3,
          "invalid dispatch shape");
  Require(source_offset == 0 || source_offset == 16 || source_offset == 4096,
          "invalid raw SRV offset");
  Require(!baseline || f.baseline_pipeline, "baseline pipeline not supplied");
  Require(f.next_fence < 294, "dispatch count budget exceeded");
  const uint32_t pixel_bytes = 1u << pixel_log2;
  const uint32_t payload_words = kPixels * pixel_bytes * sx * sy / 4;
  const uint32_t prefix_words = source_offset / 4;
  const uint32_t source_words = prefix_words + payload_words + kGuardWords;
  const uint32_t output_words = kPixels * pixel_bytes / 4 + kGuardWords;
  const uint64_t source_bytes = uint64_t(source_words) * 4;
  const uint64_t output_bytes = uint64_t(output_words) * 4;
  Require(2 * source_bytes + 3 * output_bytes < kResourceBudget, "aggregate resource budget");
  // Poison lies outside the descriptor's view. It probes erroneous absolute
  // address use; it is not a claim of independently observed input immutability.
  std::vector<uint32_t> source(source_words, 0x5A5AC33C), initial(output_words, kSentinel);
  for (uint32_t i = 0; i < payload_words; ++i) source[prefix_words + i] = Label(i);
  auto upload = f.Buffer(source_bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
  auto initial_upload = f.Buffer(output_bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
  auto input = f.Buffer(source_bytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
  auto output = f.Buffer(output_bytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST,
                         D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  auto readback = f.Buffer(output_bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
  Upload(upload.Get(), source); Upload(initial_upload.Get(), initial);
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
  heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap_desc.NumDescriptors = 2; heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ComPtr<ID3D12DescriptorHeap> heap;
  Hr(f.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)), "descriptor heap failed");
  auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
  const auto stride = f.device->GetDescriptorHandleIncrementSize(heap_desc.Type);
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
  srv.Format = DXGI_FORMAT_R32_TYPELESS; srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv.Buffer.FirstElement = prefix_words; srv.Buffer.NumElements = payload_words;
  srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
  f.device->CreateShaderResourceView(input.Get(), &srv, cpu);
  cpu.ptr += stride;
  D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
  uav.Format = DXGI_FORMAT_R32_TYPELESS; uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uav.Buffer.NumElements = output_words; uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
  f.device->CreateUnorderedAccessView(output.Get(), nullptr, &uav, cpu);
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  Hr(f.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "allocator failed");
  Hr(f.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                baseline ? f.baseline_pipeline.Get() : f.pipeline.Get(),
                                IID_PPV_ARGS(&list)), "command list failed");
  list->CopyBufferRegion(input.Get(), 0, upload.Get(), 0, source_bytes);
  list->CopyBufferRegion(output.Get(), 0, initial_upload.Get(), 0, output_bytes);
  Transition(list.Get(), input.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  Transition(list.Get(), output.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  list->SetComputeRootSignature(f.root.Get());
  ID3D12DescriptorHeap* heaps[]{heap.Get()}; list->SetDescriptorHeaps(1, heaps);
  const uint32_t constants[]{sx, sy, pixel_log2, kTiles, center ? 1u : 0u};
  list->SetComputeRoot32BitConstants(0, 5, constants, 0);
  auto gpu = heap->GetGPUDescriptorHandleForHeapStart();
  list->SetComputeRootDescriptorTable(1, gpu); gpu.ptr += stride;
  list->SetComputeRootDescriptorTable(2, gpu);
  list->Dispatch(kTiles + 1, 1, 1); // Extra group must leave the sentinel tile alone.
  Transition(list.Get(), output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  list->CopyBufferRegion(readback.Get(), 0, output.Get(), 0, output_bytes);
  Hr(list->Close(), "command list close failed");
  ID3D12CommandList* lists[]{list.Get()}; f.queue->ExecuteCommandLists(1, lists); f.Wait();
  const D3D12_RANGE read_range{0, SIZE_T(output_bytes)}; void* data = nullptr;
  Hr(readback->Map(0, &read_range, &data), "readback map failed");
  const auto* observed = static_cast<const uint32_t*>(data);
  std::vector<uint32_t> result(observed, observed + output_words);
  const D3D12_RANGE no_write{0, 0}; readback->Unmap(0, &no_write);
  return result;
}
void CheckGuards(Counts& c, const std::vector<uint32_t>& result, uint32_t payload_words) {
  Require(result.size() == payload_words + kGuardWords, "unexpected readback size");
  for (uint32_t p = payload_words; p < result.size(); ++p)
    c.Check(result[p] == kSentinel, "out-of-range dispatched tile unchanged");
}
void RunCase(Fixture& f, Counts& c, uint32_t sx, uint32_t sy, bool center,
             uint32_t source_offset) {
  const auto observed = ExecuteCase(f, sx, sy, center, 2, source_offset, false);
  uint32_t mismatches = 0;
  const uint32_t sample_x = center ? sx / 2 : 0, sample_y = center ? sy / 2 : 0;
  for (uint32_t p = 0; p < kPixels; ++p) {
    const auto source_word = SourceWord(p, sx, sy, sample_x, sample_y);
    const auto expected = Label(source_word);
    ++c.checks;
    if (observed[p] != expected) {
      ++c.failures;
      if (mismatches++ < 4)
        std::cout << "MISMATCH sx=" << sx << " sy=" << sy << " center=" << center << " pixel=" << p
                  << " srv_base_byte=" << source_offset
                  << " expected_source_byte=" << source_word * 4 << " expected=0x" << std::hex
                  << expected << " actual=0x" << observed[p] << std::dec << '\n';
    }
  }
  CheckGuards(c, observed, kPixels);
  std::cout << "CASE sx=" << sx << " sy=" << sy << " center=" << center
            << " srv_base_byte=" << source_offset << " pixels=" << kPixels
            << " mismatches=" << mismatches << '\n' << std::flush;
}
void RunDifferential(Fixture& f, Counts& c, uint32_t sx, uint32_t sy, bool center,
                     uint32_t source_offset, uint32_t pixel_log2) {
  Require(pixel_log2 == 0 || pixel_log2 == 1 || pixel_log2 == 3,
          "32bpp must use correctness oracle, not baseline equivalence");
  const auto baseline = ExecuteCase(f, sx, sy, center, pixel_log2, source_offset, true);
  const auto candidate = ExecuteCase(f, sx, sy, center, pixel_log2, source_offset, false);
  Require(baseline.size() == candidate.size(), "differential sizes differ");
  const uint32_t payload_words = kPixels * (1u << pixel_log2) / 4;
  CheckGuards(c, baseline, payload_words); CheckGuards(c, candidate, payload_words);
  uint32_t mismatches = 0;
  for (uint32_t word = 0; word < payload_words; ++word) {
    ++c.checks;
    if (baseline[word] != candidate[word]) {
      ++c.failures;
      if (mismatches++ < 4)
        std::cout << "DIFFERENTIAL_MISMATCH bpp=" << (8u << pixel_log2) << " sx=" << sx
                  << " sy=" << sy << " center=" << center << " srv_base_byte=" << source_offset
                  << " output_byte=" << word * 4 << " baseline=0x" << std::hex << baseline[word]
                  << " candidate=0x" << candidate[word] << std::dec << '\n';
    }
  }
  std::cout << "EQUIVALENCE_ONLY bpp=" << (8u << pixel_log2) << " sx=" << sx << " sy=" << sy
            << " center=" << center << " srv_base_byte=" << source_offset
            << " mismatches=" << mismatches << " not_format_correctness\n" << std::flush;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
  try {
    if (argc != 2 || (std::wstring_view(argv[1]) != L"--self-test" &&
                      std::wstring_view(argv[1]) != L"--run-warp" &&
                      std::wstring_view(argv[1]) != L"--run-differential")) {
      std::cerr << "Usage: mcla_resolve_downscale_native_win_test --self-test|--run-warp|--run-differential\n";
      return 2;
    }
    const bool differential = std::wstring_view(argv[1]) == L"--run-differential";
#if !defined(MCLA_DOWNSCALE_BASELINE_HEADER)
    Require(!differential, "differential requires compile-time sealed baseline header");
#endif
    Counts counts; PureTests(counts);
    if (counts.failures) return 1;
    if (std::wstring_view(argv[1]) == L"--run-warp") {
      Fixture fixture;
      for (uint32_t sx = 1; sx <= kMaxAxis; ++sx)
        for (uint32_t sy = 1; sy <= kMaxAxis; ++sy)
          for (bool center : {false, true})
            for (const auto offset : kSourceOffsets) RunCase(fixture, counts, sx, sy, center, offset);
      Require(fixture.next_fence == 294, "incomplete correctness matrix");
    } else if (differential) {
      Fixture fixture(true);
      for (const auto pixel_log2 : {0u, 1u, 3u})
        for (const auto axes : kDifferentialScales)
          for (bool center : {false, true})
            for (const auto offset : kSourceOffsets)
              RunDifferential(fixture, counts, axes[0], axes[1], center, offset, pixel_log2);
      Require(fixture.next_fence == 216, "incomplete differential matrix");
    }
    std::cout << "checks=" << counts.checks << " failures=" << counts.failures << '\n';
    return counts.failures ? 1 : 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR " << error.what() << '\n';
    return 2;
  }
}
