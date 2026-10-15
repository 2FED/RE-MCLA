#include "../src/mcla_vector_store_record.h"
#include <iostream>
#include <type_traits>
using namespace mcla::vector_store;
static_assert(std::is_trivially_copyable_v<Entry>&&std::is_trivially_copyable_v<Event>);
static_assert(!std::is_copy_constructible_v<Model<>>&&!std::is_move_constructible_v<Model<>>);
using Small=Model<8,256,4>;
static const Vector v{0x7fc01234,0x80000000,0xffffffff,1};
static const Vector result{0,1,2,3};
static Entry Make(uint64_t serial=1,uint64_t parent=1,uint32_t event=7) {
  namespace w=mcla::work_emission;namespace d=mcla::consumer_detail;namespace t=mcla::virtual_detail;namespace q=mcla::vector_target;
  d::Entry p{{27,parent},{37,parent},{99,47,500},d::kTarget,d::kCaller,0x11,0x22};
  d::Child c{t::kCaller,0xffffffffBDF9D620ull,0x7ff80000deadbeefull,0xaabbccdd8258D1A0ull,t::kTarget,d::ChildState::kActive,true};
  t::Entry virt{{17,parent},p,c,t::kTarget,t::kCaller,c.r3,c.f1_bits};
  t::Call call{w::kCaller,c.r3,0x8000000000000000ull,0xaabbccdd82741010ull,w::kTail,t::CallState::kActive};
  w::Entry work{{7,parent},virt,call,w::kBody,w::kCaller,call.r3,call.f1_bits};
  w::Payload pre;pre.fields={q::kCaller,3,0xffff0000ffffff0full,0x7ff80000deadbeefull,0xaabbccdd8259F300ull};pre.vector=v;
  q::Entry target{{8,parent},work,pre,5,q::kTarget,q::kCaller,pre.fields[2],v};
  q::Payload slot;slot.fields={kCaller,target.r3,0xaabbccdd8259E768ull,0};slot.vector=v;
  return {{9,serial},target,slot,event,kTarget,kCaller,target.r3,v};
}
static uint32_t Address(const Entry& e,uint32_t n){return (static_cast<uint32_t>(e.r3)+n)&~15u;}
template<class M> bool Steps(M& m,const Entry& e,unsigned through=8) {
  const auto id=e.invocation;
  if(through>=1&&!m.OldLoaded(id,Address(e,256),v))return false;
  if(through>=2&&!m.RateLoaded(id,Address(e,272),v))return false;
  if(through>=3&&!m.Computed(id,result))return false;
  if(through>=4&&!m.AuxLoaded(id,Address(e,288),v))return false;
  if(through>=5&&!m.Scaled(id,v))return false;
  if(through>=6&&!m.Stored(id,Address(e,256),result))return false;
  if(through>=7&&!m.TailPre(id,kCaller,e.r3,v))return false;
  if(through>=8&&!m.TailReturn(id))return false;
  return true;
}
template<class M> bool Complete(M& m,const Entry& e){return m.Enter(e)&&Steps(m,e)&&m.Exit(e.invocation,true);}
int main(){
  unsigned checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok){++failures;std::cerr<<"failed "<<checks<<'\n';}};
  const auto e=Make();const auto id=e.invocation;
  {Small m(9);check(Complete(m,e));check(m.entry_count()==1&&m.size()==10&&!m.pending_count()&&m.fault()==Fault::kNone);
   check(m.entries()[0]==e);check(m.events()[1].payload.fields[0]==0); // wrap and alignment
   check(m.events()[3].payload.vector==result&&m.events()[6].payload.vector==result);
   check(m.events()[5].payload.vector==v&&m.events()[7].payload.vector==v);
   check(m.events()[7].payload.fields==std::array<uint64_t,4>{kCaller,e.r3,kTail,0});
   check(Complete(m,Make(2,2,90)));check(m.parent_target_epoch()==8);}
  for(unsigned stage=0;stage<=8;++stage){Small m(9);check(m.Enter(e));check(Steps(m,e,stage));check(m.Exit(id,false));check(m.fault()==Fault::kNone&&!m.pending_count());}
  for(unsigned stage=0;stage<8;++stage){Small m(9);check(m.Enter(e));check(Steps(m,e,stage));check(!m.Exit(id,true));check(m.fault()==Fault::kTransition);}
  {Small m(9);check(m.Enter(e));check(Steps(m,e,7));check(m.TailReturn(id,true));check(m.Exit(id,false));check(m.events()[8].kind==EventKind::kTailException);}
  for(unsigned bad=0;bad<18;++bad){auto b=e;
   switch(bad){
    case 0:b.invocation.thread_epoch++;break;case 1:b.invocation.serial=0;break;
    case 2:b.target_entry.work_entry.actual_target++;break;case 3:b.target_entry.actual_target++;break;
    case 4:b.target_entry.v1[0]++;break;case 5:b.parent_event_index=0;break;case 6:b.parent_event_index=65536;break;
    case 7:b.actual_target++;break;case 8:b.caller_lr++;break;case 9:b.r3++;break;case 10:b.v1[0]++;break;
    case 11:b.slot28_pre.fields[0]++;break;case 12:b.slot28_pre.fields[1]++;break;
    case 13:b.slot28_pre.fields[2]++;break;case 14:b.slot28_pre.fields[3]=1;break;
    case 15:b.slot28_pre.vector[0]++;break;case 16:b.target_entry.invocation.serial=0;break;
    case 17:b.target_entry.work_entry.virtual_entry.parent_entry.update.native_thread_id=0;break;
   }
   Small m(9);check(!m.Enter(b));check(m.fault()==Fault::kIdentity&&m.size()==0&&m.entry_count()==0);
  }
  for(unsigned bad=0;bad<4;++bad){Small m(9);check(Complete(m,e));auto next=Make(2,2,90);
   if(bad==0)next.invocation.serial=1;if(bad==1)next.target_entry.invocation.serial=1;
   if(bad==2)next.parent_event_index=7;if(bad==3)next.target_entry.invocation.thread_epoch++;
   check(!m.Enter(next));check(m.entry_count()==1&&m.fault()==Fault::kIdentity);}
  for(unsigned bad=0;bad<3;++bad){Small m(9);check(m.Enter(e));check(Steps(m,e,bad==0?0:bad==1?1:3));
   check(!(bad==0?m.OldLoaded(id,1,v):bad==1?m.RateLoaded(id,17,v):m.AuxLoaded(id,33,v)));check(m.fault()==Fault::kTransition);}
  for(unsigned bad=0;bad<2;++bad){Small m(9);check(m.Enter(e));check(Steps(m,e,5));check(!m.Stored(id,bad?Address(e,256):16,bad?v:result));check(m.fault()==Fault::kTransition);}
  for(unsigned bad=0;bad<3;++bad){Small m(9);check(m.Enter(e));check(Steps(m,e,6));check(!m.TailPre(id,kCaller+(bad==0),e.r3+(bad==1),bad==2?result:v));}
  {Small m(9);check(m.Enter(e));check(!m.Computed(id,result));const auto n=m.size();check(!m.OldLoaded(id,Address(e,256),v));check(m.size()==n&&m.fault()==Fault::kTransition);}
  {Small m(9);check(m.Enter(e));check(!m.OldLoaded({9,999},Address(e,256),v));check(m.fault()==Fault::kIdentity);}
  for(unsigned stage=0;stage<=8;++stage){Small m(9);check(m.Enter(e));check(Steps(m,e,stage));const auto n=m.size();const auto f=m.pending()[0];m.Freeze();check(!m.Exit(id,false));check(m.size()==n&&m.pending()[0]==f&&m.fault()==Fault::kNone);}
  {Model<1,20,1> m(9);check(Complete(m,e));check(!m.Enter(Make(2,2,20)));check(m.fault()==Fault::kEntryCapacity&&m.entry_count()==1);}
  {Model<2,2,2> m(9);check(m.Enter(e));check(m.OldLoaded(id,Address(e,256),v));check(!m.RateLoaded(id,Address(e,272),v));check(m.fault()==Fault::kEventCapacity&&m.pending_count()==1);}
  {Model<2,20,1> m(9);check(m.Enter(e));check(!m.Enter(Make(2,2,20)));check(m.fault()==Fault::kDepth&&m.pending_count()==1);}
  {Small m(9);check(m.Enter(e));auto inner=Make(2,2,20);check(m.Enter(inner));check(m.pending()[1].parent==id);check(m.Exit(inner.invocation,false));check(Steps(m,e));check(m.Exit(id,true));}
  {Small m(0);check(m.fault()==Fault::kIdentity&&!m.Enter(e));}
  {Small m(9);auto zero=e;zero.v1={};zero.target_entry.v1={};zero.target_entry.vector_pre.vector={};zero.slot28_pre.vector={};check(Complete(m,zero));}
  std::cout<<"vector store core: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
