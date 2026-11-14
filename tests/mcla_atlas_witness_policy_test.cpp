#include "../scripts/diagnostics/mcla_atlas_witness_policy.h"

#include <iostream>
#include <limits>

namespace {
using namespace mcla::diagnostics::atlas_witness;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr HostDesc Good() { return {77, 3, 1024, 1024, 1, 9, 1}; }
bool Recorded(Result r) { return r.kind == Kind::Recorded; }
void Shape() {
  static_assert(Matches(Good()));
  Check(Matches(Good()), "exact host shape admitted");
  for (unsigned field = 0; field < 7; ++field) {
    for (unsigned variant = 0; variant < 2; ++variant) {
      auto d = Good();
      const auto change = [variant](auto& n) { if (variant) ++n; else n = 0; };
      switch (field) {
        case 0: change(d.format); break; case 1: change(d.dimension); break;
        case 2: change(d.width); break; case 3: change(d.height); break;
        case 4: change(d.depth); break; case 5: change(d.mips); break;
        case 6: change(d.samples); break;
      }
      Policy p(true);
      Check(!Matches(d), "individual wrong shape rejected");
      Check(p.Create(d).kind == Kind::Filtered && p.creations() == 0 && !p.incomplete(),
            "noncandidate neither assigned nor failed");
      Check(p.Create(Good()).id == 1, "filter does not consume creation ID");
    }
  }
  auto d = Good(); d.width = std::numeric_limits<std::uint64_t>::max();
  Check(!Matches(d), "wide width never truncates to expected value");
  // Guest properties deliberately are not API inputs: identical host desc
  // admits regardless of caller guest dimensions / scaled-resource identity.
  Policy p(true);
  for (unsigned guestScale : {1u, 2u, 3u}) {
    (void)guestScale;
    Check(Recorded(p.Create(Good())), "host admission independent of guest scale");
  }
}
void Disabled() {
  Policy p;
  for (unsigned i = 0; i < 200; ++i) {
    Check(p.Create(Good()).kind == Kind::Disabled, "default off create no-op");
    Check(p.BeginLoad(UINT32_MAX).kind == Kind::Disabled, "disabled invalid begin no-op");
    Check(p.QueueLoad({0, UINT32_MAX}).kind == Kind::Disabled, "disabled completion no-op");
    Check(p.RecordDescriptor(0).kind == Kind::Disabled, "disabled descriptor no-op");
    Check(p.Fail(Failure::ExternalFailure).kind == Kind::Disabled, "disabled failure no-op");
  }
  Check(!p.enabled() && !p.incomplete() && p.failure() == Failure::None &&
        p.creations() == 0 && p.load_records() == 0 && p.descriptor_records() == 0 &&
        p.pending_loads() == 0 && p.TakeFailureNotification() == Failure::None,
        "disabled retains all initial state");
  Policy explicitOff(false);
  const auto r = explicitOff.Create(Good());
  Check(r.id == 0 && r.generation == 0 && r.record == 0 && !r.load_pending,
        "disabled result cannot authorize name or metadata record");
}
void Ordering() {
  Policy p(true);
  const auto id = p.Create(Good()).id;
  auto descriptor = p.RecordDescriptor(id);
  Check(Recorded(descriptor) && descriptor.generation == 0 && !descriptor.load_pending,
        "descriptor before load explicitly generation zero");
  const auto first = p.BeginLoad(id);
  Check(Recorded(first) && first.generation == 1 && first.record == 1 &&
        first.load_pending && p.incomplete(), "begin is pending, not GPU completion");
  descriptor = p.RecordDescriptor(id);
  Check(descriptor.generation == 1 && descriptor.load_pending,
        "descriptor carries pending generation explicitly");
  const auto queued = p.QueueLoad(first.token());
  Check(Recorded(queued) && queued.record == 2 && queued.generation == 1 &&
        !queued.load_pending && !p.incomplete(), "queue completion consumes separate record");
  const auto second = p.BeginLoad(id);
  Check(second.generation == 2 && second.record == 3, "generation monotonically increases");
  Check(Recorded(p.QueueLoad(second.token())) && p.load_records() == 4, "second queue completion");
  const auto id2 = p.Create(Good()).id;
  Check(id2 == 2 && p.BeginLoad(id2).generation == 1, "new ID has independent generation");
}
void InvalidOperations() {
  for (auto id : {0u, 2u, UINT32_MAX}) {
    for (unsigned op = 0; op < 3; ++op) {
      Policy p(true); p.Create(Good());
      const auto r = op == 0 ? p.BeginLoad(id) : op == 1 ? p.QueueLoad({id, 1}) : p.RecordDescriptor(id);
      Check(r.kind == Kind::Failed && p.failure() == Failure::InvalidId,
            "zero unknown unadmitted IDs fail closed");
    }
  }
  for (unsigned op = 0; op < 5; ++op) {
    Policy p(true); p.Create(Good());
    const auto first = p.BeginLoad(1);
    Result r;
    if (op == 0) r = p.BeginLoad(1);
    if (op == 1) r = p.QueueLoad({1, 0});
    if (op == 2) r = p.QueueLoad({1, UINT32_MAX});
    if (op == 3) { p.QueueLoad(first.token()); r = p.QueueLoad(first.token()); }
    if (op == 4) { p.QueueLoad(first.token()); p.BeginLoad(1); r = p.QueueLoad(first.token()); }
    Check(r.kind == Kind::Failed && p.failure() == Failure::InvalidOperation,
          "duplicate begin completion stale or invalid generation fail");
  }
  Policy p(true); p.Create(Good());
  Check(p.QueueLoad({1, 1}).kind == Kind::Failed, "completion without begin rejected");
}
void LimitsAndLatch() {
  Policy p(true);
  for (unsigned id = 1; id <= 32; ++id) Check(p.Create(Good()).id == id, "unique creation IDs 1..32");
  Check(p.Create(Good()).kind == Kind::Failed && p.failure() == Failure::CreationLimit,
        "creation cap fails, never reuse");
  Check(p.incomplete() && p.TakeFailureNotification() == Failure::CreationLimit &&
        p.TakeFailureNotification() == Failure::None, "one failure notification");
  p.Fail(Failure::ExternalFailure);
  Check(p.failure() == Failure::CreationLimit && p.TakeFailureNotification() == Failure::None,
        "first failure sticky");
  Check(p.Create({}).kind == Kind::Failed && p.BeginLoad(1).kind == Kind::Failed &&
        p.RecordDescriptor(1).kind == Kind::Failed && p.creations() == 32 &&
        p.load_records() == 0 && p.descriptor_records() == 0, "no recording after failure");

  Policy loads(true); loads.Create(Good());
  for (unsigned generation = 1; generation <= 64; ++generation) {
    const auto b = loads.BeginLoad(1);
    Check(Recorded(b) && b.generation == generation && b.record == generation * 2 - 1,
          "bounded load begin generation");
    Check(loads.QueueLoad(b.token()).record == generation * 2, "bounded queue record");
  }
  Check(loads.load_records() == 128 && !loads.incomplete(), "exact load cap fully usable");
  Check(loads.BeginLoad(1).kind == Kind::Failed && loads.failure() == Failure::LoadLimit &&
        loads.load_records() == 128 && loads.pending_loads() == 0, "load cap no counter wrap");

  Policy reserved(true);
  for (unsigned i = 0; i < 32; ++i) reserved.Create(Good());
  for (unsigned i = 0; i < 32; ++i) { auto b = reserved.BeginLoad(1); reserved.QueueLoad(b.token()); }
  std::array<LoadToken, 32> tokens{};
  for (unsigned i = 0; i < 32; ++i) tokens[i] = reserved.BeginLoad(i + 1).token();
  Check(reserved.load_records() == 96 && reserved.pending_loads() == 32, "all pending completions reserved");
  for (const auto t : tokens) Check(Recorded(reserved.QueueLoad(t)), "reserved completion remains available");
  Check(reserved.load_records() == 128 && !reserved.incomplete(), "reserved budget exactly completes");

  Policy descriptors(true); descriptors.Create(Good());
  for (unsigned i = 1; i <= 128; ++i) Check(descriptors.RecordDescriptor(1).record == i, "descriptor cap records");
  Check(descriptors.RecordDescriptor(1).kind == Kind::Failed &&
        descriptors.failure() == Failure::DescriptorLimit && descriptors.descriptor_records() == 128,
        "descriptor overflow fails without counter wrap");
  Policy invalid(true);
  invalid.Fail(static_cast<Failure>(999));
  Check(invalid.failure() == Failure::InvalidOperation, "unknown failure reason rejected");
  Policy noReason(true); noReason.Fail(Failure::None);
  Check(noReason.failure() == Failure::InvalidOperation, "empty failure reason rejected");
  Policy external(true); external.Create(Good()); external.BeginLoad(1);
  external.Fail(Failure::ExternalFailure);
  Check(external.incomplete() && external.pending_loads() == 1 &&
        external.failure() == Failure::ExternalFailure, "emission failure preserves incomplete pending evidence");
}
}  // namespace
int main() {
  Shape(); Disabled(); Ordering(); InvalidOperations(); LimitsAndLatch();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
