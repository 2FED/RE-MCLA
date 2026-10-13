#include "../src/mcla_work_emission_record.h"
#include <iostream>
#include <type_traits>
using namespace mcla::work_emission;
static_assert(std::is_trivially_copyable_v<Entry> && std::is_trivially_copyable_v<Event>);
static_assert(sizeof(Event) < sizeof(Entry));
static_assert(!std::is_copy_constructible_v<Model<>> && !std::is_copy_assignable_v<Model<>>);
static_assert(!std::is_move_constructible_v<Model<>> && !std::is_move_assignable_v<Model<>>);
static_assert(!kNativeExecutionProven && !kResolvedTargetsProven && !kTaskCompletionProven && !kSimulationProven);
using Small = Model<8,128,4>;
static const std::array<uint64_t,6> args{0xffff000000000003ull,4,5,6,7,8};
static const std::array<uint32_t,4> vector{0x7fc01234u,0x80000000u,0xffffffffu,0};
static Entry Make(uint64_t own=1, uint64_t virt=10, uint64_t detail=20, uint64_t router=30) {
  mcla::consumer_detail::Entry p{{27,detail},{37,router},{99,47,500},
      mcla::consumer_detail::kTarget,mcla::consumer_detail::kCaller,0x11,0x22};
  mcla::consumer_detail::Child c{mcla::virtual_detail::kCaller,0xffffffffBDF9D620ull,
      0x7ff80000deadbeefull,0xaabbccdd8258D1A0ull,mcla::virtual_detail::kTarget,
      mcla::consumer_detail::ChildState::kActive,true};
  mcla::virtual_detail::Entry v{{17,virt},p,c,mcla::virtual_detail::kTarget,
      mcla::virtual_detail::kCaller,c.r3,c.f1_bits};
  mcla::virtual_detail::Call call{kCaller,0xffffffffBDF9D620ull,0x8000000000000000ull,
      0xaabbccdd82741010ull,kTail,mcla::virtual_detail::CallState::kActive};
  return {{7,own},v,call,kBody,kCaller,call.r3,call.f1_bits};
}
template<size_t E,size_t V,size_t D>
static bool Scalar(Model<E,V,D>& m, InvocationId id, uint64_t index) {
  return m.ScalarPre(id,index,0xffffffff00000003ull,0x7ff80000deadbeefull,
                     0x8000000000000000ull,0xaabbccdd00000000ull);
}
template<size_t E,size_t V,size_t D>
static bool Vector(Model<E,V,D>& m, InvocationId id, uint64_t index) {
  return m.VectorPre(id,index,3,0x7ff0000000000001ull,0xffffffffDEADBEEFull,vector);
}
// Setup all legal pending stages, without running any original operations.
template<size_t E,size_t V,size_t D>
static bool Prepare(Model<E,V,D>& m, unsigned stage) {
  const auto e=Make(); const auto id=e.invocation;
  if(!m.Enter(e))return false;
  if(stage==0)return true;
  if(stage==15)return m.MainGate(id,0);
  if(!m.MainGate(id,1))return false;
  if(stage==1)return true;
  if(!m.ScalarCount(id,2))return false;
  if(stage==2)return true;
  if(stage<=5 || stage==16) {
    if(!Scalar(m,id,1))return false;
    if(stage==3)return true;
    if(stage==16)return m.ScalarException(id);
    if(!m.ScalarReturn(id,0xffffffff00000100ull))return false;
    if(stage==4)return true;
    return m.CleanupPre(id,3,0xffff0000DEADBEEFull);
  }
  if(stage<=11 || stage==17) {
    if(!m.ModeGate(id,0))return false;
    if(stage==6)return true;
    if(stage==10 || stage==11 || stage==17) {
      if(!m.JobBound(id,0))return false;
      if(stage==10)return true;
      if(!m.WaitPre(id,0,4,1))return false;
      return stage==17 ? m.WaitReturn(id) : true;
    }
    if(!m.JobBound(id,1))return false;
    if(stage==7)return true;
    if(!m.JobPre(id,0,args,0x7fc01234u))return false;
    if(stage==8)return true;
    return m.JobReturn(id,0xDEADBEEF);
  }
  if(!m.ModeGate(id,1))return false;
  if(stage==12)return true;
  if(stage==18)return m.VectorCount(id,0);
  if(!m.VectorCount(id,2))return false;
  if(stage==13)return true;
  return Vector(m,id,1);
}
int main() {
  unsigned checks=0,failures=0;
  auto check=[&](bool value){++checks;if(!value)++failures;};
  const auto e=Make(); const auto id=e.invocation;
  {
    Small m(7); auto mutable_entry=e; check(m.Enter(mutable_entry)); mutable_entry.r3=0;
    check(m.entries()[0]==e && m.entry_count()==1 && m.size()==1);
    check(m.MainGate(id,0)); check(m.Exit(id,true));
    check(m.events()[2].kind==EventKind::kNormalExit && m.pending_count()==0);
    check(m.events()[0].entry_index==0 && m.events()[0].invocation==id);
    check(m.events()[0].payload==Payload{}); // No manufactured entry operands.
    check(m.parent_virtual_epoch()==17 && m.parent_detail_epoch()==27 && m.parent_router_epoch()==37);
    check(m.entries()[0].active_call0.requested_target!=m.entries()[0].actual_target);
  }
  // Scalar filtering permits gaps, never duplicate/increasing loop indices.
  {
    Small m(7); check(m.Enter(e)); check(m.MainGate(id,1)); check(m.ScalarCount(id,10));
    check(Scalar(m,id,8)); check(m.ScalarReturn(id,0xffff000000000101ull));
    check(Scalar(m,id,2)); check(m.ScalarReturn(id,0xffffffff00000100ull));
    check(m.CleanupPre(id,0xffff000000000009ull,0xffffffff82345678ull)); check(m.CleanupReturn(id));
    check(m.ModeGate(id,255)); check(m.VectorCount(id,0));
    const auto f=m.pending()[0]; check(f.scalar_emissions==2 && f.cleanup_emissions==1 && f.scalar_upper==2);
    check(f.scalar_return==0xffffffff00000100ull && f.stage==Stage::kComplete);
    check(m.events()[3].payload.fields[3]==0x7ff80000deadbeefull && m.events()[3].payload.fields[4]==0x8000000000000000ull);
    check(m.events()[3].payload.fields[5]==0xaabbccdd00000000ull);
    check(m.Exit(id,true) && m.fault()==Fault::kNone);
  }
  // All scalar candidates may filter out. The prelude is still unobserved.
  for(uint64_t count:{0ull,65535ull}) {
    Small m(7); check(m.Enter(e)); check(m.MainGate(id,1)); check(m.ScalarCount(id,count));
    check(m.ModeGate(id,1)); check(m.VectorCount(id,0)); check(m.Exit(id,true));
    check(m.size()==6 && m.entries()[0]==e);
  }
  // Initial signed32 nonpositive job bounds still require the original wait.
  for(uint64_t bound:{0ull,0x80000000ull,0xffffffffull}) {
    Small m(7); check(m.Enter(e)); check(m.MainGate(id,1)); check(m.ScalarCount(id,0));
    check(m.ModeGate(id,0)); check(m.JobBound(id,bound));
    check(m.pending()[0].stage==Stage::kWaitRequired && m.pending()[0].job_bound==bound);
    check(m.WaitPre(id,bound,0xffffffff00000112ull,1)); check(m.WaitReturn(id));
    check(m.Exit(id,true)); check(m.size()==8 && m.events()[4].kind==EventKind::kJobInitialBound);
  }
  {
    Small m(7); check(m.Enter(e)); check(m.MainGate(id,1)); check(m.ScalarCount(id,0)); check(m.ModeGate(id,0));
    check(m.JobBound(id,1)); check(m.JobPre(id,0,args,0x7fc01234u)); check(m.JobReturn(id,0x123456789abcdef0ull));
    check(m.JobBound(id,3)); // Growing bound, not inferred from the initial count.
    check(m.JobPre(id,1,args,0x80000000u)); check(m.JobReturn(id,0xFFFFFFFF));
    check(m.JobBound(id,1)); // Shrinks below next ordinal; wait uses this exact raw bound.
    check(m.pending()[0].next_job==2 && m.pending()[0].job_emissions==2 && m.pending()[0].stage==Stage::kWaitRequired);
    check(m.WaitPre(id,1,4,1)); check(m.WaitReturn(id)); check(m.Exit(id,true));
    const auto& p=m.events()[5].payload; check(p.fields[8]==0x7fc01234u && p.fields[9]==0 && p.vector==std::array<uint32_t,4>{});
    for(size_t i=0;i<6;++i)check(p.fields[i+2]==args[i]);
    check(m.events()[7].kind==EventKind::kJobReloadBound && m.events()[7].payload.fields[0]==3);
  }
  {
    Small m(7); check(m.Enter(e)); check(m.MainGate(id,1)); check(m.ScalarCount(id,0)); check(m.ModeGate(id,1));
    check(m.VectorCount(id,3));
    for(uint64_t index=3;index!=0;--index){check(Vector(m,id,index-1));check(m.VectorReturn(id));}
    check(m.pending()[0].vector_emissions==3 && m.pending()[0].stage==Stage::kComplete);
    check(m.events()[5].payload.vector==vector && m.events()[5].payload.fields[3]==0x7ff0000000000001ull);
    check(m.events()[5].payload.fields[4]==0xffffffffDEADBEEFull); check(m.Exit(id,true));
  }
  // Exceptional exits and freeze at every valid stage preserve incomplete work.
  for(unsigned stage=0;stage!=19;++stage) {
    Small m(7); check(Prepare(m,stage)); const auto before=m.pending()[0];
    check(m.Exit(id,false)); check(m.events()[m.size()-1].kind==EventKind::kExceptionalExit);
    check(m.pending_count()==0 && m.entries()[0]==e && m.fault()==Fault::kNone);
    Small cut(7); check(Prepare(cut,stage));
    const auto entries=cut.entries(); const auto events=cut.events(); const auto stack=cut.pending(); const auto n=cut.size();
    cut.Freeze(); cut.Freeze();
    check(cut.frozen() && cut.pending_count()==1 && cut.pending()[0]==before);
    check(!cut.Enter(Make(2,11,21,31)) && !cut.MainGate(id,0) && !cut.ScalarCount(id,0) && !Scalar(cut,id,0) &&
          !cut.ScalarReturn(id,0) && !cut.ScalarException(id) && !cut.CleanupPre(id,0,0) && !cut.CleanupReturn(id) &&
          !cut.CleanupException(id) && !cut.ModeGate(id,0) && !cut.JobBound(id,0) && !cut.JobPre(id,0,args,0) &&
          !cut.JobReturn(id,0) && !cut.JobException(id) && !cut.WaitPre(id,0,0,1) && !cut.WaitReturn(id) &&
          !cut.WaitException(id) && !cut.VectorCount(id,0) && !Vector(cut,id,0) && !cut.VectorReturn(id) &&
          !cut.VectorException(id) && !cut.Exit(id,false));
    check(cut.entries()==entries && cut.events()==events && cut.pending()==stack && cut.size()==n && cut.fault()==Fault::kNone);
  }
  for(unsigned active:{3u,5u,8u,11u,14u}) {
    Small m(7); check(Prepare(m,active)); bool ended=false;
    if(active==3)ended=m.ScalarException(id);
    if(active==5)ended=m.CleanupException(id);
    if(active==8)ended=m.JobException(id);
    if(active==11)ended=m.WaitException(id);
    if(active==14)ended=m.VectorException(id);
    check(ended && m.pending()[0].stage==Stage::kEmissionException);
    check(m.Exit(id,false) && m.fault()==Fault::kNone);
  }
  {
    Small m(7); check(Prepare(m,3)); const auto outer=m.pending()[0]; const auto inner=Make(2,11,21,31);
    check(m.Enter(inner)); check(m.pending()[1].parent==id && m.pending()[1].entry_index==1);
    check(m.MainGate(inner.invocation,0)); check(m.Exit(inner.invocation,true));
    check(m.pending_count()==1 && m.pending()[0]==outer && m.entries()[0]==e && m.entries()[1]==inner);
    check(m.ScalarReturn(id,1)); check(m.ModeGate(id,1)); check(m.VectorCount(id,0)); check(m.Exit(id,true));
    for(size_t i=0;i<m.size();++i)check(m.events()[i].invocation==m.entries()[m.events()[i].entry_index].invocation);
  }
  // One body per exact parent call0; strict nonwrapping IDs across exits/nesting.
  for(bool nested:{false,true})for(unsigned field=0;field!=4;++field) {
    Small m(7); check(m.Enter(e)); if(!nested)check(m.Exit(id,false)); auto x=Make(2,11,21,31);
    if(field==0)x.invocation.serial=1;
    if(field==1)x.virtual_entry.invocation.serial=10;
    if(field==2)x.virtual_entry.parent_entry.invocation.serial=20;
    if(field==3)x.virtual_entry.parent_entry.router.serial=30;
    const auto entries=m.entries();const auto events=m.events();const auto stack=m.pending();
    check(!m.Enter(x) && m.fault()==Fault::kIdentity); check(m.entries()==entries && m.events()==events && m.pending()==stack);
  }
  {
    Small m(7); auto maximum=Make(UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX);
    check(m.Enter(maximum));check(m.Exit(maximum.invocation,false));
    check(!m.Enter(e) && m.fault()==Fault::kIdentity);
  }
  for(unsigned field=0;field!=4;++field) {
    Small m(7);check(m.Enter(e));check(m.Exit(id,false));const auto gap=Make(10,100,200,300);
    check(m.Enter(gap));check(m.Exit(gap.invocation,false));auto x=Make(11,101,201,301);
    if(field==0)x.invocation.serial=9;
    if(field==1)x.virtual_entry.invocation.serial=99;
    if(field==2)x.virtual_entry.parent_entry.invocation.serial=199;
    if(field==3)x.virtual_entry.parent_entry.router.serial=299;
    check(!m.Enter(x) && m.fault()==Fault::kIdentity && m.entry_count()==2);
  }
  // Capacity checks precede all payload/entry mutation.
  {
    Model<1,32,4> m(7);check(m.Enter(e));check(m.Exit(id,false));const auto entries=m.entries();
    check(!m.Enter(Make(2,11,21,31)) && m.fault()==Fault::kEntryCapacity && m.entries()==entries && m.entry_count()==1);
    Model<8,1,4> n(7);check(n.Enter(e));const auto stack=n.pending();const auto events=n.events();
    check(!n.MainGate(id,0) && n.fault()==Fault::kEventCapacity && n.pending()==stack && n.events()==events);
    Model<8,3,4> exact(7);check(exact.Enter(e));check(exact.MainGate(id,0));check(exact.Exit(id,true));
    check(!exact.Enter(Make(2,11,21,31)) && exact.fault()==Fault::kEventCapacity && exact.entry_count()==1);
    Model<8,32,1> depth(7);check(depth.Enter(e));check(!depth.Enter(Make(2,11,21,31)) && depth.fault()==Fault::kDepth);
  }
  // Full storage at distinct branch/emission boundaries keeps the exact prefix.
  for(unsigned stage:{4u,7u,10u,13u}) {
    Model<8,5,4> m(7);check(Prepare(m,stage));const auto entries=m.entries();const auto events=m.events();const auto stack=m.pending();
    bool accepted=true;
    if(stage==4)accepted=m.CleanupPre(id,0,0);
    if(stage==7)accepted=m.JobPre(id,0,args,0);
    if(stage==10)accepted=m.WaitPre(id,0,0,1);
    if(stage==13)accepted=Vector(m,id,1);
    check(!accepted && m.fault()==Fault::kEventCapacity);
    check(m.entries()==entries && m.events()==events && m.pending()==stack && m.size()==5);
  }
  for(unsigned stage:{5u,8u,11u,14u}) {
    Model<8,6,4> m(7);check(Prepare(m,stage));const auto events=m.events();const auto stack=m.pending();
    bool accepted=true;
    if(stage==5)accepted=m.CleanupReturn(id);
    if(stage==8)accepted=m.JobReturn(id,0);
    if(stage==11)accepted=m.WaitReturn(id);
    if(stage==14)accepted=m.VectorReturn(id);
    check(!accepted && m.fault()==Fault::kEventCapacity && m.events()==events && m.pending()==stack);
  }
  {
    Model<8,7,4> m(7);check(Prepare(m,9));const auto stack=m.pending();
    check(!m.JobBound(id,0) && m.fault()==Fault::kEventCapacity && m.pending()==stack);
    Model<8,4,4> n(7);check(Prepare(n,3));const auto pending=n.pending();
    check(!n.ScalarException(id) && n.fault()==Fault::kEventCapacity && n.pending()==pending);
  }
  // Invalid orders, widths, mandatory cleanup/reloads/wait, raw LR and indices.
  for(unsigned bad=0;bad!=26;++bad) {
    Small m(7);bool accepted=true;
    if(bad<=4)check(Prepare(m,0));else if(bad<=8)check(Prepare(m,2));
    else if(bad<=11)check(Prepare(m,4));else if(bad<=14)check(Prepare(m,7));
    else if(bad<=17)check(Prepare(m,9));else if(bad<=20)check(Prepare(m,10));
    else if(bad<=23)check(Prepare(m,13));else check(Prepare(m,16));
    switch(bad) {
      case 0:accepted=m.Exit(id,true);break;
      case 1:accepted=m.MainGate(id,256);break;
      case 2:accepted=m.ModeGate(id,0);break;
      case 3:accepted=m.MainGate({7,2},0);break;
      case 4:check(m.MainGate(id,1));accepted=m.ScalarCount(id,65536);break;
      case 5:accepted=Scalar(m,id,2);break;
      case 6:accepted=m.ScalarPre(id,1,0,0,0,0,kScalarLr+1);break;
      case 7:check(Scalar(m,id,1));check(m.ScalarReturn(id,1));accepted=Scalar(m,id,1);break;
      case 8:accepted=m.ModeGate(id,256);break;
      case 9:accepted=m.ModeGate(id,0);break;
      case 10:accepted=Scalar(m,id,0);break;
      case 11:accepted=m.CleanupPre(id,0,0,kCleanupLr+1);break;
      case 12:accepted=m.WaitPre(id,1,0,1);break;
      case 13:accepted=m.JobPre(id,1,args,0);break;
      case 14:accepted=m.JobPre(id,0,args,0,kJobLr+1);break;
      case 15:accepted=m.JobPre(id,1,args,0);break;
      case 16:accepted=m.WaitPre(id,1,0,1);break;
      case 17:accepted=m.JobBound(id,0x100000000ull);break;
      case 18:accepted=m.Exit(id,true);break;
      case 19:accepted=m.WaitPre(id,1,0,1);break;
      case 20:accepted=m.WaitPre(id,0,0,0);break;
      case 21:accepted=Vector(m,id,0);break;
      case 22:accepted=m.VectorPre(id,1,0,0,0,vector,kVectorLr+1);break;
      case 23:accepted=m.VectorReturn(id);break;
      case 24:accepted=m.Exit(id,true);break;
      case 25:accepted=m.ScalarReturn(id,1);break;
    }
    check(!accepted && m.fault()!=Fault::kNone);const auto n=m.size();check(!m.Exit(id,false) && m.size()==n);
  }
  for(unsigned bad=0;bad!=24;++bad) {
    Small m(7);auto x=e;
    switch(bad) {
      case 0:x.invocation.thread_epoch=17;break;
      case 1:x.invocation.serial=0;break;
      case 2:x.virtual_entry.invocation.thread_epoch=0;break;
      case 3:x.virtual_entry.invocation.serial=0;break;
      case 4:x.virtual_entry.parent_entry.invocation.thread_epoch=0;break;
      case 5:x.virtual_entry.parent_entry.invocation.serial=0;break;
      case 6:x.virtual_entry.parent_entry.router.thread_epoch=0;break;
      case 7:x.virtual_entry.parent_entry.router.serial=0;break;
      case 8:x.virtual_entry.parent_entry.update={1,0,0};break;
      case 9:x.virtual_entry.parent_entry.actual_target=0;break;
      case 10:x.virtual_entry.parent_entry.caller_lr=0;break;
      case 11:x.virtual_entry.parent_child.state=mcla::consumer_detail::ChildState::kReturned;break;
      case 12:x.virtual_entry.parent_child.indirect=false;break;
      case 13:x.virtual_entry.parent_child.indirect_ctr^=1;break;
      case 14:x.virtual_entry.actual_target=0;break;
      case 15:x.virtual_entry.r3^=1;break;
      case 16:x.active_call0.state=mcla::virtual_detail::CallState::kReturned;break;
      case 17:x.active_call0.caller_lr=kScalarLr;break;
      case 18:x.active_call0.requested_target=kBody;break;
      case 19:x.active_call0.indirect_ctr^=1;break;
      case 20:x.actual_target=kTail;break;
      case 21:x.caller_lr=kScalarLr;break;
      case 22:x.r3^=1ull<<40;break;
      case 23:x.f1_bits^=1ull<<40;break;
    }
    check(!m.Enter(x) && m.fault()==Fault::kIdentity && m.entry_count()==0 && m.size()==0);
    check(m.parent_virtual_epoch()==0 && m.parent_detail_epoch()==0 && m.parent_router_epoch()==0);
  }
  for(unsigned which=0;which!=3;++which) {
    Small m(7);check(m.Enter(e));check(m.Exit(id,false));auto x=Make(2,11,21,31);
    if(which==0)x.virtual_entry.invocation.thread_epoch=18;
    if(which==1)x.virtual_entry.parent_entry.invocation.thread_epoch=28;
    if(which==2)x.virtual_entry.parent_entry.router.thread_epoch=38;
    check(!m.Enter(x) && m.fault()==Fault::kIdentity);
  }
  {
    Small zero(0);check(!zero.Enter(e));Small fixed(7,18);check(!fixed.Enter(e));
    Small frozen(7);frozen.Freeze();check(!frozen.Enter(e) && frozen.entry_count()==0 && frozen.parent_virtual_epoch()==0);
    Small unbound(7);auto x=e;x.virtual_entry.parent_entry.update={};
    check(unbound.Enter(x) && unbound.entries()[0].virtual_entry.parent_entry.update==mcla::consumer_detail::UpdateBinding{});
  }
  std::cout<<checks<<" checks, "<<failures<<" failures\n";
  return failures?1:0;
}
