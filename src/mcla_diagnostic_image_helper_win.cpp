// One-shot, image-only HWND WGC helper. NOT the M8 timing harness.
// Parent provides an unnamed inherited mapping and owns a kill-on-close Job,
// hard deadline and normal exit/Job-drain admission. Never inspect live pixels.
// No files, network, child processes, desktop capture, picker or input injection.
// SDK C++/WinRT; C++20; windowsapp.lib d3d11.lib dxgi.lib dwmapi.lib user32.lib.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <roapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/base.h>

#include "mcla_diagnostic_image_protocol.h"

#include <cstring>
#include <memory>
#include <span>
#include <string_view>

namespace protocol = mcla::diagnostics::image;
namespace capture = winrt::Windows::Graphics::Capture;
namespace directx = winrt::Windows::Graphics::DirectX;
namespace direct3d = winrt::Windows::Graphics::DirectX::Direct3D11;

namespace {
struct Failure {
  protocol::Failure reason;
};

void Require(bool value, protocol::Failure reason = protocol::Failure::kInternal) {
  if (!value) {
    throw Failure{reason};
  }
}

void NativeCheck(bool value) {
  if (!value) {
    const DWORD error = GetLastError();
    throw winrt::hresult_error(error ? HRESULT_FROM_WIN32(error) : E_FAIL);
  }
}

// Native/driver/COM calls can block. This soft deadline does not bound them;
// the separate parent's hard deadline and owned Job are mandatory containment.
struct Deadline {
  const ULONGLONG begin = GetTickCount64();
  void Check() const {
    Require(GetTickCount64() - begin < 3000, protocol::Failure::kTimeout);
  }
};

struct Handle {
  HANDLE value = nullptr;
  explicit Handle(HANDLE handle = nullptr) : value(handle) {}
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  ~Handle() { if (value) CloseHandle(value); }
  void Close() {
    if (value) {
      NativeCheck(CloseHandle(value) != FALSE);
      value = nullptr;
    }
  }
};

struct Apartment {
  bool active = false;
  Apartment() {
    winrt::check_hresult(RoInitialize(RO_INIT_MULTITHREADED));
    active = true;
  }
  ~Apartment() { if (active) RoUninitialize(); }
  void Close() {
    if (active) {
      RoUninitialize();
      active = false;
    }
  }
};

struct CallerDpi {
  DPI_AWARENESS_CONTEXT previous = nullptr;
  CallerDpi() {
    previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    NativeCheck(previous != nullptr);
  }
  ~CallerDpi() { if (previous) SetThreadDpiAwarenessContext(previous); }
  void Close() {
    if (previous) {
      NativeCheck(SetThreadDpiAwarenessContext(previous) != nullptr);
      previous = nullptr;
    }
  }
};

uint64_t Decimal(std::wstring_view text) {
  Require(!text.empty() && text.size() <= 20);
  uint64_t value = 0;
  for (const wchar_t character : text) {
    Require(character >= L'0' && character <= L'9');
    const uint64_t digit = static_cast<uint64_t>(character - L'0');
    Require(value <= (UINT64_MAX - digit) / 10);
    value = value * 10 + digit;
  }
  return value;
}

struct Mapping {
  Handle handle;
  std::byte *view = nullptr;
  const size_t bytes;
  protocol::Request request{};
  protocol::Result pending{};

  Mapping(uint64_t inherited_handle, uint64_t mapping_bytes)
      : handle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(inherited_handle))),
        bytes(static_cast<size_t>(mapping_bytes)) {
    DWORD flags = 0;
    NativeCheck(GetHandleInformation(handle.value, &flags) != FALSE);
    Require((flags & HANDLE_FLAG_INHERIT) != 0);
    view = static_cast<std::byte *>(MapViewOfFile(
        handle.value, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, bytes));
    NativeCheck(view != nullptr);
    try {
      protocol::Result initial;
      std::memcpy(&initial, view, sizeof(initial));
      Require(protocol::ValidateRequest(initial.request));
      Require(initial.request.hwnd <= std::numeric_limits<uintptr_t>::max());
      Require(bytes == sizeof(protocol::Result) + initial.request.payload_capacity);
      request = initial.request;
      pending.request = request;
      Require(std::memcmp(&initial, &pending, sizeof(initial)) == 0);
      for (const std::byte value : Pixels()) {
        Require(value == std::byte{});
      }
    } catch (...) {
      UnmapViewOfFile(view);
      view = nullptr;
      throw;
    }
  }
  ~Mapping() { if (view) UnmapViewOfFile(view); }
  std::span<std::byte> Pixels() {
    return {view + sizeof(protocol::Result), bytes - sizeof(protocol::Result)};
  }
  void VerifyPending() const {
    Require(view && std::memcmp(view, &pending, sizeof(pending)) == 0);
  }
  void Publish(const protocol::Result &result) {
    Require(protocol::ValidateResult(request, result, Pixels().size()));
    protocol::Result temporary = result;
    temporary.status = protocol::Status::kPending;
    std::memcpy(view, &temporary, sizeof(temporary));
    // Release publication is supplementary: parent MUST wait for process/Job.
    auto *status = reinterpret_cast<volatile LONG64 *>(
        view + offsetof(protocol::Result, status));
    InterlockedExchange64(status, static_cast<LONG64>(result.status));
  }
  void CloseView() {
    if (view) {
      NativeCheck(UnmapViewOfFile(view) != FALSE);
      view = nullptr;
    }
  }
};

struct Snapshot {
  int64_t x = 0;
  int64_t y = 0;
  uint64_t width = 0;
  uint64_t height = 0;
  bool operator==(const Snapshot &) const = default;
};

protocol::Result Shape(const protocol::Request &request, const Snapshot &bounds) {
  protocol::Result result;
  result.request = request;
  result.status = protocol::Status::kSuccess;
  result.content_x = bounds.x;
  result.content_y = bounds.y;
  result.content_width = bounds.width;
  result.content_height = bounds.height;
  uint64_t row_bytes = 0;
  uint64_t image_bytes = 0;
  Require(protocol::ImageShape(bounds.width, bounds.height, row_bytes, image_bytes),
          protocol::Failure::kInvalidImage);
  Require(protocol::PhysicalAxis(bounds.x, bounds.width) &&
              protocol::PhysicalAxis(bounds.y, bounds.height) &&
              request.client_x >= bounds.x && request.client_y >= bounds.y,
          protocol::Failure::kOwnerChanged);
  result.source_row_pitch = row_bytes;
  result.source_buffer_bytes = image_bytes;
  result.crop_x = static_cast<uint64_t>(request.client_x - bounds.x);
  result.crop_y = static_cast<uint64_t>(request.client_y - bounds.y);
  result.layout = protocol::Layout::kTopDownTightBgra8;
  result.output_row_pitch = request.client_width * 4;
  result.payload_bytes = request.payload_capacity;
  Require(protocol::ValidateResult(request, result, request.payload_capacity),
          protocol::Failure::kInvalidImage);
  return result;
}

Snapshot Attest(const protocol::Request &request, HANDLE process,
                const Deadline &deadline) {
  deadline.Check();
  Require(WaitForSingleObject(process, 0) == WAIT_TIMEOUT,
          protocol::Failure::kOwnerChanged);
  FILETIME birth{}, exited{}, kernel{}, user{};
  NativeCheck(GetProcessTimes(process, &birth, &exited, &kernel, &user) != FALSE);
  Require(((uint64_t(birth.dwHighDateTime) << 32) | birth.dwLowDateTime) ==
              request.owner_birth && GetProcessId(process) == request.owner_pid,
          protocol::Failure::kOwnerChanged);
  const HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(request.hwnd));
  Require(IsWindow(hwnd) != FALSE, protocol::Failure::kOwnerChanged);
  RECT bounds_before{};
  winrt::check_hresult(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                             &bounds_before, sizeof(bounds_before)));
  DWORD pid = 0;
  const DWORD thread = GetWindowThreadProcessId(hwnd, &pid);
  Require(pid == request.owner_pid && thread == request.owner_thread &&
              IsWindowVisible(hwnd) && !IsIconic(hwnd) &&
              GetForegroundWindow() == hwnd && GetAncestor(hwnd, GA_ROOT) == hwnd,
          protocol::Failure::kOwnerChanged);
  Require(AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(hwnd),
                                      DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) &&
              GetDpiForWindow(hwnd) == request.dpi,
          protocol::Failure::kOwnerChanged);
  RECT client{};
  POINT origin{};
  NativeCheck(GetClientRect(hwnd, &client) != FALSE);
  NativeCheck(ClientToScreen(hwnd, &origin) != FALSE);
  Require(client.left == 0 && client.top == 0 &&
              int64_t(client.right) == int64_t(request.client_width) &&
              int64_t(client.bottom) == int64_t(request.client_height) &&
              origin.x == request.client_x && origin.y == request.client_y,
          protocol::Failure::kOwnerChanged);
  DWORD cloaked = 0;
  winrt::check_hresult(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)));
  Require(cloaked == 0, protocol::Failure::kOwnerChanged);
  RECT bounds_after{};
  winrt::check_hresult(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                             &bounds_after, sizeof(bounds_after)));
  Require(bounds_before.left == bounds_after.left &&
              bounds_before.top == bounds_after.top &&
              bounds_before.right == bounds_after.right &&
              bounds_before.bottom == bounds_after.bottom &&
              bounds_after.right > bounds_after.left &&
              bounds_after.bottom > bounds_after.top,
          protocol::Failure::kOwnerChanged);
  Snapshot result{bounds_after.left, bounds_after.top,
                  uint64_t(int64_t(bounds_after.right) - bounds_after.left),
                  uint64_t(int64_t(bounds_after.bottom) - bounds_after.top)};
  (void)Shape(request, result); // Check bounded surface/client containment.
  deadline.Check();
  return result;
}

struct CaptureResources {
  capture::Direct3D11CaptureFramePool pool{nullptr};
  capture::GraphicsCaptureSession session{nullptr};
  capture::Direct3D11CaptureFrame frame{nullptr};
  void Close() {
    HRESULT first = S_OK;
    const auto close_one = [&](auto &resource) {
      if (resource) {
        try { resource.Close(); }
        catch (...) { if (SUCCEEDED(first)) first = winrt::to_hresult(); }
        resource = nullptr;
      }
    };
    close_one(frame);
    close_one(session);
    close_one(pool);
    winrt::check_hresult(first);
  }
  ~CaptureResources() { try { Close(); } catch (...) {} }
};

struct Unmap {
  ID3D11DeviceContext *context;
  ID3D11Resource *resource;
  ~Unmap() { if (context) context->Unmap(resource, 0); }
  void Close() {
    if (context) {
      context->Unmap(resource, 0);
      context = nullptr;
    }
  }
};

protocol::Result CaptureOnce(const protocol::Request &request,
                              std::span<std::byte> output,
                              const Deadline &deadline) {
  Apartment apartment;
  CallerDpi caller_dpi;
  Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                             FALSE, static_cast<DWORD>(request.owner_pid)));
  NativeCheck(process.value != nullptr);
  const Snapshot initial = Attest(request, process.value, deadline);
  const auto same_owner = [&] {
    Require(Attest(request, process.value, deadline) == initial,
            protocol::Failure::kOwnerChanged);
  };
  protocol::Result result = Shape(request, initial);
  {
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature{};
    // Hardware only; helper adapter choice does not alter or attest game adapter.
    winrt::check_hresult(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, device.put(), &feature, context.put()));
    auto dxgi_device = device.as<IDXGIDevice>();
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable.put()));
    auto projected_device = inspectable.as<direct3d::IDirect3DDevice>();
    Require(capture::GraphicsCaptureSession::IsSupported(), protocol::Failure::kUnsupported);
    auto factory = winrt::get_activation_factory<capture::GraphicsCaptureItem,
                                                   IGraphicsCaptureItemInterop>();
    capture::GraphicsCaptureItem item{nullptr};
    winrt::check_hresult(factory->CreateForWindow(
        reinterpret_cast<HWND>(static_cast<uintptr_t>(request.hwnd)),
        winrt::guid_of<capture::GraphicsCaptureItem>(), winrt::put_abi(item)));
    const auto size = item.Size();
    Require(size.Width == int64_t(initial.width) && size.Height == int64_t(initial.height),
            protocol::Failure::kInvalidImage);
    same_owner();
    CaptureResources resources;
    resources.pool = capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
        projected_device, directx::DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, size);
    resources.session = resources.pool.CreateCaptureSession(item);
    resources.session.IsCursorCaptureEnabled(false);
    // Do not disable the default capture-notification border or request an
    // exemption. No monitor/desktop/picker fallback or alternate frame retries.
    resources.session.StartCapture();
    for (;;) {
      same_owner();
      resources.frame = resources.pool.TryGetNextFrame();
      if (resources.frame) break; // Exactly the first available frame.
      Sleep(5);
    }
    same_owner();
    const auto content = resources.frame.ContentSize();
    const auto current_size = item.Size();
    Require(content.Width == size.Width && content.Height == size.Height &&
                current_size.Width == size.Width && current_size.Height == size.Height,
            protocol::Failure::kInvalidImage);
    // Intentionally no SystemRelativeTime access or producer clock correlation.
    auto access = resources.frame.Surface().as<
        ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
    D3D11_TEXTURE2D_DESC description{};
    texture->GetDesc(&description);
    Require(description.Width == UINT(size.Width) && description.Height == UINT(size.Height) &&
                description.Format == DXGI_FORMAT_B8G8R8A8_UNORM &&
                description.ArraySize == 1 && description.MipLevels == 1 &&
                description.SampleDesc.Count == 1 && description.SampleDesc.Quality == 0,
            protocol::Failure::kInvalidImage);
    D3D11_TEXTURE2D_DESC target{};
    target.Width = description.Width;
    target.Height = description.Height;
    target.MipLevels = target.ArraySize = target.SampleDesc.Count = 1;
    target.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    target.Usage = D3D11_USAGE_STAGING;
    target.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    winrt::com_ptr<ID3D11Texture2D> staging;
    winrt::check_hresult(device->CreateTexture2D(&target, nullptr, staging.put()));
    same_owner();
    context->CopyResource(staging.get(), texture.get());
    D3D11_QUERY_DESC query_description{D3D11_QUERY_EVENT, 0};
    winrt::com_ptr<ID3D11Query> query;
    winrt::check_hresult(device->CreateQuery(&query_description, query.put()));
    context->End(query.get());
    context->Flush();
    for (;;) {
      same_owner();
      const HRESULT ready = context->GetData(query.get(), nullptr, 0,
                                             D3D11_ASYNC_GETDATA_DONOTFLUSH);
      if (ready == S_OK) break;
      winrt::check_hresult(ready);
      Require(ready == S_FALSE, protocol::Failure::kCaptureFailed);
      Sleep(5);
    }
    same_owner();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ,
                                      D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped));
    Unmap unmap{context.get(), staging.get()};
    result.source_row_pitch = mapped.RowPitch;
    Require(mapped.pData && protocol::CheckedMultiply(
                result.source_row_pitch, result.content_height, result.source_buffer_bytes) &&
                protocol::ValidateResult(request, result, output.size()),
            protocol::Failure::kInvalidImage);
    const auto *source = static_cast<const std::byte *>(mapped.pData);
    for (uint64_t row = 0; row < request.client_height; ++row) {
      deadline.Check();
      // Validated crop and <=64MiB full staging footprint make every row safe.
      const uint64_t offset = (result.crop_y + row) * result.source_row_pitch + result.crop_x * 4;
      std::memcpy(output.data() + row * result.output_row_pitch,
                  source + offset, static_cast<size_t>(result.output_row_pitch));
    }
    unmap.Close();
    same_owner();
    resources.Close();
  } // All capture/COM/D3D references released before apartment shutdown.
  same_owner();
  process.Close();
  caller_dpi.Close();
  apartment.Close();
  deadline.Check();
  Require(protocol::ValidateResult(request, result, output.size()), protocol::Failure::kInvalidImage);
  return result;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  const Deadline deadline;
  std::unique_ptr<Mapping> mapping;
  try {
    Require(argc == 3);
    const uint64_t inherited_handle = Decimal(argv[1]);
    const uint64_t mapping_bytes = Decimal(argv[2]);
    Require(inherited_handle != 0 && inherited_handle != UINT64_MAX &&
                inherited_handle <= std::numeric_limits<uintptr_t>::max());
    Require(mapping_bytes >= sizeof(protocol::Result) + 4 &&
                mapping_bytes <= sizeof(protocol::Result) + protocol::kMaxPayloadBytes &&
                mapping_bytes <= std::numeric_limits<size_t>::max());
    deadline.Check();
    mapping = std::make_unique<Mapping>(inherited_handle, mapping_bytes);
    deadline.Check();
    const auto result = CaptureOnce(mapping->request, mapping->Pixels(), deadline);
    mapping->VerifyPending();
    mapping->handle.Close();
    deadline.Check();
    mapping->Publish(result);
    mapping->CloseView();
    deadline.Check();
    return 0;
  } catch (...) {
    protocol::Failure reason = protocol::Failure::kCaptureFailed;
    try { throw; }
    catch (const Failure &failure) { reason = failure.reason; }
    catch (...) {}
    if (mapping && mapping->view) {
      // Rejected bytes are never usable. Parent rejects any nonzero exit even
      // if native cleanup prevents publication or zeroing from completing.
      std::memset(mapping->Pixels().data(), 0, mapping->Pixels().size());
      protocol::Result failed;
      failed.request = mapping->request;
      failed.status = protocol::Status::kFailure;
      failed.failure = reason;
      try { mapping->Publish(failed); }
      catch (...) {}
    }
    return 1;
  }
}
