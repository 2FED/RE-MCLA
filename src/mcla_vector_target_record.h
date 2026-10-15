#pragma once
// Bounded register-only journal for actual 8259F300. Callers supply already
// loaded operands and events AFTER successful loads/stores. No guest reads,
// clock, resolver, allocation, arithmetic on vectors, or simulation claims.
#include "mcla_work_emission_record.h"

namespace mcla::vector_target {
using InvocationId = work_emission::InvocationId;
using Vector = std::array<uint32_t,4>;
constexpr uint32_t kTarget = 0x8259F300;
constexpr uint64_t kCaller = 0x825886F4;
constexpr bool kNativeExecutionProven=false, kSimulationProven=false,
    kResolvedTargetsProven=false, kTotalWorkProven=false;
enum class Site : uint8_t { kDirectE790, kSlot48, kSlot28, kDirectD1E8,
                           kSlot100, kSlot104, kSlot192 };
constexpr uint64_t Caller(Site s) noexcept {
  switch(s) {
    case Site::kDirectE790:return 0x8259F334;
    case Site::kSlot48:return 0x8259F34C;
    case Site::kSlot28:return 0x8259F364;
    case Site::kDirectD1E8:return 0x8259F36C;
    case Site::kSlot100:return 0x8259F474;
    case Site::kSlot104:return 0x8259F48C;
    case Site::kSlot192:return 0x8259F4B4;
  }
  return 0;
}
struct Entry {
  InvocationId invocation;
  work_emission::Entry work_entry;
  work_emission::Payload vector_pre;
  uint32_t parent_event_index=0, actual_target=0;
  uint64_t caller_lr=0, r3=0;
  Vector v1{};
  bool operator==(const Entry&) const = default;
};
enum class EventKind : uint8_t { kEntry, kInitialV126, kCallPre, kCallReturn,
  kCallException, kFlagStore, kGate100, kCounterLoad, kCounterStore, kNormalExit, kExceptionalExit };
// InitialV126: address; CallPre: LR,R3,CTR; FlagStore: address,rawR3;
// Gate100:rawR10; CounterLoad:address,loadedU32;
// CounterStore:address,loadedU32,incrementedRawR11.
// Vector lanes only for InitialV126 and CallPre; unused payload fields zero.
struct Payload {
  std::array<uint64_t,4> fields{};
  Vector vector{};
  bool operator==(const Payload&) const = default;
};
struct Event {
  InvocationId invocation;
  uint32_t entry_index=0;
  EventKind kind=EventKind::kEntry;
  Site site=Site::kDirectE790; // Meaningful only for call events.
  Payload payload;
  bool operator==(const Event&) const = default;
};
enum class Stage : uint8_t { kEntered, kDirectE790, kSlot48, kSlot28,
  kDirectD1E8, kFlagStore, kGate100, kSlot100, kSlot104, kCounterLoad, kCounterStore,
  kSlot192, kComplete, kCallActive, kCallException };
enum class Fault : uint8_t { kNone, kIdentity, kTransition, kEntryCapacity,
  kEventCapacity, kDepth };
struct Frame {
  uint32_t entry_index=0;
  InvocationId parent;
  Stage stage=Stage::kEntered;
  Site active=Site::kDirectE790;
  uint64_t counter_loaded=0;
  bool operator==(const Frame&) const = default;
};

template<size_t Entries=2048,size_t Events=65536,size_t Depth=8>
class Model {
  static_assert(Entries>0 && Entries<=4096 && Events>0 && Events<=65536 && Depth>0 && Depth<=64);
 public:
  explicit Model(uint64_t epoch,uint64_t work_epoch=0) noexcept : epoch_(epoch),work_epoch_(work_epoch) {
    if(!epoch) fault_=Fault::kIdentity;
  }
  Model(const Model&)=delete; Model& operator=(const Model&)=delete;
  Model(Model&&)=delete; Model& operator=(Model&&)=delete;
  bool Enter(const Entry& e) noexcept {
    if(!Writable()) return false;
    const auto& w=e.work_entry; const auto& p=e.vector_pre;
    // Reuse the full existing ancestry contract, without retaining a second
    // journal or admitting any original work. The caller still owns provenance.
    work_emission::Model<1,1,1> parent_check(w.invocation.thread_epoch);
    bool unused=true; for(size_t i=5;i<p.fields.size();++i) unused=unused && !p.fields[i];
    if(!parent_check.Enter(w) || e.invocation.thread_epoch!=epoch_ || !e.invocation.serial ||
       e.invocation.serial<=last_serial_ || (work_epoch_ && w.invocation.thread_epoch!=work_epoch_) ||
       e.actual_target!=kTarget || e.caller_lr!=kCaller || p.fields[0]!=kCaller ||
       static_cast<uint32_t>(p.fields[4])!=kTarget || p.fields[1]>=UINT16_MAX ||
       !unused || e.r3!=p.fields[2] || e.v1!=p.vector || !e.parent_event_index ||
       e.parent_event_index>=65536 || w.invocation.serial<last_work_serial_)
      return Fail(Fault::kIdentity);
    if(entry_size_) {
      const auto& old=entries_[entry_size_-1];
      if(e.parent_event_index<=old.parent_event_index ||
         (w.invocation.serial==last_work_serial_ &&
          (w!=old.work_entry || p.fields[1]>=old.vector_pre.fields[1])))
        return Fail(Fault::kIdentity);
    }
    if(depth_==Depth) return Fail(Fault::kDepth);
    if(entry_size_==Entries) return Fail(Fault::kEntryCapacity);
    if(!Room()) return false;
    Frame f; f.entry_index=static_cast<uint32_t>(entry_size_);
    if(depth_) f.parent=entries_[stack_[depth_-1].entry_index].invocation;
    entries_[entry_size_++]=e; stack_[depth_++]=f;
    work_epoch_=w.invocation.thread_epoch; last_work_serial_=w.invocation.serial;
    last_serial_=e.invocation.serial; Emit(EventKind::kEntry); return true;
  }
  bool InitialV126(InvocationId id,uint32_t address,const Vector& lanes) noexcept {
    if(!At(id,Stage::kEntered)) return false;
    if(address!=((static_cast<uint32_t>(Current().r3)+400u)&~15u)) return Fail(Fault::kTransition);
    if(!Room()) return false;
    Payload p; p.fields[0]=address; p.vector=lanes;
    TopFrame().stage=Stage::kDirectE790; Emit(EventKind::kInitialV126,p); return true;
  }
  // CTR is the observed raw register even at direct calls; it is not a direct
  // target assertion. Dynamic call sites do not claim resolved callee identity.
  bool CallPre(InvocationId id,Site site,uint64_t lr,uint64_t r3,uint64_t ctr,const Vector& v1) noexcept {
    if(!Top(id)) return false;
    if(!Caller(site) || TopFrame().stage!=Ready(site) || lr!=Caller(site) || r3!=Current().r3 ||
       ((site==Site::kDirectE790 || site==Site::kSlot48 || site==Site::kSlot28 || site==Site::kSlot104) && v1!=Current().v1))
      return Fail(Fault::kTransition);
    if(!Room()) return false;
    Payload p; p.fields={lr,r3,ctr,0}; p.vector=v1;
    TopFrame().active=site; TopFrame().stage=Stage::kCallActive;
    Emit(EventKind::kCallPre,p,site); return true;
  }
  bool CallReturn(InvocationId id,Site site) noexcept { return EndCall(id,site,false); }
  bool CallException(InvocationId id,Site site) noexcept { return EndCall(id,site,true); }
  bool FlagStore(InvocationId id,uint32_t address,uint64_t raw_r3) noexcept {
    if(!At(id,Stage::kFlagStore)) return false;
    if(address!=static_cast<uint32_t>(Current().r3)+657u || raw_r3>1) return Fail(Fault::kTransition);
    if(!Room()) return false;
    Payload p; p.fields={address,raw_r3,0,0}; TopFrame().stage=Stage::kGate100;
    Emit(EventKind::kFlagStore,p); return true;
  }
  bool Gate100(InvocationId id,uint64_t raw_r10) noexcept {
    if(!At(id,Stage::kGate100)) return false;
    if(raw_r10!=0 && raw_r10!=16) return Fail(Fault::kTransition);
    if(!Room()) return false;
    Payload p; p.fields[0]=raw_r10; TopFrame().stage=raw_r10?Stage::kSlot104:Stage::kSlot100;
    Emit(EventKind::kGate100,p); return true;
  }
  bool CounterLoad(InvocationId id,uint32_t address,uint64_t loaded_u32) noexcept {
    if(!At(id,Stage::kCounterLoad)) return false;
    if(address!=static_cast<uint32_t>(Current().r3)+664u || loaded_u32>UINT32_MAX)
      return Fail(Fault::kTransition);
    if(!Room()) return false;
    Payload p; p.fields={address,loaded_u32,0,0};
    TopFrame().counter_loaded=loaded_u32; TopFrame().stage=Stage::kCounterStore;
    Emit(EventKind::kCounterLoad,p); return true;
  }
  bool CounterStore(InvocationId id,uint32_t address,uint64_t loaded_u32,uint64_t incremented_r11) noexcept {
    if(!At(id,Stage::kCounterStore)) return false;
    if(address!=static_cast<uint32_t>(Current().r3)+664u || loaded_u32>UINT32_MAX ||
       loaded_u32!=TopFrame().counter_loaded || incremented_r11!=loaded_u32+1) return Fail(Fault::kTransition);
    if(!Room()) return false;
    const auto low=static_cast<uint32_t>(incremented_r11);
    Payload p; p.fields={address,loaded_u32,incremented_r11,0};
    TopFrame().stage=low>32 && low<0x80000000u?Stage::kSlot192:Stage::kComplete;
    Emit(EventKind::kCounterStore,p); return true;
  }
  bool Exit(InvocationId id,bool normal) noexcept {
    if(!Top(id)) return false;
    if(normal && TopFrame().stage!=Stage::kComplete) return Fail(Fault::kTransition);
    if(!Room()) return false;
    Emit(normal?EventKind::kNormalExit:EventKind::kExceptionalExit); stack_[--depth_]={}; return true;
  }
  void Freeze() noexcept { frozen_=true; }
  bool frozen() const noexcept { return frozen_; }
  Fault fault() const noexcept { return fault_; }
  size_t entry_count() const noexcept { return entry_size_; }
  size_t size() const noexcept { return event_size_; }
  size_t pending_count() const noexcept { return depth_; }
  uint64_t parent_work_epoch() const noexcept { return work_epoch_; }
  const auto& entries() const noexcept { return entries_; }
  const auto& events() const noexcept { return events_; }
  const auto& pending() const noexcept { return stack_; }
 private:
  static Stage Ready(Site s) noexcept {
    switch(s) {
      case Site::kDirectE790:return Stage::kDirectE790;
      case Site::kSlot48:return Stage::kSlot48;
      case Site::kSlot28:return Stage::kSlot28;
      case Site::kDirectD1E8:return Stage::kDirectD1E8;
      case Site::kSlot100:return Stage::kSlot100;
      case Site::kSlot104:return Stage::kSlot104;
      case Site::kSlot192:return Stage::kSlot192;
    }
    return Stage::kEntered;
  }
  static Stage After(Site s) noexcept {
    switch(s) {
      case Site::kDirectE790:return Stage::kSlot48;
      case Site::kSlot48:return Stage::kSlot28;
      case Site::kSlot28:return Stage::kDirectD1E8;
      case Site::kDirectD1E8:return Stage::kFlagStore;
      case Site::kSlot100:return Stage::kSlot104;
      case Site::kSlot104:return Stage::kCounterLoad;
      case Site::kSlot192:return Stage::kComplete;
    }
    return Stage::kEntered;
  }
  bool EndCall(InvocationId id,Site s,bool exception) noexcept {
    if(!At(id,Stage::kCallActive)) return false;
    if(TopFrame().active!=s) return Fail(Fault::kTransition);
    if(!Room()) return false;
    TopFrame().stage=exception?Stage::kCallException:After(s);
    Emit(exception?EventKind::kCallException:EventKind::kCallReturn,{},s); return true;
  }
  bool Writable() const noexcept { return !frozen_ && fault_==Fault::kNone; }
  bool Fail(Fault f) noexcept { if(Writable()) fault_=f; return false; }
  bool Room() noexcept { return event_size_<Events || Fail(Fault::kEventCapacity); }
  Frame& TopFrame() noexcept { return stack_[depth_-1]; }
  const Entry& Current() const noexcept { return entries_[stack_[depth_-1].entry_index]; }
  bool Top(InvocationId id) noexcept {
    if(!Writable()) return false;
    return (depth_ && Current().invocation==id) || Fail(Fault::kIdentity);
  }
  bool At(InvocationId id,Stage s) noexcept {
    return Top(id) && (TopFrame().stage==s || Fail(Fault::kTransition));
  }
  void Emit(EventKind k,const Payload& p={},Site s=Site::kDirectE790) noexcept {
    events_[event_size_++]={Current().invocation,TopFrame().entry_index,k,s,p};
  }
  uint64_t epoch_=0,work_epoch_=0,last_serial_=0,last_work_serial_=0;
  size_t entry_size_=0,event_size_=0,depth_=0;
  Fault fault_=Fault::kNone; bool frozen_=false;
  std::array<Entry,Entries> entries_{};
  std::array<Event,Events> events_{};
  std::array<Frame,Depth> stack_{};
};
} // namespace mcla::vector_target
