#include "mcla_build_lease_network_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <array>
#include <atomic>
#include <memory>
#include <utility>

namespace mcla::lease {
std::expected<void, std::wstring> ValidateHttpBody(
    uint32_t status, std::optional<uint64_t> declared_length,
    std::string_view body, bool successful_eof) {
  if (status != 200) return std::unexpected(L"Public lease endpoint did not return HTTP 200");
  if (!successful_eof) return std::unexpected(L"Public lease response was interrupted");
  if (body.empty() || body.size() > kMaximumPublicLeaseBytes ||
      (declared_length && (*declared_length == 0 || *declared_length > kMaximumPublicLeaseBytes)))
    return std::unexpected(L"Public lease response violates byte bounds");
  if (declared_length && *declared_length != body.size())
    return std::unexpected(L"Public lease response length differs from Content-Length");
  return {};
}

namespace {
constexpr wchar_t kHost[] = L"raw.githubusercontent.com";
constexpr wchar_t kPath[] = L"/2FED/RE-MCLA/DebugBundle/active-builds.txt";
constexpr DWORD kCallbacks = WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE |
    WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE | WINHTTP_CALLBACK_FLAG_READ_COMPLETE |
    WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_HANDLES;
struct Completion { DWORD kind = 0; DWORD bytes = 0; };

// WinHTTP may complete handle destruction after the public function returns.
// The handle owns one reference until HANDLE_CLOSING, the documented final
// notification. No detached application thread or unbounded cleanup join.
struct Context {
  std::atomic<unsigned> references{1};
  HINTERNET session = nullptr, connection = nullptr, request = nullptr;
  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  SRWLOCK mutex = SRWLOCK_INIT;
  Completion completion;
  std::array<char, 8192> buffer{};
  void Retain() { references.fetch_add(1, std::memory_order_relaxed); }
  void Release() { if (references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this; }
  ~Context() {
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    if (event) CloseHandle(event);
  }
};
void CALLBACK Callback(HINTERNET, DWORD_PTR cookie, DWORD status, void *, DWORD bytes) noexcept {
  if (!cookie) return;
  auto *context = reinterpret_cast<Context *>(cookie);
  context->Retain();
  if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
    context->Release(); // Reference belonging to the request handle.
  } else if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE ||
             status == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE ||
             status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE ||
             status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
    // The callback never starts another request API operation: only the caller
    // does that, preventing API-after-close races at the deadline.
    AcquireSRWLockExclusive(&context->mutex);
    context->completion = {status, bytes};
    ReleaseSRWLockExclusive(&context->mutex);
    SetEvent(context->event);
  }
  context->Release();
}
struct ContextRelease { void operator()(Context *c) const { c->Release(); } };
struct CloseRequest {
  Context &context;
  ~CloseRequest() {
    if (const auto request = std::exchange(context.request, nullptr)) WinHttpCloseHandle(request);
  }
};
std::expected<Completion, std::wstring> Wait(Context &context, ULONGLONG started, DWORD expected) {
  const auto elapsed = GetTickCount64() - started;
  if (elapsed >= kPublicLeaseDeadlineMs) return std::unexpected(L"Public lease request exceeded five-second deadline");
  const auto waited = WaitForSingleObject(context.event, static_cast<DWORD>(kPublicLeaseDeadlineMs - elapsed));
  if (waited != WAIT_OBJECT_0 || GetTickCount64() - started >= kPublicLeaseDeadlineMs)
    return std::unexpected(L"Public lease request timed out or was cancelled");
  AcquireSRWLockExclusive(&context.mutex);
  const auto result = std::exchange(context.completion, Completion{});
  ReleaseSRWLockExclusive(&context.mutex);
  if (result.kind != expected) return std::unexpected(L"Public lease transport failed");
  return result;
}
std::expected<std::optional<uint64_t>, std::wstring> ContentLength(HINTERNET request) {
  std::array<wchar_t, 32> text{};
  DWORD bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
  if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                            text.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
    if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND) return std::optional<uint64_t>{};
    return std::unexpected(L"Public lease Content-Length is unreadable or oversized");
  }
  if (bytes == 0 || bytes % sizeof(wchar_t) || bytes >= text.size() * sizeof(wchar_t))
    return std::unexpected(L"Public lease Content-Length is malformed");
  uint64_t value = 0;
  for (size_t i = 0; i < bytes / sizeof(wchar_t); ++i) {
    const auto c = text[i];
    if (c < L'0' || c > L'9' || value > (kMaximumPublicLeaseBytes - static_cast<unsigned>(c - L'0')) / 10)
      return std::unexpected(L"Public lease Content-Length violates byte bounds");
    value = value * 10 + static_cast<unsigned>(c - L'0');
  }
  if (value == 0) return std::unexpected(L"Public lease response is empty");
  return std::optional<uint64_t>{value};
}
} // namespace

std::expected<std::string, std::wstring> FetchPublicLeases() {
  try {
    const auto started = GetTickCount64();
    std::unique_ptr<Context, ContextRelease> context(new Context);
    if (!context->event) return std::unexpected(L"Cannot create public lease completion event");
    // Direct connection intentionally avoids WPAD/PAC and authenticated proxies.
    context->session = WinHttpOpen(L"MCLA-R-Public-Lease/1", WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if (!context->session || !WinHttpSetTimeouts(context->session, 5000, 5000, 5000, 5000))
      return std::unexpected(L"Cannot configure public lease HTTP session");
    context->connection = WinHttpConnect(context->session, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!context->connection) return std::unexpected(L"Cannot configure public lease HTTPS connection");
    context->request = WinHttpOpenRequest(context->connection, L"GET", kPath, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    CloseRequest cleanup{*context};
    if (!context->request) return std::unexpected(L"Cannot configure public lease request");
    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION |
                     WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_KEEP_ALIVE;
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD_PTR cookie = reinterpret_cast<DWORD_PTR>(context.get());
    if (!WinHttpSetOption(context->request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
        !WinHttpSetOption(context->request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSetOption(context->request, WINHTTP_OPTION_CONTEXT_VALUE, &cookie, sizeof(cookie)))
      return std::unexpected(L"Cannot enforce public lease request privacy options");
    context->Retain();
    if (WinHttpSetStatusCallback(context->request, Callback, kCallbacks, 0) == WINHTTP_INVALID_STATUS_CALLBACK) {
      context->Release();
      return std::unexpected(L"Cannot register public lease completion callback");
    }
    if (GetTickCount64() - started >= kPublicLeaseDeadlineMs ||
        !WinHttpSendRequest(context->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, cookie))
      return std::unexpected(L"Cannot start public lease request within deadline");
    auto sent = Wait(*context, started, WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE);
    if (!sent) return std::unexpected(sent.error());
    if (!WinHttpReceiveResponse(context->request, nullptr)) return std::unexpected(L"Cannot receive public lease response");
    auto headers = Wait(*context, started, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE);
    if (!headers) return std::unexpected(headers.error());
    DWORD status = 0, bytes = sizeof(status);
    if (!WinHttpQueryHeaders(context->request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX) || status != 200)
      return std::unexpected(L"Public lease endpoint did not return HTTP 200; redirects are not followed");
    auto length = ContentLength(context->request);
    if (!length) return std::unexpected(length.error());
    std::string body;
    body.reserve(length->value_or(kMaximumPublicLeaseBytes));
    for (;;) {
      if (GetTickCount64() - started >= kPublicLeaseDeadlineMs)
        return std::unexpected(L"Public lease response exceeded five-second deadline");
      if (!WinHttpReadData(context->request, context->buffer.data(), static_cast<DWORD>(context->buffer.size()), nullptr))
        return std::unexpected(L"Cannot read public lease response");
      auto read = Wait(*context, started, WINHTTP_CALLBACK_STATUS_READ_COMPLETE);
      if (!read) return std::unexpected(read.error());
      if (read->bytes > context->buffer.size() || read->bytes > kMaximumPublicLeaseBytes - body.size())
        return std::unexpected(L"Public lease response exceeds 65536 bytes");
      if (!read->bytes) break;
      body.append(context->buffer.data(), read->bytes);
      if (*length && body.size() > **length) return std::unexpected(L"Public lease response exceeds declared length");
    }
    auto valid = ValidateHttpBody(status, *length, body, true);
    if (!valid) return std::unexpected(valid.error());
    return body;
  } catch (const std::exception &) {
    return std::unexpected(L"Public lease request failed locally");
  }
}
} // namespace mcla::lease
