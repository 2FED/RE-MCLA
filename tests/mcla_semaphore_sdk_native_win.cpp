// Private no-game SDK integration test. Tool-mode Runtime, no XEX/GPU/input.
// One fresh explicitly supplied scratch directory; never a player profile.
#include <rex/runtime.h>
#include <rex/stream.h>
#include <rex/system/xsemaphore.h>
#include <rex/system/semaphore_witness.h>
#include <array>
#include <filesystem>
#include <iostream>

using rex::X_HANDLE;
using rex::X_STATUS;

namespace {
namespace witness = rex::diagnostics::semaphore_witness;
using rex::system::XObject;
using rex::system::XSemaphore;
struct Row { unsigned event{}; uint64_t cookie{}, ticket{}; witness::Creation creation{};
             witness::Admission admission{}; witness::Completion completion{}; };
std::array<Row, 128> rows{};
size_t used = 0;
uint64_t serial = 0;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* why) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << why << '\n'; }
}
void Add(Row row) { if (used == rows.size()) throw 1; rows[used++] = row; SetLastError(0xA123); }
void Create(void*, const witness::Creation& value) { Row row; row.event=1; row.cookie=value.cookie; row.creation=value; Add(row); }
void Destroy(void*, uint64_t cookie) { Row row; row.event=2; row.cookie=cookie; Add(row); }
uint64_t Begin(void*, const witness::Admission& value) {
  Row row; row.event=3; row.cookie=value.cookie; row.admission=value; row.ticket=++serial; Add(row); return row.ticket;
}
void End(void*, const witness::Completion& value) { Row row; row.event=4; row.ticket=value.ticket; row.completion=value; Add(row); }
void Pair(size_t start, uint64_t cookie, witness::Kind kind, witness::Result result) {
  Check(used == start+2, "exactly one admission/completion pair");
  if (used != start+2) return;
  Check(rows[start].event==3 && rows[start+1].event==4 && rows[start].cookie==cookie &&
        rows[start].admission.kind==kind && rows[start+1].completion.kind==kind &&
        rows[start].ticket==rows[start+1].ticket && rows[start+1].completion.result==result,
        "actual SDK object/kind/ticket/outcome binding");
}
void Drop(rex::system::object_ref<XSemaphore>& object) {
  const auto handles=object->handles();
  for (auto handle:handles) Check(object->kernel_state()->object_table()->RemoveHandle(handle)==X_STATUS_SUCCESS,
                                 "remove only fixture-owned handle");
  object.reset();
}
}
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  const auto root=std::filesystem::path(argv[1]);
  if(!root.is_absolute() || !std::filesystem::is_directory(root) || !std::filesystem::is_empty(root)) return 3;
  auto& registry=witness::ProcessRegistry();
  Check(registry.Register({nullptr, Create, Destroy, Begin, End}), "one exported process registry registers");
  rex::Runtime runtime({}, root/"user", {}, root/"cache", root/"metadata");
  rex::RuntimeConfig config; config.tool_mode=true;
  if(runtime.Setup(std::move(config))!=X_STATUS_SUCCESS) return 4;
  auto object=rex::system::object_ref<XSemaphore>(new XSemaphore(runtime.kernel_state()));
  Check(object->Initialize(0,8), "actual native semaphore creation");
  const auto cookie=witness::Cookie(object.get());
  Check(used==1 && rows[0].event==1 && rows[0].cookie==cookie && rows[0].creation.initial_count==0 &&
        rows[0].creation.maximum_count==8 && rows[0].creation.origin==witness::Origin::kFresh,
        "successful fresh creation metadata");
  size_t start=used; int32_t previous=-1;
  Check(object->ReleaseSemaphore(1,&previous) && previous==0, "original release result/output");
  Pair(start,cookie,witness::Kind::kRelease,witness::Result::kSuccess);
  Check(rows[start+1].completion.previous_count==0, "actual previous count transported");
  uint64_t zero=0;
  start=used; Check(object->Wait(0,0,0,&zero)==X_STATUS_SUCCESS, "original wait consumes real credit");
  Pair(start,cookie,witness::Kind::kWait,witness::Result::kSuccess);
  start=used; Check(object->Wait(0,0,0,&zero)==X_STATUS_TIMEOUT, "zero timeout without invented credit");
  Pair(start,cookie,witness::Kind::kWait,witness::Result::kTimeout);
  Check(rows[start].admission.timeout==witness::TimeoutClass::kZero, "zero timeout class");
  start=used; SetLastError(0xB234);
  const auto over_limit=object->ReleaseSemaphore(9,&previous);
  const auto native_error=GetLastError();
  Check(!over_limit, "native over-limit release fails");
  Check(native_error==ERROR_TOO_MANY_POSTS, "actual Win32 release error survives clobbering End callback");
  Pair(start,cookie,witness::Kind::kRelease,witness::Result::kFailed);
  Check(!rows[start+1].completion.previous_count, "failed output is not reported as observed count");
  X_HANDLE alias=0;
  Check(runtime.kernel_state()->object_table()->AddHandle(object.get(),&alias)==X_STATUS_SUCCESS, "fixture alias handle");
  auto aliased=runtime.kernel_state()->object_table()->LookupObject<XSemaphore>(alias);
  Check(aliased.get()==object.get(), "alias resolves same object");
  start=used; Check(aliased->ReleaseSemaphore(1,nullptr), "alias release");
  Pair(start,cookie,witness::Kind::kRelease,witness::Result::kSuccess);
  aliased.reset();
  std::array<uint8_t,1024> storage{};
  rex::stream::ByteStream stream(storage.data(),storage.size());
  start=used; Check(object->Save(&stream), "actual Save remains successful");
  Pair(start,cookie,witness::Kind::kSave,witness::Result::kUnsupported);
  Check(object->Wait(0,0,0,&zero)==X_STATUS_SUCCESS, "Save restores original credit");
  Check(object->ReleaseSemaphore(1,nullptr), "prepare bounded multi wait");
  XObject* objects[]{object.get()};
  start=used; Check(XObject::WaitMultiple(1,objects,1,0,0,0,&zero)==X_STATUS_SUCCESS, "actual WaitAny");
  Pair(start,cookie,witness::Kind::kWaitMultiple,witness::Result::kUnsupported);
  Check(object->ReleaseSemaphore(1,nullptr), "prepare bounded all wait");
  start=used; Check(XObject::WaitMultiple(1,objects,0,0,0,0,&zero)==X_STATUS_SUCCESS, "actual WaitAll");
  Pair(start,cookie,witness::Kind::kWaitMultiple,witness::Result::kUnsupported);
  start=used; Check(XObject::SignalAndWait(object.get(),object.get(),0,0,0,&zero)==X_STATUS_SUCCESS,
                   "actual SignalAndWait on fixture semaphore");
  Check(used==start+4 && rows[start].admission.kind==witness::Kind::kSignalAndWait &&
        rows[start+1].admission.kind==witness::Kind::kSignalAndWait &&
        rows[start+2].completion.result==witness::Result::kUnsupported &&
        rows[start+3].completion.result==witness::Result::kUnsupported, "both combined-call participants remain unsupported");
  start=used; Drop(object);
  Check(used==start+1 && rows[start].event==2 && rows[start].cookie==cookie, "destruction after final alias/reference release");
  auto native=rex::system::object_ref<XSemaphore>(new XSemaphore(runtime.kernel_state()));
  rex::system::X_KSEMAPHORE header{}; header.limit=8; header.header.signal_state=0;
  start=used; Check(native->InitializeNative(&header,&header.header), "native initialization");
  Check(used==start+1 && rows[start].creation.origin==witness::Origin::kNative, "native origin never fresh");
  Drop(native);
  Check(!registry.failed(), "no callback or transport fault");
  registry.Disable();
  runtime.Shutdown();
  std::cout << "SDK semaphore fixture: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
