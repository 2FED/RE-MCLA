#include "../src/mcla_vector_target_record.h"
#include <iostream>
#include <type_traits>
using namespace mcla::vector_target;
static_assert(std::is_trivially_copyable_v<Entry> && std::is_trivially_copyable_v<Event>);
static_assert(!std::is_copy_constructible_v<Model<>> && !std::is_move_constructible_v<Model<>>);
static_assert(!kNativeExecutionProven && !kSimulationProven && !kResolvedTargetsProven && !kTotalWorkProven);
using Small=Model<8,256,4>;
static const Vector v{0x7fc01234,0x80000000,0xffffffff,0x00000001};
static const Vector changed{0,1,2,3};
static Entry Make(uint64_t serial=1,uint64_t work_serial=1,uint32_t event=10,uint64_t ordinal=3) {
  namespace w=mcla::work_emission; namespace d=mcla::consumer_detail; namespace t=mcla::virtual_detail;
  d::Entry p{{27,work_serial},{37,work_serial},{99,47,500},d::kTarget,d::kCaller,0x11,0x22};
  d::Child c{t::kCaller,0xffffffffBDF9D620ull,0x7ff80000deadbeefull,
      0xaabbccdd8258D1A0ull,t::kTarget,d::ChildState::kActive,true};
  t::Entry virt{{17,work_serial},p,c,t::kTarget,t::kCaller,c.r3,c.f1_bits};
  t::Call call{w::kCaller,c.r3,0x8000000000000000ull,0xaabbccdd82741010ull,w::kTail,t::CallState::kActive};
  w::Entry parent{{7,work_serial},virt,call,w::kBody,w::kCaller,call.r3,call.f1_bits};
  w::Payload pre; pre.fields={kCaller,ordinal,0xffff0000ffffff00ull,0x7ff80000deadbeefull,0xaabbccdd8259F300ull}; pre.vector=v;
  return {{8,serial},parent,pre,event,kTarget,kCaller,pre.fields[2],v};
}
template<class M> bool Pre(M& m,const Entry& e,Site s,const Vector& lanes=v) {
  return m.CallPre(e.invocation,s,Caller(s),e.r3,0xffff0000deadbeefull,lanes);
}
template<class M> bool Call(M& m,const Entry& e,Site s,const Vector& lanes=v) {
  return Pre(m,e,s,lanes) && m.CallReturn(e.invocation,s);
}
template<class M> bool Counter(M& m,const Entry& e,uint64_t loaded) {
  const auto address=static_cast<uint32_t>(e.r3)+664u;
  return m.CounterLoad(e.invocation,address,loaded) && m.CounterStore(e.invocation,address,loaded,loaded+1);
}
template<class M> bool Prefix(M& m,const Entry& e) {
  return m.Enter(e) && m.InitialV126(e.invocation,(static_cast<uint32_t>(e.r3)+400u)&~15u,changed) &&
    Call(m,e,Site::kDirectE790) && Call(m,e,Site::kSlot48) && Call(m,e,Site::kSlot28) &&
    Call(m,e,Site::kDirectD1E8,changed) && m.FlagStore(e.invocation,static_cast<uint32_t>(e.r3)+657u,1);
}
template<class M> bool Complete(M& m,const Entry& e,uint64_t gate,uint64_t loaded) {
  if(!Prefix(m,e) || !m.Gate100(e.invocation,gate)) return false;
  if(!gate && !Call(m,e,Site::kSlot100,changed)) return false;
  if(!Call(m,e,Site::kSlot104) || !Counter(m,e,loaded)) return false;
  const uint32_t low=static_cast<uint32_t>(loaded+1);
  if(low>32 && low<0x80000000u && !Call(m,e,Site::kSlot192,changed)) return false;
  return m.Exit(e.invocation,true);
}
int main() {
  unsigned checks=0,failures=0;
  auto check=[&](bool x){++checks;if(!x){++failures;std::cerr<<"failed check "<<checks<<'\n';}};
  const auto e=Make(); const auto id=e.invocation;
  for(uint64_t gate:{0ull,16ull}) for(uint64_t count:{0ull,31ull,32ull,33ull,0x7ffffffeull,0x7fffffffull,0xfffffffeull,0xffffffffull}) {
    Small m(8); check(Complete(m,e,gate,count)); check(m.fault()==Fault::kNone && !m.pending_count());
    check(m.entries()[0]==e); check(m.events()[1].payload.vector==changed);
    unsigned calls100=0,calls192=0;
    for(size_t n=0;n<m.size();++n) if(m.events()[n].kind==EventKind::kCallPre) {
      calls100+=m.events()[n].site==Site::kSlot100; calls192+=m.events()[n].site==Site::kSlot192;
    }
    uint32_t low=static_cast<uint32_t>(count+1);
    check(calls100==(!gate?1u:0u)); check(calls192==((low>32 && low<0x80000000u)?1u:0u));
  }
  // Repeated parent emits descending ordinals, not repeated entry association.
  { Small m(8); check(Complete(m,e,0,31)); auto next=Make(2,1,12,2); check(Complete(m,next,16,32));
    auto third=Make(3,2,50,6); check(Complete(m,third,16,0)); check(m.entry_count()==3); }
  // A different list item may dispatch another target: gaps are not invented
  // missing invocations in this selected actual-body journal.
  { Small m(8); check(Complete(m,e,16,0)); check(Complete(m,Make(2,1,20,0),16,0)); }
  for(unsigned bad=0;bad<16;++bad) {
    Small m(8); auto x=e;
    switch(bad) {
      case 0:x.invocation.thread_epoch=9;break;
      case 1:x.actual_target++;break;
      case 2:x.caller_lr++;break;
      case 3:x.r3++;break;
      case 4:x.v1[1]++;break;
      case 5:x.vector_pre.fields[0]++;break;
      case 6:x.vector_pre.fields[4]++;break;
      case 7:x.vector_pre.fields[5]=1;break;
      case 8:x.vector_pre.fields[1]=65535;break;
      case 9:x.parent_event_index=0;break;
      case 10:x.parent_event_index=65536;break;
      case 11:x.work_entry.actual_target++;break;
      case 12:x.work_entry.virtual_entry.parent_entry.actual_target++;break;
      case 13:x.work_entry.active_call0.caller_lr++;break;
      case 14:x.work_entry.invocation.serial=0;break;
      case 15:x.invocation.serial=0;break;
    }
    check(!m.Enter(x) && m.fault()==Fault::kIdentity && !m.entry_count()); check(!m.Enter(e));
  }
  for(unsigned bad=0;bad<7;++bad) {
    Small m(8); check(Complete(m,e,16,0)); auto x=Make(2,1,12,2);
    switch(bad) {
      case 0:x.parent_event_index=10;break;
      case 1:x.vector_pre.fields[1]=3;break;
      case 2:x.vector_pre.fields[1]=4;break;
      case 3:x.work_entry.r3++;break;
      case 4:x.work_entry.invocation.thread_epoch++;break;
      case 5:x.invocation.serial=1;break;
      case 6:x.work_entry.virtual_entry.parent_entry.r3++;break;
    }
    check(!m.Enter(x)); check(m.entry_count()==1);
  }
  { Small m(8,9); check(!m.Enter(e)); }
  { Small m(0); check(!m.Enter(e)); }
  { Small m(8); check(Complete(m,Make(1,2),16,0)); check(!m.Enter(Make(2,1,50))); }
  // Every legal observation boundary permits exceptional exit; no fabricated
  // normal return, store, or completion event is needed after an exception.
  for(unsigned boundary=0;boundary<21;++boundary) {
    Small m(8); check(m.Enter(e)); unsigned step=0;
    auto stop=[&](){return step++==boundary;};
    bool done=stop();
    if(!done){check(m.InitialV126(id,(static_cast<uint32_t>(e.r3)+400u)&~15u,changed));done=stop();}
    for(Site s:{Site::kDirectE790,Site::kSlot48,Site::kSlot28,Site::kDirectD1E8}) {
      if(!done){check(Pre(m,e,s));done=stop();}
      if(!done){check(m.CallReturn(id,s));done=stop();}
    }
    if(!done){check(m.FlagStore(id,static_cast<uint32_t>(e.r3)+657u,0));done=stop();}
    if(!done){check(m.Gate100(id,0));done=stop();}
    for(Site s:{Site::kSlot100,Site::kSlot104}) {
      if(!done){check(Pre(m,e,s));done=stop();}
      if(!done){check(m.CallReturn(id,s));done=stop();}
    }
    if(!done){check(m.CounterLoad(id,static_cast<uint32_t>(e.r3)+664u,32));done=stop();}
    if(!done){check(m.CounterStore(id,static_cast<uint32_t>(e.r3)+664u,32,33));done=stop();}
    if(!done){check(Pre(m,e,Site::kSlot192));done=stop();}
    if(!done){check(m.CallReturn(id,Site::kSlot192));}
    check(m.Exit(id,false)); check(!m.pending_count() && m.fault()==Fault::kNone);
  }
  // Explicit call exceptions at every direct/indirect site.
  for(unsigned which=0;which<7;++which) {
    Small m(8); check(m.Enter(e)); check(m.InitialV126(id,(static_cast<uint32_t>(e.r3)+400u)&~15u,v));
    for(unsigned i=0;i<=which;++i) {
      const auto site=static_cast<Site>(i);
      if(i==4){check(m.FlagStore(id,static_cast<uint32_t>(e.r3)+657u,0));check(m.Gate100(id,0));}
      if(i==6) check(Counter(m,e,32));
      check(Pre(m,e,site));
      check(i==which?m.CallException(id,site):m.CallReturn(id,site));
    }
    check(m.Exit(id,false)); check(m.events()[m.size()-2].kind==EventKind::kCallException);
  }
  { Small m(8); check(m.Enter(e)); check(!m.Exit(id,true)); }
  { Small m(8); check(m.Enter(e)); check(!Pre(m,e,Site::kSlot48)); }
  { Small m(8); check(m.Enter(e)); check(!m.InitialV126(id,17,v)); }
  { Small m(8); check(m.Enter(e)); check(m.InitialV126(id,(static_cast<uint32_t>(e.r3)+400u)&~15u,v));
    check(!Pre(m,e,Site::kDirectE790,changed)); }
  { Small m(8); check(m.Enter(e)); check(m.InitialV126(id,(static_cast<uint32_t>(e.r3)+400u)&~15u,v));
    check(Pre(m,e,Site::kDirectE790)); check(m.CallException(id,Site::kDirectE790)); check(!m.Exit(id,true)); }
  { Small m(8); check(Prefix(m,e)); check(!m.Gate100(id,1)); }
  { Small m(8); check(Prefix(m,e)); check(m.Gate100(id,16)); check(!Pre(m,e,Site::kSlot100)); }
  { Small m(8); check(Prefix(m,e)); check(m.Gate100(id,16)); check(!Pre(m,e,Site::kSlot104,changed)); }
  { Small m(8); check(Prefix(m,e)); check(m.Gate100(id,16)); check(Call(m,e,Site::kSlot104));
    check(m.CounterLoad(id,static_cast<uint32_t>(e.r3)+664u,0xffffffffull));
    check(!m.CounterStore(id,static_cast<uint32_t>(e.r3)+664u,0xffffffffull,0)); }
  { Small m(8); check(Prefix(m,e)); check(m.Gate100(id,16)); check(Call(m,e,Site::kSlot104));
    check(!m.CounterStore(id,static_cast<uint32_t>(e.r3)+664u,32,33)); }
  { Small m(8); check(Prefix(m,e)); check(m.Gate100(id,16)); check(Call(m,e,Site::kSlot104));
    check(m.CounterLoad(id,static_cast<uint32_t>(e.r3)+664u,32));
    check(!m.CounterStore(id,static_cast<uint32_t>(e.r3)+664u,33,34)); }
  { Small m(8); check(m.Enter(e)); check(m.InitialV126(id,(static_cast<uint32_t>(e.r3)+400u)&~15u,v));
    check(!m.CallPre(id,Site::kDirectE790,Caller(Site::kDirectE790)+4,e.r3,0,v)); }
  { Small m(8); check(m.Enter(e)); check(m.InitialV126(id,(static_cast<uint32_t>(e.r3)+400u)&~15u,v));
    check(Pre(m,e,Site::kDirectE790)); check(!m.CallReturn(id,Site::kSlot48)); }
  { Small m(8); check(m.Enter(e)); const auto before=m.size(); m.Freeze();
    check(m.frozen() && m.pending_count()==1); check(!m.Exit(id,false)); check(m.size()==before && m.fault()==Fault::kNone); }
  { Model<1,64,2> m(8); check(Complete(m,e,16,0)); check(!m.Enter(Make(2,2,50))); check(m.fault()==Fault::kEntryCapacity); }
  { Model<4,1,2> m(8); check(m.Enter(e)); check(!m.Exit(id,false)); check(m.fault()==Fault::kEventCapacity && m.pending_count()==1); }
  { Model<4,64,1> m(8); check(m.Enter(e)); check(!m.Enter(Make(2,2,50))); check(m.fault()==Fault::kDepth); }
  { Small m(8); check(m.Enter(e)); check(m.Enter(Make(2,2,50))); check(m.pending()[1].parent==id);
    check(m.Exit({8,2},false)); check(m.Exit(id,false)); }
  { Small m(8); check(m.Enter(e)); check(!m.Exit({8,2},false)); check(m.fault()==Fault::kIdentity); }
  std::cout<<checks<<" checks, "<<failures<<" failures\n"; return failures?1:0;
}
