// Synthetic exception report through the real IPC/helper. No game/guest assets.
#include "../src/mcla_native_crash_win.cpp"
#include "../src/mcla_ppc_fault_context_win.cpp"
#include <iostream>

int wmain(int argc, wchar_t** argv) {
  using namespace mcla::diagnostics::native;
  namespace f=mcla::ppc_fault;
  if(argc!=3)return 2;
  const std::filesystem::path root(argv[1]);
  const std::wstring mode(argv[2]);
  if(!root.is_absolute() || !std::filesystem::is_directory(root) ||
      !std::filesystem::is_empty(root))return 2;
  if(mode!=L"active" && mode!=L"absent" && mode!=L"disabled" && mode!=L"mismatch")return 2;
  std::filesystem::create_directory(root/L"user");
  std::filesystem::create_directory(root/L"diagnostics");
  const auto journal=root/L"journal.txt";
  HANDLE file=CreateFileW(journal.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return 3;
  const char text[]="Synthetic crash protocol fixture. No gameplay data.\n";
  DWORD written{};const bool journal_ok=WriteFile(file,text,sizeof(text)-1,&written,nullptr)!=FALSE;
  CloseHandle(file);if(!journal_ok)return 3;
  if(InstallPpcFaultProvider(f::Capture))return 4;
  std::string error;
  if(!Start(root/L"diagnostics",root/L"user",journal,"synthetic",false,error)){
    std::cerr<<error<<'\n';return 5;
  }
  if(mode==L"mismatch"){
    const bool live=WriteLiveMiniDump(root/L"must-not-exist.dmp",std::chrono::seconds(5),error);
    DWORD code=0;const auto wait=WaitForSingleObject(state.helper_process,5000);
    const bool exited=wait==WAIT_OBJECT_0 && GetExitCodeProcess(state.helper_process,&code);
    Stop();
    const bool ok=!live && exited && code==4 && !std::filesystem::exists(root/L"must-not-exist.dmp");
    std::cout<<(ok?"MISMATCH_REJECTED":"MISMATCH_FAILED")<<'\n';return ok?0:6;
  }
  if(mode!=L"disabled"){
    if(!InstallPpcFaultProvider(f::Capture)){Stop();return 7;}
    if(InstallPpcFaultProvider(f::Capture)){Stop();return 8;}
    f::Enable();
  }
  auto report=[] {
    CONTEXT context{}; RtlCaptureContext(&context);
    EXCEPTION_RECORD record{}; record.ExceptionCode=0xE0434D44;
    record.ExceptionFlags=EXCEPTION_NONCONTINUABLE;
    record.ExceptionAddress=reinterpret_cast<void*>(context.Rip);
    EXCEPTION_POINTERS pointers{&record,&context};
    RequestCrash(&pointers);
  };
  PPCContext ctx{};ctx.lr=0x821315A4;ctx.r3.u64=0x200;ctx.r4.u64=0x300;ctx.r5.u64=32;
  if(mode==L"active"){
    f::Scope scope(2,ctx);
    ctx.r3.u64=0;ctx.r26.u64=0x300;ctx.r27.u64=32;ctx.r28.u64=7;ctx.r31.u64=0x200;
    ctx.guest_pc=0x827607E8;ctx.current_function=0x827600A0;
    report();
  }else report();
  const auto wait=WaitForSingleObject(state.helper_process,10000);
  DWORD code=0;const bool exited=wait==WAIT_OBJECT_0 && GetExitCodeProcess(state.helper_process,&code);
  const auto result=InterlockedCompareExchange(&state.shared->crash_result,0,0);
  if(!exited){
    SetEvent(state.stop);
    if(WaitForSingleObject(state.helper_process,3000)!=WAIT_OBJECT_0){
      TerminateProcess(state.helper_process,90); // Exact fixture-created helper handle only.
      WaitForSingleObject(state.helper_process,5000);
    }
  }
  Stop();
  const bool ok=exited && code==0 && result==1;
  std::cout<<(ok?"SYNTHETIC_REPORT_COMPLETE":"SYNTHETIC_REPORT_FAILED")<<'\n';
  return ok?0:9;
}
