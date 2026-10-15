#pragma once
// Register-only witness of the exact 8259E768 arithmetic/store slice. No
// guest reads, floating operations, units, physical semantics or FPS claims.
#include "mcla_vector_target_record.h"

namespace mcla::vector_store {
using InvocationId=vector_target::InvocationId;
using Vector=vector_target::Vector;
constexpr uint32_t kTarget=0x8259E768,kTail=0x8259D030;
constexpr uint64_t kCaller=0x8259F364;
struct Entry {
  InvocationId invocation{};
  vector_target::Entry target_entry{};
  vector_target::Payload slot28_pre{};
  uint32_t parent_event_index{},actual_target{};
  uint64_t caller_lr{},r3{};
  Vector v1{};
  bool operator==(const Entry&) const = default;
};
enum class EventKind:uint8_t {kEntry,kOldLoaded,kRateLoaded,kComputed,kAuxLoaded,
  kScaled,kStored,kTailPre,kTailReturn,kTailException,kNormalExit,kExceptionalExit};
enum class Stage:uint8_t {kEntered,kOldLoaded,kRateLoaded,kComputed,kAuxLoaded,
  kScaled,kStored,kTailActive,kTailReturned,kTailException};
enum class Fault:uint8_t {kNone,kIdentity,kTransition,kEntryCapacity,kEventCapacity,kDepth};
// Loaded/stored: fields[0]=original effective address, vector=raw operand.
// Computed/scaled: vector only. TailPre: LR,R3,static tail target,0 + scaled V1.
struct Payload {std::array<uint64_t,4> fields{};Vector vector{};
  bool operator==(const Payload&) const = default;};
struct Event {InvocationId invocation{};uint32_t entry_index{};
  EventKind kind=EventKind::kEntry;Payload payload{};
  bool operator==(const Event&) const = default;};
struct Frame {uint32_t entry_index{};InvocationId parent{};
  Stage stage=Stage::kEntered;Vector computed{},scaled{};
  bool operator==(const Frame&) const = default;};

template<size_t Entries=2048,size_t Events=65536,size_t Depth=8>
class Model {
  static_assert(Entries>0&&Entries<=4096&&Events>0&&Events<=65536&&Depth>0&&Depth<=64);
 public:
  explicit Model(uint64_t epoch,uint64_t target_epoch=0) noexcept:epoch_(epoch),target_epoch_(target_epoch){if(!epoch)fault_=Fault::kIdentity;}
  Model(const Model&)=delete;Model& operator=(const Model&)=delete;
  Model(Model&&)=delete;Model& operator=(Model&&)=delete;
  bool Enter(const Entry& e) noexcept {
    if(!Writable())return false;
    const auto& p=e.target_entry;const auto& c=e.slot28_pre;
    vector_target::Model<1,1,1> check(p.invocation.thread_epoch);
    if(!check.Enter(p)||e.invocation.thread_epoch!=epoch_||!e.invocation.serial||
       e.invocation.serial<=last_serial_||(target_epoch_&&p.invocation.thread_epoch!=target_epoch_)||
       p.invocation.serial<=last_target_serial_||!e.parent_event_index||e.parent_event_index>=65536||
       (entry_size_&&e.parent_event_index<=entries_[entry_size_-1].parent_event_index)||
       e.actual_target!=kTarget||e.caller_lr!=kCaller||e.r3!=p.r3||e.v1!=p.v1||
       c.fields[0]!=kCaller||c.fields[1]!=p.r3||static_cast<uint32_t>(c.fields[2])!=kTarget||
       c.fields[3]||c.vector!=p.v1)return Fail(Fault::kIdentity);
    if(depth_==Depth)return Fail(Fault::kDepth);
    if(entry_size_==Entries)return Fail(Fault::kEntryCapacity);
    if(!Room())return false;
    Frame f;f.entry_index=static_cast<uint32_t>(entry_size_);
    if(depth_)f.parent=Current().invocation;
    entries_[entry_size_++]=e;stack_[depth_++]=f;
    target_epoch_=p.invocation.thread_epoch;last_target_serial_=p.invocation.serial;last_serial_=e.invocation.serial;
    Emit(EventKind::kEntry);return true;
  }
  bool OldLoaded(InvocationId id,uint32_t ea,const Vector& v) noexcept {return Loaded(id,Stage::kEntered,Stage::kOldLoaded,EventKind::kOldLoaded,256,ea,v);}
  bool RateLoaded(InvocationId id,uint32_t ea,const Vector& v) noexcept {return Loaded(id,Stage::kOldLoaded,Stage::kRateLoaded,EventKind::kRateLoaded,272,ea,v);}
  bool Computed(InvocationId id,const Vector& v) noexcept {
    if(!At(id,Stage::kRateLoaded)||!Room())return false;
    TopFrame().computed=v;TopFrame().stage=Stage::kComputed;Payload p;p.vector=v;Emit(EventKind::kComputed,p);return true;
  }
  bool AuxLoaded(InvocationId id,uint32_t ea,const Vector& v) noexcept {return Loaded(id,Stage::kComputed,Stage::kAuxLoaded,EventKind::kAuxLoaded,288,ea,v);}
  bool Scaled(InvocationId id,const Vector& v) noexcept {
    if(!At(id,Stage::kAuxLoaded)||!Room())return false;
    TopFrame().scaled=v;TopFrame().stage=Stage::kScaled;Payload p;p.vector=v;Emit(EventKind::kScaled,p);return true;
  }
  bool Stored(InvocationId id,uint32_t ea,const Vector& v) noexcept {
    if(!At(id,Stage::kScaled))return false;
    if(ea!=Address(256)||v!=TopFrame().computed)return Fail(Fault::kTransition);
    if(!Room())return false;
    TopFrame().stage=Stage::kStored;Payload p;p.fields[0]=ea;p.vector=v;Emit(EventKind::kStored,p);return true;
  }
  bool TailPre(InvocationId id,uint64_t lr,uint64_t r3,const Vector& v) noexcept {
    if(!At(id,Stage::kStored))return false;
    if(lr!=kCaller||r3!=Current().r3||v!=TopFrame().scaled)return Fail(Fault::kTransition);
    if(!Room())return false;
    TopFrame().stage=Stage::kTailActive;Payload p;p.fields={lr,r3,kTail,0};p.vector=v;Emit(EventKind::kTailPre,p);return true;
  }
  bool TailReturn(InvocationId id,bool exception=false) noexcept {
    if(!At(id,Stage::kTailActive)||!Room())return false;
    TopFrame().stage=exception?Stage::kTailException:Stage::kTailReturned;
    Emit(exception?EventKind::kTailException:EventKind::kTailReturn);return true;
  }
  bool Exit(InvocationId id,bool normal) noexcept {
    if(!Top(id))return false;
    if(normal&&TopFrame().stage!=Stage::kTailReturned)return Fail(Fault::kTransition);
    if(!Room())return false;
    Emit(normal?EventKind::kNormalExit:EventKind::kExceptionalExit);stack_[--depth_]={};return true;
  }
  void Freeze() noexcept {frozen_=true;}
  bool frozen() const noexcept{return frozen_;}
  Fault fault() const noexcept{return fault_;}
  size_t entry_count() const noexcept{return entry_size_;}
  size_t size() const noexcept{return event_size_;}
  size_t pending_count() const noexcept{return depth_;}
  uint64_t parent_target_epoch() const noexcept{return target_epoch_;}
  const auto& entries() const noexcept{return entries_;}
  const auto& events() const noexcept{return events_;}
  const auto& pending() const noexcept{return stack_;}
 private:
  bool Loaded(InvocationId id,Stage before,Stage after,EventKind kind,uint32_t offset,uint32_t ea,const Vector& v) noexcept {
    if(!At(id,before))return false;
    if(ea!=Address(offset))return Fail(Fault::kTransition);
    if(!Room())return false;
    TopFrame().stage=after;Payload p;p.fields[0]=ea;p.vector=v;Emit(kind,p);return true;
  }
  uint32_t Address(uint32_t offset) const noexcept{return (static_cast<uint32_t>(Current().r3)+offset)&~15u;}
  bool Writable() const noexcept{return !frozen_&&fault_==Fault::kNone;}
  bool Fail(Fault f) noexcept{if(Writable())fault_=f;return false;}
  bool Room() noexcept{return event_size_<Events||Fail(Fault::kEventCapacity);}
  Frame& TopFrame() noexcept{return stack_[depth_-1];}
  const Entry& Current() const noexcept{return entries_[stack_[depth_-1].entry_index];}
  bool Top(InvocationId id) noexcept{if(!Writable())return false;return (depth_&&Current().invocation==id)||Fail(Fault::kIdentity);}
  bool At(InvocationId id,Stage s) noexcept{return Top(id)&&(TopFrame().stage==s||Fail(Fault::kTransition));}
  void Emit(EventKind k,const Payload& p={}) noexcept{events_[event_size_++]={Current().invocation,TopFrame().entry_index,k,p};}
  uint64_t epoch_{},target_epoch_{},last_serial_{},last_target_serial_{};
  size_t entry_size_{},event_size_{},depth_{};Fault fault_=Fault::kNone;bool frozen_{};
  std::array<Entry,Entries> entries_{};std::array<Event,Events> events_{};std::array<Frame,Depth> stack_{};
};
} // namespace mcla::vector_store
