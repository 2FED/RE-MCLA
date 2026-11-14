// Actual Invoke + lexical fault context; synthetic originals, no game/windows.
#include "../src/mcla_ppc_fault_context_win.cpp"
#include "../src/mcla_update_timing_win.cpp"
#include <cstring>
#include <iostream>
namespace {
namespace f=mcla::ppc_fault;
namespace d=mcla::update_timing::detail;
unsigned checks{},failures{},calls{},provider_calls{};
bool provider_ok{},provider_used{},throw_original{},nested{};
f::PpcFaultRecord observed{};
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
uint64_t Clock(){SetLastError(800);return 100;}
void Original(PPCContext& ctx,uint8_t* base){
  ++calls; Check(GetLastError()==77,"original entry LastError");
  f::Capture(observed);
  if(nested){
    nested=false; auto inner=ctx; inner.r3.u64=123456; SetLastError(77);
    d::Invoke(nullptr,d::Kind::Router,Original,inner,base);
    Check(observed.wrapper_entry.r3==123456,"nested top context");
    f::Capture(observed); Check(observed.wrapper_entry.r3==ctx.r3.u64,"outer context restored");
  }
  ctx.r5.u64++; _mm_setcsr(0x5F80); SetLastError(1234);
  if(throw_original)throw 42;
}
}
namespace mcla::phase_census { bool Enabled(){return true;} ActiveUpdate ReadActiveUpdate(const PPCContext&,const uint8_t*) noexcept{return{};} }
namespace mcla::guest_source_scout { bool Enabled(){return true;} }
namespace mcla::diagnostics::native {
bool InstallPpcFaultProvider(PpcFaultProvider provider) noexcept {
  ++provider_calls;
  if(!provider_ok || provider_used || provider!=f::Capture)return false;
  provider_used=true;return true;
}
}
REX_EXTERN(__imp__sub_821BDA90){Original(ctx,base);}
REX_EXTERN(__imp__sub_821BD910){Original(ctx,base);}
REX_EXTERN(__imp__sub_822C52C8){Original(ctx,base);}
int main(){
  const auto original_fp=_mm_getcsr();
  PPCContext ctx{};ctx.lr=0x821315A4;ctx.r3.u64=0x123456789;
  SetLastError(77);d::Invoke(nullptr,d::Kind::Router,Original,ctx,nullptr);
  Check(observed.status==0,"diagnostic defaults OFF");
  d::InstallFaultContextIfEnabled();Check(provider_calls==0,"false option does not install");
  Check(rex::cvar::SetFlagByName("mcla_update_fault_context_probe","true"),"fault flag exists");
  bool rejected=false;try{d::InstallFaultContextIfEnabled();}catch(...){rejected=true;}
  Check(rejected && provider_calls==0,"fault ON timing OFF rejected");
  Check(rex::cvar::SetFlagByName("mcla_update_timing_probe","true"),"timing flag exists");
  rejected=false;try{d::InstallFaultContextIfEnabled();}catch(...){rejected=true;}
  f::Capture(observed);Check(rejected && observed.status==0,"missing crash client stays disabled");
  provider_ok=true;d::InstallFaultContextIfEnabled();
  for(unsigned mode=0;mode<3;++mode){
    for(bool throws:{false,true}){
      auto state=std::make_unique<d::State>();state->clock=Clock;state->data.begin=1000;state->data.end=2000;
      if(mode==2)state->stopped.store(true);
      ctx={};ctx.lr=0x821315A4;ctx.r3.u64=0x123456789;ctx.r5.u64=7;
      auto expected=ctx;expected.r5.u64++;
      const auto before_calls=calls;throw_original=throws;_mm_setcsr(0x1F80);SetLastError(77);
      bool caught=false;
      try{d::Invoke(mode?state.get():nullptr,d::Kind::Router,Original,ctx,nullptr);}catch(int n){caught=n==42;}
      const auto error=GetLastError();const auto fp=_mm_getcsr();
      Check(calls==before_calls+1 && caught==throws,"exact once and original exception");
      Check(error==1234 && fp==0x5F80,"legitimate original LastError FP retained");
      Check(std::memcmp(&ctx,&expected,sizeof(ctx))==0,"full original context retained");
      Check(observed.status==2 && observed.wrapper_entry.r3==expected.r3.u64 && observed.wrapper_entry.r5==7,"entry observed independent admission");
      f::Capture(observed);Check(observed.status==1,"no stale scope after return or unwind");
    }
  }
  throw_original=false;nested=true;ctx={};ctx.r3.u64=77;SetLastError(77);
  d::Invoke(nullptr,d::Kind::Router,Original,ctx,nullptr);
  f::Capture(observed);Check(observed.status==1,"nested return leaves no scope");
  rejected=false;try{d::InstallFaultContextIfEnabled();}catch(...){rejected=true;}
  Check(rejected,"provider cannot be replaced");
  _mm_setcsr(original_fp);
  std::cout<<checks<<" checks, "<<failures<<" failures\n";
  return failures?1:0;
}
