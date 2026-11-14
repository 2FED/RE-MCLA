// Synthetic GPU-only swap token/resource binding test. NO window, game, save or desktop
// capture. Run exclusively under a fresh creation-owned Job with a deadline:
// the SDK fence/destructor waits are intentionally not claimed to be bounded.
#include <rex/cvar.h>
#include <rex/ui/swap_emission.h>
#include <rex/ppc/context.h>
#include <rex/ui/d3d12/d3d12_presenter.h>
#include <rex/ui/d3d12/d3d12_provider.h>
#include <atomic>
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace rex::ui;
using namespace rex::ui::d3d12;
using Microsoft::WRL::ComPtr;
namespace sw = rex::diagnostics::swap_emission;
namespace {
void Require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void Hr(HRESULT hr, const char* why) { Require(SUCCEEDED(hr), why); }
uint64_t Qpc() { LARGE_INTEGER n{}; Require(QueryPerformanceCounter(&n) && n.QuadPart > 0, "QPC"); return uint64_t(n.QuadPart); }
uint64_t Qpf() { LARGE_INTEGER n{}; Require(QueryPerformanceFrequency(&n) && n.QuadPart > 0, "QPF"); return uint64_t(n.QuadPart); }
struct Witness { uint64_t before = 0, after = 0; };
std::array<Witness, 37> witnesses{};
std::array<sw::Binding, 37> emissions{};
bool Snapshot(const PPCContext& context, const uint8_t*, sw::Snapshot& snapshot) {
  snapshot.timer_address = 0x1000;  // Synthetic identity, not a game read.
  snapshot.timer_counter = context.r4.u32;
  snapshot.timer_delta_bits = 0x3D088889;
  static_assert(sw::kSnapshotAbiVersion == 2);
  snapshot.callback_id = 1000 + context.r4.u32;  // Explicit independent synthetic ID.
  return true;
}
std::vector<GuestCaptureProvenance> captures;
struct Report {
  HANDLE file = INVALID_HANDLE_VALUE;
  ~Report() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
  void Open() {
    file = CreateFileW(L"swap-binding.tsv", GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, "exclusive report file");
  }
  void Write(const std::string& s) {
    Require(s.size() < 65536, "report bound"); DWORD written = 0;
    Require(WriteFile(file, s.data(), DWORD(s.size()), &written, nullptr) && written == s.size() &&
            FlushFileBuffers(file), "report write");
  }
};
bool Cleared(const GuestOutputCapture& c) {
  const auto& p = c.provenance;
  return !c.image.width && !c.image.height && !c.image.stride && c.image.data.empty() &&
    !p.presenter_epoch && !p.acquired_sequence && !p.qpc_frequency && !p.refresh_cpu_begin_qpc &&
    !p.refresh_cpu_end_qpc && !p.acquire_before_qpc && !p.acquire_after_qpc &&
    !p.copy_submit_before_qpc && !p.copy_completion_observed_qpc && !p.pixels_ready_qpc &&
    !p.swap_emission.match.token.epoch && !p.swap_emission.match.token.serial && !p.swap_emission.valid() &&
    p.swap_emission.match.snapshot == sw::Snapshot{};
}
uint32_t Pixel(uint64_t seq, unsigned x, unsigned y, bool packed) {
  const uint32_t max = packed ? 1023 : 255;
  const uint32_t r = (seq >> (x % 64)) & 1 ? max : 0;
  const uint32_t g = (x + y) & 1 ? max : 0;
  const uint32_t b = (seq >> (y % 64)) & 1 ? 0 : max;
  return packed ? r | (g << 10) | (b << 20) | (3u << 30)
                : r | (g << 8) | (b << 16) | (255u << 24);
}
struct Submitted {
  ComPtr<ID3D12Resource> upload;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  ComPtr<ID3D12Fence> fence;
};
bool Publish(D3D12Provider& provider, Presenter& presenter,
             std::vector<Submitted>& retained, uint64_t seq, unsigned width,
             bool affirm = true, bool callback_success = true) {
  const auto before = Qpc();
  const std::array<uint32_t, 4> stock{0x53574150, 0x1000, width, 8};
  const std::array<uint32_t, 6> fetch{uint32_t(seq), 2, 3, 4, 5, 6};
  PPCContext context{}; context.lr = 0x1234; context.r4.u32 = uint32_t(seq);
  uint8_t synthetic_memory = 0;
  sw::Payload payload;
  { sw::ExportContextScope scope(context, &synthetic_memory); payload = sw::Emit(stock, fetch); }
  auto binding = sw::Consume(payload.view(), fetch);
  Require(binding.valid() && binding.match.snapshot.snapshot_available, "synthetic emission binding");
  const bool published = presenter.RefreshGuestOutput(width, 8, width, 8,
    [&](Presenter::GuestOutputRefreshContext& raw) {
      auto& context = static_cast<D3D12Presenter::D3D12GuestOutputRefreshContext&>(raw);
      auto resource = context.resource_uav_capable();
      auto device = provider.GetDevice();
      auto desc = resource->GetDesc();
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
      UINT64 bytes = 0;
      device->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr, &bytes);
      Require(bytes && bytes <= 65536, "synthetic upload size");
      retained.emplace_back();
      auto& s = retained.back();
      D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
      D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      bd.Width = bytes; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1;
      bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      Hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bd,
         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&s.upload)), "upload");
      void* data = nullptr; D3D12_RANGE no_read{0, 0};
      Hr(s.upload->Map(0, &no_read, &data), "map upload");
      std::memset(data, 0, size_t(bytes));
      for (unsigned y = 0; y != 8; ++y) for (unsigned x = 0; x != width; ++x) {
        const auto pixel = Pixel(seq, x, y, true);
        std::memcpy(static_cast<unsigned char*>(data) + fp.Offset + y * fp.Footprint.RowPitch + x * 4, &pixel, 4);
      }
      s.upload->Unmap(0, nullptr);
      Hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s.allocator)), "allocator");
      Hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, s.allocator.Get(), nullptr,
                                  IID_PPV_ARGS(&s.list)), "command list");
      D3D12_RESOURCE_BARRIER barrier{};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = resource;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = D3D12Presenter::kGuestOutputInternalState;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
      s.list->ResourceBarrier(1, &barrier);
      D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = s.upload.Get();
      src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
      D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = resource;
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      s.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
      s.list->ResourceBarrier(1, &barrier);
      Hr(s.list->Close(), "close");
      Hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&s.fence)), "source fence");
      ID3D12CommandList* lists[] = {s.list.Get()};
      provider.GetDirectQueue()->ExecuteCommandLists(1, lists);
      const bool signaled = SUCCEEDED(provider.GetDirectQueue()->Signal(s.fence.Get(), 1));
      if (affirm) context.ConfirmDiagnosticSubmission(signaled && SUCCEEDED(device->GetDeviceRemovedReason()));
      return callback_success;
    }, binding);
  const auto after = Qpc();
  if (published) {
    Require(seq < witnesses.size(), "witness index"); witnesses[seq] = {before, after};
    emissions[seq] = binding;
  }
  return published;
}
void Verify(const GuestOutputCapture& c, uint64_t epoch) {
  const auto& p = c.provenance;
  Require(p.presenter_epoch == epoch && p.acquired_sequence <= 36, "epoch/sequence");
  const auto& expected = emissions.at(size_t(p.acquired_sequence));
  Require(p.swap_emission.valid() && p.swap_emission.match.token == expected.match.token &&
          p.swap_emission.match.snapshot == expected.match.snapshot &&
          p.swap_emission.consume_qpc == expected.consume_qpc &&
          p.swap_emission.stock == expected.stock && p.swap_emission.fetch == expected.fetch,
          "token/source slot mismatch");
  Require(p.swap_emission.match.snapshot.timer_counter == p.acquired_sequence,
          "synthetic identity/pixels mismatch");
  Require(p.swap_emission.match.snapshot.callback_id == 1000 + p.acquired_sequence,
          "callback identity lost across GPU source mailbox");
  Require(p.qpc_frequency == Qpf(), "independent clock scale");
  Require(c.image.width == (p.acquired_sequence == 3 ? 32u : 64u) && c.image.height == 8, "exact shape");
  Require(ValidGuestCaptureProvenance(p, c.image.width, c.image.height,
          c.image.stride, c.image.data.size()), "metadata");
  for (unsigned y = 0; y != c.image.height; ++y)
    for (unsigned x = 0; x != c.image.width; ++x) {
      uint32_t actual; std::memcpy(&actual, c.image.data.data() + y * c.image.stride + x * 4, 4);
      Require(actual == Pixel(p.acquired_sequence, x, y, false), "pixel/sequence mismatch");
    }
  captures.push_back(p);
}
} // namespace

int main(int argc, char** argv) {
  Report report;
  try {
    Require(argc == 2 && std::string(argv[1]) == "--owned-synthetic-swap-test", "explicit mode required");
    report.Open(); report.Write("status\tSTARTED\n");
    const auto begin = Qpc(), frequency = Qpf();
    Require(sw::Configure({0x5357415054455354, begin, begin + 12 * frequency, frequency, Snapshot}),
            "synthetic swap window");
    Require(rex::cvar::SetFlagByName("d3d12_queue_priority", "0"), "normal queue priority");
    auto provider = D3D12Provider::Create(); Require(bool(provider), "provider");
    // Resources outlive presenter shutdown, including failed-publication work.
    std::vector<Submitted> retained;
    auto presenter = provider->CreatePresenter(); Require(bool(presenter), "presenter");
    GuestOutputCapture c;
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kFailed, "inactive");
    Require(Publish(*provider, *presenter, retained, 1, 64), "publish 1");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "capture 1");
    const auto first = c.provenance; Verify(c, first.presenter_epoch);
    RawImage legacy; Require(!presenter->CaptureGuestOutput(legacy), "legacy must still require presentation");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "duplicate");
    Verify(c, first.presenter_epoch);
    Require(c.provenance.acquired_sequence == 1 && c.provenance.refresh_cpu_begin_qpc == first.refresh_cpu_begin_qpc &&
            c.provenance.refresh_cpu_end_qpc == first.refresh_cpu_end_qpc, "duplicate retains source time");
    Require(!Publish(*provider, *presenter, retained, 99, 64, true, false), "failed callback");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "old acquired resource");
    Verify(c, first.presenter_epoch); Require(c.provenance.acquired_sequence == 1, "no failed-refresh relabel");
    Require(Publish(*provider, *presenter, retained, 2, 64, false), "unaffirmed publish");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kFailed &&
            Cleared(c), "unaffirmed clears output");
    Require(Publish(*provider, *presenter, retained, 3, 32), "resize publish");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "resize capture");
    Verify(c, first.presenter_epoch); Require(c.image.width == 32, "new dimensions");
    Require(!presenter->RefreshGuestOutput(0, 0, 0, 0, [](auto&) { return false; }), "deactivate returns inactive");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kFailed &&
            Cleared(c), "inactive clears output");
    Require(Publish(*provider, *presenter, retained, 4, 64), "reactivate");
    std::atomic<bool> producer_ok{true};
    std::atomic<unsigned> requested{0}, finished{0};
    // Test-only rendezvous executes WITH the acquired-slot lock retained. Eight
    // real publications overwrite the other slots before this readback proceeds.
    static_cast<D3D12Presenter*>(presenter.get())->SetDiagnosticAcquiredHookForTesting([&] {
      const unsigned step = ++requested;
      const auto deadline = GetTickCount64() + 5000;
      while (finished.load() < step && producer_ok.load() && GetTickCount64() < deadline) Sleep(1);
      Require(producer_ok && finished.load() == step, "held-slot producer rendezvous");
    });
    std::jthread producer([&](std::stop_token stop) {
      try { for (unsigned step = 1; step <= 4; ++step) {
        const auto deadline = GetTickCount64() + 5000;
        while (requested.load() < step && !stop.stop_requested() && GetTickCount64() < deadline) Sleep(1);
        Require(!stop.stop_requested() && requested.load() == step, "producer rendezvous");
        for (unsigned seq = 5 + (step - 1) * 8; seq <= 4 + step * 8; ++seq)
          Require(Publish(*provider, *presenter, retained, seq, 64), "concurrent publish");
        finished = step;
      }
      } catch (...) { producer_ok = false; }
    });
    for (unsigned i = 0; i != 4; ++i) {
      Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "concurrent capture");
      Verify(c, first.presenter_epoch);
      Require(c.provenance.acquired_sequence == 4 + i * 8, "captured resource changed during producer advancement");
    }
    producer.join(); Require(producer_ok, "producer failure");
    static_cast<D3D12Presenter*>(presenter.get())->SetDiagnosticAcquiredHookForTesting({});
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "last capture");
    Verify(c, first.presenter_epoch); Require(c.provenance.acquired_sequence == 36, "final sequence");
    for (const auto& p : captures) {
      const auto& w = witnesses.at(size_t(p.acquired_sequence));
      Require(w.before && w.before <= p.refresh_cpu_begin_qpc && p.refresh_cpu_end_qpc <= w.after,
              "source times not inside that publication's independent witness");
      std::ostringstream row;
      row << "capture\t" << p.presenter_epoch << '\t' << p.acquired_sequence << '\t'
          << p.qpc_frequency << '\t' << w.before << '\t' << p.refresh_cpu_begin_qpc << '\t'
          << p.refresh_cpu_end_qpc << '\t' << w.after << '\t' << p.acquire_before_qpc << '\t'
          << p.acquire_after_qpc << '\t' << p.copy_submit_before_qpc << '\t'
          << p.copy_completion_observed_qpc << '\t' << p.pixels_ready_qpc << '\n';
      row << "swap_phaseid\t" << p.acquired_sequence << '\t' << p.swap_emission.match.token.epoch
          << '\t' << p.swap_emission.match.token.serial << '\t'
          << p.swap_emission.match.snapshot.timer_counter << '\t'
          << p.swap_emission.match.snapshot.qpc_before << '\t'
          << p.swap_emission.match.snapshot.qpc_after << '\t' << p.swap_emission.consume_qpc << '\t'
          << p.swap_emission.match.snapshot.callback_id << '\n';
      report.Write(row.str());
    }
    captures.clear();
    presenter.reset();
    presenter = provider->CreatePresenter(); Require(bool(presenter), "recreated presenter");
    Require(Publish(*provider, *presenter, retained, 1, 64), "new epoch publish");
    Require(presenter->CaptureGuestOutputWithProvenance(c) == GuestCaptureStatus::kSuccess, "new epoch capture");
    Require(c.provenance.presenter_epoch != first.presenter_epoch, "epoch reused");
    Verify(c, c.provenance.presenter_epoch);
    Require(witnesses[1].before <= c.provenance.refresh_cpu_begin_qpc &&
            c.provenance.refresh_cpu_end_qpc <= witnesses[1].after, "new epoch witness");
    presenter.reset(); retained.clear(); provider.reset();
    sw::StopIssuing();
    const auto stats = sw::ReadStatistics();
    Require(!stats.failed && !stats.rejected && !stats.callback_failed && stats.issued == stats.matched,
            "swap diagnostic health");
    report.Write("status\tSYNTHETIC_PHASEID_BINDING_PASS_NO_GAME_NO_DISPLAY_NO_FPS\n");
    std::cout << "SYNTHETIC_PHASEID_BINDING_PASS_NO_GAME_NO_DISPLAY_NO_FPS\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAILED: " << e.what() << '\n';
    try { if (report.file != INVALID_HANDLE_VALUE) report.Write(std::string("FAILED\t") + e.what() + "\n"); } catch (...) {}
    return 1;
  }
}
