// Guest-free tests only. All accessible addresses are this test's own temporary
// VirtualAlloc pages. No game, files, process attachment or native admission.
#include "mcla_fixed60_word_edit_win.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
namespace edit=mcla::fixed60_word_edit;
unsigned checks{},queries{},cases{},reads{};
void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Pages {
  uint8_t* data{};size_t bytes{};
  explicit Pages(size_t size=8192):bytes(size){
    data=static_cast<uint8_t*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!data)throw std::runtime_error("owned allocation failed");
  }
  ~Pages(){if(data)VirtualFree(data,0,MEM_RELEASE);}
  Pages(const Pages&)=delete;Pages& operator=(const Pages&)=delete;
  uintptr_t Base(size_t offset=0)const{return reinterpret_cast<uintptr_t>(data+offset)-0x2000;}
  void Protect(DWORD protection,size_t offset=0,size_t size=4096){DWORD old{};
    Check(VirtualProtect(data+offset,size,protection,&old)!=FALSE,"owned protection change");}
};
void Seed(uint8_t* p,uint32_t bits=edit::kExpectedGuestBits){
  std::memset(p,0xA5,108);for(unsigned i=0;i<4;++i)p[84+i]=uint8_t(bits>>(24-8*i));
}
std::array<uint8_t,108> Copy(const uint8_t* p){std::array<uint8_t,108> out{};std::memcpy(out.data(),p,out.size());return out;}
SIZE_T WINAPI CountQuery(LPCVOID p,PMEMORY_BASIC_INFORMATION info,SIZE_T size){++queries;return VirtualQuery(p,info,size);}
edit::detail::CasResult CountCas(volatile LONG* p)noexcept{++cases;return edit::detail::CompareExchangeLeaf(p);}
edit::detail::ReadResult CountRead(const volatile LONG* p)noexcept{++reads;return edit::detail::ReadLeaf(p);}
void ResetCounts(){queries=cases=reads=0;}
edit::Result Counted(bool enabled,uintptr_t base,uint32_t config=0x2000){
  return edit::detail::EditUsing(enabled,base,config,CountQuery,CountCas,CountRead);
}
void NoWrite(const edit::Result& r){Check(!r.cas_attempted&&!r.cas_completed&&!r.observed_available&&!r.changed&&
  !r.readback_attempted&&!r.readback_available&&!r.MayHaveChanged(),"pre-CAS rejection metadata");}
void Preserved(const edit::Result&){Check(GetLastError()==0x13572468&&_mm_getcsr()==0x3F80,"LastError and exact MXCSR preserved");}
void Sentinel(){SetLastError(0x13572468);_mm_setcsr(0x3F80);}

// Test-only seam deliberately changes ONLY an owned page after its successful
// real query. This demonstrates why query success cannot guarantee CAS safety.
uint8_t* race_page{};bool race_protected{};
SIZE_T WINAPI RaceQuery(LPCVOID p,PMEMORY_BASIC_INFORMATION info,SIZE_T size){
  ++queries;const auto n=VirtualQuery(p,info,size);DWORD old{};
  race_protected=VirtualProtect(race_page,4096,PAGE_NOACCESS,&old)!=FALSE;return n;
}
SIZE_T WINAPI ChangeWordAfterQuery(LPCVOID p,PMEMORY_BASIC_INFORMATION info,SIZE_T size){
  ++queries;const auto n=VirtualQuery(p,info,size);
  InterlockedExchange(reinterpret_cast<volatile LONG*>(race_page+84),LONG(edit::detail::Swap(0x40000000)));
  return n;
}
edit::detail::CasResult CasThenProtect(volatile LONG* p)noexcept{
  ++cases;auto result=edit::detail::CompareExchangeLeaf(p);DWORD old{};
  race_protected=VirtualProtect(race_page,4096,PAGE_NOACCESS,&old)!=FALSE;return result;
}
edit::detail::ReadResult RacingRead(const volatile LONG* p)noexcept{
  ++reads;InterlockedExchange(const_cast<volatile LONG*>(p),LONG(edit::detail::Swap(0x40000000)));
  return edit::detail::ReadLeaf(p);
}
SIZE_T WINAPI ShortQuery(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T){++queries;SetLastError(ERROR_INVALID_PARAMETER);return 0;}
SIZE_T WINAPI ShortRegionQuery(LPCVOID p,PMEMORY_BASIC_INFORMATION info,SIZE_T size){
  ++queries;const auto n=VirtualQuery(p,info,size);info->BaseAddress=const_cast<void*>(p);info->RegionSize=107;return n;
}
SIZE_T WINAPI OverflowRegionQuery(LPCVOID p,PMEMORY_BASIC_INFORMATION info,SIZE_T size){
  ++queries;const auto n=VirtualQuery(p,info,size);info->RegionSize=UINTPTR_MAX;return n;
}
edit::detail::CasResult SideEffectCas(volatile LONG* p)noexcept{
  ++cases;SetLastError(0x87654321);_mm_setcsr(0x5F80);return edit::detail::CompareExchangeLeaf(p);
}

void TestDisabledAndBounds(){Pages p;Seed(p.data);const auto before=Copy(p.data);
  ResetCounts();Sentinel();auto r=Counted(false,p.Base());Preserved(r);
  Check(r.outcome==edit::Outcome::Disabled&&queries==0&&cases==0&&reads==0,"disabled has no query or memory primitive");NoWrite(r);Check(Copy(p.data)==before,"disabled identity");
  ResetCounts();r=Counted(false,UINTPTR_MAX,UINT32_MAX);Check(r.outcome==edit::Outcome::Disabled&&queries==0,"disabled does not translate invalid pointer");
  const std::array<std::pair<uintptr_t,uint32_t>,8> bad{{
    {0,0x2000},{p.Base(),0},{p.Base(),0x2001},{p.Base()+1,0x2000},
    {UINTPTR_MAX,0x2000},{p.Base(),0xFFFFFFFC},{p.Base(),0xDFFFFFC0},
    {UINTPTR_MAX-0x2000,0x2000}}};
  for(const auto& [base,config]:bad){ResetCounts();Sentinel();r=Counted(true,base,config);Preserved(r);
    Check(r.outcome==edit::Outcome::RejectedAddress&&queries==0&&cases==0&&reads==0,"invalid translation rejected before query");NoWrite(r);Check(Copy(p.data)==before,"invalid input leaves owned memory intact");}
}
void TestSuccessAndMismatch(){Pages p;Seed(p.data);const auto before=Copy(p.data);
  ResetCounts();Sentinel();const auto r=Counted(true,p.Base());Preserved(r);
  Check(r.outcome==edit::Outcome::ChangedVerified&&r.query_attempted&&r.region_accepted&&r.cas_attempted&&r.cas_completed&&r.changed&&r.MayHaveChanged(),"successful CAS metadata");
  Check(r.observed_available&&r.observed_guest_bits==edit::kExpectedGuestBits&&r.readback_available&&r.readback_guest_bits==edit::kReplacementGuestBits,"BE prior and readback metadata");
  Check(!r.exception_code&&queries==1&&cases==1&&reads==1,"one query/CAS/readback");
  auto expected=before;expected[84]=0x42;expected[85]=0x70;expected[86]=expected[87]=0;
  Check(Copy(p.data)==expected,"exact four bytes and all neighbor bytes");
  for(uint32_t bits:{0u,0x41F00001u,edit::kReplacementGuestBits,0x7FC00000u}){
    Seed(p.data,bits);const auto unchanged=Copy(p.data);ResetCounts();Sentinel();const auto mismatch=Counted(true,p.Base());Preserved(mismatch);
    Check(mismatch.outcome==edit::Outcome::CompareMismatch&&mismatch.cas_attempted&&mismatch.cas_completed&&mismatch.observed_available&&mismatch.observed_guest_bits==bits,"mismatch reports actual target word");
    Check(!mismatch.changed&&!mismatch.MayHaveChanged()&&!mismatch.readback_attempted&&queries==1&&cases==1&&reads==0,"mismatch no readback or own change");
    Check(Copy(p.data)==unchanged,"compare mismatch identity");
  }
  Seed(p.data);ResetCounts();Sentinel();const auto affected=edit::detail::EditUsing(true,p.Base(),0x2000,CountQuery,SideEffectCas,CountRead);Preserved(affected);
  Check(affected.outcome==edit::Outcome::ChangedVerified,"observation-side LastError/MXCSR changes contained");
  // Exercise the existing guest physical-address translation branch without
  // mapping a foreign pointer: its translated address is still our own page.
  Seed(p.data);constexpr uint32_t high_guest=0xE0002000;
  const auto high_base=reinterpret_cast<uintptr_t>(p.data)-uint64_t(high_guest)-0x1000;
  Check(reinterpret_cast<uintptr_t>(p.data)>uint64_t(high_guest)+0x1000,"owned high-branch translation has positive base");
  ResetCounts();const auto high=Counted(true,high_base,high_guest);
  Check(high.outcome==edit::Outcome::ChangedVerified&&cases==1&&reads==1,"physical-offset translation reaches only owned target");
}
void TestRegions(){Pages p;
  for(DWORD protection:{PAGE_READONLY,PAGE_NOACCESS,PAGE_EXECUTE_READ,PAGE_EXECUTE_READWRITE,PAGE_READWRITE|PAGE_GUARD}){
    p.Protect(PAGE_READWRITE);Seed(p.data);const auto before=Copy(p.data);p.Protect(protection);ResetCounts();Sentinel();const auto r=Counted(true,p.Base());Preserved(r);
    Check(r.outcome==edit::Outcome::RejectedRegion&&queries==1&&cases==0&&reads==0,"disallowed region rejected without touching it");NoWrite(r);
    p.Protect(PAGE_READWRITE);Check(Copy(p.data)==before,"rejected protection leaves bytes unchanged");
  }
}
void TestRegionBoundaries(){Pages p;
  // Seed before restricting the second page; the target word at offset4080 is
  // RW, but the complete108-byte object crosses into the protected region.
  Seed(p.data+3996);const auto before=Copy(p.data+3996);p.Protect(PAGE_NOACCESS,4096);
  ResetCounts();auto r=Counted(true,p.Base(3996));Check(r.outcome==edit::Outcome::RejectedRegion&&cases==0,"cross-region108 span rejected");NoWrite(r);
  p.Protect(PAGE_READWRITE,4096);Check(Copy(p.data+3996)==before,"cross-region rejection unchanged");
  Seed(p.data+3988);p.Protect(PAGE_NOACCESS,4096);ResetCounts();r=Counted(true,p.Base(3988));
  Check(r.outcome==edit::Outcome::ChangedVerified,"exact readable108-byte boundary admitted");p.Protect(PAGE_READWRITE,4096);
  const auto freed=p.Base();Check(VirtualFree(p.data,0,MEM_RELEASE)!=FALSE,"free only owned allocation");p.data=nullptr;
  ResetCounts();r=Counted(true,freed);Check(r.outcome==edit::Outcome::RejectedRegion&&cases==0,"unmapped owned address rejected");NoWrite(r);
}
void TestQueryFailures(){Pages p;Seed(p.data);const auto before=Copy(p.data);
  for(auto query:{ShortQuery,ShortRegionQuery,OverflowRegionQuery}){
    ResetCounts();Sentinel();const auto r=edit::detail::EditUsing(true,p.Base(),0x2000,query,CountCas,CountRead);Preserved(r);
    Check(r.outcome==edit::Outcome::RejectedRegion&&queries==1&&cases==0&&reads==0,"query failure or invalid bounds rejected");NoWrite(r);Check(Copy(p.data)==before,"query rejection unchanged");
  }
}
void TestRealFaultRaces(){Pages p;race_page=p.data;Seed(p.data);const auto before=Copy(p.data);
  ResetCounts();Sentinel();auto changed=edit::detail::EditUsing(true,p.Base(),0x2000,ChangeWordAfterQuery,CountCas,CountRead);Preserved(changed);
  Check(changed.outcome==edit::Outcome::CompareMismatch&&!changed.MayHaveChanged()&&changed.observed_guest_bits==0x40000000,"racing prior writer is not overwritten");
  Check(queries==1&&cases==1&&reads==0,"changed-prior race uses one compare and no readback");
  auto race_expected=before;race_expected[84]=0x40;race_expected[85]=0;race_expected[86]=race_expected[87]=0;
  Check(Copy(p.data)==race_expected,"racing writer's bytes and all neighbors preserved");Seed(p.data);
  ResetCounts();race_protected=false;Sentinel();auto r=edit::detail::EditUsing(true,p.Base(),0x2000,RaceQuery,CountCas,CountRead);Preserved(r);
  Check(race_protected,"owned page protection raced CAS");
  Check(r.outcome==edit::Outcome::CasFaultAmbiguous&&r.cas_attempted&&!r.cas_completed&&!r.observed_available&&!r.changed&&r.MayHaveChanged()&&r.exception_code==EXCEPTION_ACCESS_VIOLATION,"actual SEH CAS fault conservatively ambiguous");
  Check(queries==1&&cases==1&&reads==0&&!r.readback_attempted,"CAS fault no retry/readback");p.Protect(PAGE_READWRITE);Check(Copy(p.data)==before,"this controlled fault did not modify page");
  Seed(p.data);ResetCounts();race_protected=false;Sentinel();r=edit::detail::EditUsing(true,p.Base(),0x2000,CountQuery,CasThenProtect,CountRead);Preserved(r);
  Check(race_protected,"owned page protection raced readback");
  Check(r.outcome==edit::Outcome::ChangedReadbackFault&&r.changed&&r.MayHaveChanged()&&r.cas_completed&&r.readback_attempted&&!r.readback_available&&r.exception_code==EXCEPTION_ACCESS_VIOLATION,"actual post-CAS readback SEH fault");
  Check(queries==1&&cases==1&&reads==1,"readback fault no retry");p.Protect(PAGE_READWRITE);
  Check(p.data[84]==0x42&&p.data[85]==0x70,"successful CAS remains changed after readback fault");
  Seed(p.data);ResetCounts();Sentinel();r=edit::detail::EditUsing(true,p.Base(),0x2000,CountQuery,CountCas,RacingRead);Preserved(r);
  Check(r.outcome==edit::Outcome::ChangedReadbackMismatch&&r.changed&&r.MayHaveChanged()&&r.readback_available&&r.readback_guest_bits==0x40000000,"other writer after CAS is not reported verified");
  Check(queries==1&&cases==1&&reads==1,"readback mismatch no retry or rollback");race_page=nullptr;
}
}
int main(){const auto csr=_mm_getcsr();const auto error=GetLastError();
  try{TestDisabledAndBounds();TestSuccessAndMismatch();TestRegions();TestRegionBoundaries();TestQueryFailures();TestRealFaultRaces();
    _mm_setcsr(csr);SetLastError(error);std::cout<<"fixed60 word edit: "<<checks<<" checks PASS; guest-free only; no runtime admission\n";return 0;
  }catch(const std::exception& e){_mm_setcsr(csr);SetLastError(error);std::cerr<<"fixed60 word edit failed: "<<e.what()<<'\n';return 1;}
}
