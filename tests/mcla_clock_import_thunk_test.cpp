// Pure synthetic mapped-image fixture; no Windows/SDK loader, files or native
// function execution. Vector allocation belongs to test setup, not Verify().
#include "../src/mcla_clock_import_thunk.h"
#include <algorithm>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <vector>

namespace {
namespace thunk = mcla::clock_read_witness::import_thunk;
constexpr uintptr_t kBase=0x140000000ull, kExport=0x180001234ull;
constexpr uint32_t kImage=0x4000, kPe=0x80, kOptional=kPe+24;
constexpr uint32_t kThunk=0x1000, kLookup=0x2000, kIat=0x2100;
constexpr uint32_t kDll=0x2200, kImports=0x2400, kSymbol=0x2800, kOtherSymbol=0x2B00;
unsigned checks{}, failures{};
void Check(bool ok,const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Fixture {
  std::vector<uint8_t> image=std::vector<uint8_t>(kImage);
  void U16(size_t at,uint16_t value) { for (unsigned i=0;i<2;++i) image.at(at+i)=uint8_t(value>>(i*8)); }
  void U32(size_t at,uint32_t value) { for (unsigned i=0;i<4;++i) image.at(at+i)=uint8_t(value>>(i*8)); }
  void U64(size_t at,uint64_t value) { for (unsigned i=0;i<8;++i) image.at(at+i)=uint8_t(value>>(i*8)); }
  void Text(size_t at,std::string_view value) {
    for (size_t i=0;i<value.size();++i) image.at(at+i)=uint8_t(value[i]);
    image.at(at+value.size())=0;
  }
  void Directory(unsigned index,uint32_t rva,uint32_t size) {
    U32(kOptional+112+index*8,rva); U32(kOptional+116+index*8,size);
  }
  void Jump(uint32_t location,uint32_t slot) {
    image.at(location)=0xFF; image.at(location+1)=0x25;
    U32(location+2,uint32_t(int64_t(slot)-int64_t(location+6)));
  }
  void Descriptor(uint32_t offset,uint32_t lookup=kLookup,uint32_t first=kIat,uint32_t name=kDll) {
    U32(offset,lookup); U32(offset+4,0); U32(offset+8,0); U32(offset+12,name); U32(offset+16,first);
  }
  Fixture() {
    U16(0,0x5A4D); U32(0x3C,kPe); U32(kPe,0x00004550); U16(kPe+4,0x8664);
    U16(kPe+6,1); U16(kPe+20,240); U16(kOptional,0x20B);
    U32(kOptional+56,kImage); U32(kOptional+60,0x400); U32(kOptional+108,16);
    Directory(1,kImports,40); Directory(12,kIat,16);
    // One section header fits SizeOfHeaders; no raw-file layout is used.
    U32(kOptional+240+8,0x3000); U32(kOptional+240+12,0x1000);
    U32(kOptional+240+16,0x3000); U32(kOptional+240+20,0x400);
    Descriptor(kImports); Text(kDll,"rexruntime.dll"); U16(kSymbol,17);
    Text(kSymbol+2,"?QueryGuestTickCount@Clock@chrono@rex@@SA_KXZ");
    U64(kLookup,kSymbol); U64(kLookup+8,0); U64(kIat,kExport); U64(kIat+8,0);
    Jump(kThunk,kIat);
  }
  thunk::Result Verify(uintptr_t cpp=kBase+kThunk,uintptr_t exported=kExport,
                       uintptr_t base=kBase) const {
    return thunk::Verify(image.data(),image.size(),base,cpp,exported);
  }
};
template<class Change> void Reject(Change change,const char* label) {
  Fixture f; change(f); const auto result=f.Verify();
  Check(!result.accepted() && result.kind==thunk::Kind::Rejected &&
      !result.iat_rva && !result.thunk_rva,label);
}

void AcceptExact() {
  Fixture f;
  const auto forward=f.Verify();
  Check(forward.accepted() && forward.kind==thunk::Kind::ExactImportThunk &&
      forward.iat_rva==kIat && forward.thunk_rva==kThunk,"exact FF25 forward-rel32 named import thunk accepted");
  f.Jump(0x3000,kIat);
  const auto backward=f.Verify(kBase+0x3000);
  Check(backward.accepted() && backward.kind==thunk::Kind::ExactImportThunk &&
      backward.iat_rva==kIat && backward.thunk_rva==0x3000,"signed backward-rel32 thunk accepted without unsigned-wrap guess");
  const auto direct=f.Verify(kExport);
  Check(direct.accepted() && direct.kind==thunk::Kind::DirectExport &&
      direct.iat_rva==kIat && direct.thunk_rva==0,"direct export still requires exact named live IAT attestation");
  f.Text(kDll,"ReXrUnTiMe.DlL");
  Check(f.Verify().accepted(),"fixed DLL matching is ASCII case insensitive");
  const uintptr_t syntheticBase=0x440000000ull;
  Check(f.Verify(syntheticBase+kThunk,kExport,syntheticBase).accepted(),
      "synthetic virtual addresses may differ from backing vector address");
  Check(!thunk::kRuntimeAdmitted && !thunk::kSdkIdentityProven && !thunk::kCompleteClockHistoryProven &&
      !thunk::kSimulationProven && !thunk::kFpsProven,"all engineering and SDK identity proof flags remain false");
}

void RejectIdentityAndSlots() {
  Reject([](Fixture& f){f.U64(kIat,kExport+1);},"wrong live IAT target rejected");
  Fixture direct; direct.U64(kIat,kExport+1);
  Check(!direct.Verify(kExport).accepted(),"direct address equality cannot bypass IAT mismatch");
  Reject([](Fixture& f){f.Text(kDll,"rexruntime2.dll");},"other DLL rejected");
  Reject([](Fixture& f){f.Text(kDll,"rexruntime.dll.evil");},"DLL prefix match is not exact identity");
  Reject([](Fixture& f){f.Text(kSymbol+2,"?QueryGuestTickCount@Clock@chrono@rex@@SA_KXY");},"wrong exact mangled symbol rejected");
  Reject([](Fixture& f){f.Text(kSymbol+2,"?QueryGuestTickCount@Clock@chrono@rex@@SA_KXZx");},"symbol prefix with trailing bytes rejected");
  Reject([](Fixture& f){f.Text(kSymbol+2,"?queryGuestTickCount@Clock@chrono@rex@@SA_KXZ");},"symbol matching is case sensitive");
  Reject([](Fixture& f){f.U32(kImports,0);},"OriginalFirstThunk required with no IAT-name guess");
  Reject([](Fixture& f){f.U64(kLookup,0x8000000000000001ull);},"ordinal-only import cannot attest clock name");
  Reject([](Fixture& f){f.U64(kLookup,0x100000000ull+kSymbol);},"64bit lookup value cannot truncate to named RVA");
  Reject([](Fixture& f){f.U64(kIat+8,kExport);},"live IAT terminator must be zero");
  Reject([](Fixture& f){f.U64(kIat,0);},"null named IAT target rejected");
  Reject([](Fixture& f){f.U64(kLookup,0);f.U64(kIat,0);},"descriptor with no exact clock named slot rejected");
  Reject([](Fixture& f){f.Directory(1,kImports,60);f.Descriptor(kImports+20);},"duplicate relevant DLL descriptors rejected");
  Reject([](Fixture& f){
    f.Directory(12,kIat,24);f.U64(kLookup+8,kSymbol);f.U64(kLookup+16,0);
    f.U64(kIat+8,kExport);f.U64(kIat+16,0);
  },"duplicate exact named slots rejected");
  Reject([](Fixture& f){
    f.Directory(12,kIat,24);f.U64(kLookup+8,0x8000000000000002ull);f.U64(kLookup+16,0);
    f.U64(kIat+8,kExport);f.U64(kIat+16,0);
  },"ordinal imports in relevant SDK descriptor conservatively rejected");
  Reject([](Fixture& f){f.U64(0x2500,kExport);f.Jump(kThunk,0x2500);},"arbitrary same-target pointer outside attested IAT rejected");
  Reject([](Fixture& f){
    f.Directory(12,kIat,24);f.Text(kOtherSymbol+2,"OtherFunction");f.U64(kLookup+8,kOtherSymbol);
    f.U64(kLookup+16,0);f.U64(kIat+8,kExport);f.U64(kIat+16,0);f.Jump(kThunk,kIat+8);
  },"neighboring live slot with same target cannot replace exact clock slot");
  Fixture unrelated;
  unrelated.Directory(1,kImports,60); unrelated.Descriptor(kImports+20,kLookup,kIat,kDll+64);
  unrelated.Text(kDll+64,"other.dll");
  Check(unrelated.Verify().accepted(),"unrelated DLL descriptor does not fabricate a second target descriptor");
}

void RejectThunkShapesAndArithmetic() {
  Reject([](Fixture& f){f.image[kThunk]=0xE9;},"relative jump chain not followed");
  Reject([](Fixture& f){f.image[kThunk+1]=0x15;},"FF15 call is not exact FF25 jump");
  Reject([](Fixture& f){f.image[kThunk]=0x66;},"prefix variant is not accepted as six-byte thunk");
  Reject([](Fixture& f){f.U64(kIat,kBase+0x3000);f.Jump(0x3000,kIat);},"IAT jump chain not followed");
  Reject([](Fixture& f){f.U32(kThunk+2,0x7FFFFFFF);},"far rel32 target cannot nearest-match slot");
  Reject([](Fixture& f){f.U32(kThunk+2,0x80000000);},"minimum signed rel32 cannot wrap to slot");
  Fixture f;
  Check(!f.Verify(kBase-1).accepted() && !f.Verify(kBase+kImage).accepted(),"C++ thunk address must lie inside declared host image");
  f.image[kImage-5]=0xFF; f.image[kImage-4]=0x25;
  Check(!f.Verify(kBase+kImage-5).accepted(),"truncated six-byte thunk rejects before reading rel32");
  const auto max=std::numeric_limits<uintptr_t>::max();
  Check(!f.Verify(max-1,kExport,max-0x1000).accepted(),"virtual image-end address overflow rejected");
  Fixture positive;positive.U32(kThunk+2,0x7FFFFFFF);
  const uintptr_t nearTop=max-0x5000;
  Check(!positive.Verify(nearTop+kThunk,kExport,nearTop).accepted(),"positive rel32 overflow rejected without addition wrap");
  Fixture negative;negative.U32(kThunk+2,0x80000000);
  Check(!negative.Verify(0x2000,kExport,0x1000).accepted(),"negative rel32 address underflow rejected without subtraction wrap");
  Check(!f.Verify(0).accepted() && !f.Verify(kBase+kThunk,0).accepted() &&
      !f.Verify(kThunk,kExport,0).accepted(),"zero addresses fail closed");
  Check(!thunk::Verify(nullptr,kImage,kBase,kBase+kThunk,kExport).accepted(),"null readable span rejected");
}

void RejectHeadersAndDirectories() {
  Reject([](Fixture& f){f.U16(0,0);},"DOS signature required");
  Reject([](Fixture& f){f.U32(0x3C,63);},"PE header cannot overlap DOS header");
  Reject([](Fixture& f){f.U32(0x3C,std::numeric_limits<uint32_t>::max());},"negative or unbounded e_lfanew rejected");
  Reject([](Fixture& f){f.U32(kPe,0);},"PE signature required");
  Reject([](Fixture& f){f.U16(kPe+4,0x14C);},"nonAMD64 image rejected");
  Reject([](Fixture& f){f.U16(kOptional,0x10B);},"PE32 instead of PE32plus rejected");
  Reject([](Fixture& f){f.U16(kPe+6,0);},"zero section table rejected");
  Reject([](Fixture& f){f.U16(kPe+6,97);},"section count over narrow PE loader bound rejected");
  Reject([](Fixture& f){f.U16(kPe+20,111);},"short optional header rejected");
  Reject([](Fixture& f){f.U16(kPe+20,232);},"directory array outside optional header rejected");
  Reject([](Fixture& f){f.U32(kOptional+108,1);},"missing required import and IAT directories rejected");
  Reject([](Fixture& f){f.U32(kOptional+108,17);},"unsupported extra directories rejected rather than unbounded scan");
  Reject([](Fixture& f){f.U32(kOptional+56,kImage+1);},"declared image larger than readable span rejected");
  Reject([](Fixture& f){f.U32(kOptional+56,0);},"zero declared image rejected");
  Reject([](Fixture& f){f.U32(kOptional+60,kImage+1);},"headers cannot exceed image");
  Reject([](Fixture& f){f.U32(kOptional+60,kOptional+240+39);},"entire section header table must fit SizeOfHeaders");
  Reject([](Fixture& f){f.Directory(5,kImage-1,2);},"all nonzero RVA directory ranges must fit image");
  Reject([](Fixture& f){f.Directory(5,0,8);},"half-empty directory rejected");
  Reject([](Fixture& f){f.Directory(4,0x100,8);},"nonzero FILE-offset Security directory explicitly unsupported");
  Reject([](Fixture& f){f.Directory(1,kImage-1,40);},"import directory cannot cross image end");
  Reject([](Fixture& f){f.Directory(1,kImports,20);},"descriptor terminator must fit declared import directory");
  Reject([](Fixture& f){f.Directory(12,kIat,8);},"IAT terminator must fit declared directory");
  Reject([](Fixture& f){f.Directory(12,kIat+1,16);},"misaligned IAT directory rejected");
  Reject([](Fixture& f){f.Directory(12,kIat,15);},"partial IAT entry range rejected");
  Reject([](Fixture& f){f.U32(kImports+16,kIat+1);},"misaligned FirstThunk rejected");
  Reject([](Fixture& f){f.U32(kImports,kLookup+1);},"misaligned OriginalFirstThunk rejected");
  Fixture f;
  for (const size_t readable : {size_t{0},size_t{1},size_t{63},size_t{0x80},size_t{0x100},size_t{kImage-1}})
    Check(!thunk::Verify(f.image.data(),readable,kBase,kBase+kThunk,kExport).accepted(),
        "short readable spans reject before out-of-range access");
}

void RejectTruncatedTablesAndNames() {
  Reject([](Fixture& f){f.U32(kImports+12,kImage);},"DLL name RVA must be readable");
  Reject([](Fixture& f){std::fill_n(f.image.begin()+kDll,256,uint8_t('x'));},"DLL name must terminate within256bytes");
  Reject([](Fixture& f){std::fill_n(f.image.begin()+kSymbol+2,512,uint8_t('x'));},"import symbol must terminate within512bytes");
  Reject([](Fixture& f){f.U64(kLookup,kImage-1);},"hint and symbol bytes must be bounded");
  Reject([](Fixture& f){f.U32(kImports,kImage-8);f.U64(kImage-8,kSymbol);},"ILT must terminate before declared image end");
  Reject([](Fixture& f){f.U32(kImports+20,1);},"import descriptor sentinel must be allzero");
  Reject([](Fixture& f){f.U32(kImports+12,0);},"partial nonzero descriptor cannot omit DLL name");
}
void FixedScanBounds() {
  Fixture imports;
  constexpr uint32_t directory=0x10000;
  imports.image.resize(0x40000); imports.U32(kOptional+56,uint32_t(imports.image.size()));
  imports.Directory(1,directory,(thunk::detail::kMaxDescriptors+1)*20);
  imports.Text(kDll+64,"other.dll"); imports.Descriptor(directory);
  for (uint32_t i=1;i<thunk::detail::kMaxDescriptors;++i)
    imports.Descriptor(directory+i*20,kLookup,kIat,kDll+64);
  // The zero sentinel is one descriptor PAST the explicit resource bound.
  Check(!imports.Verify().accepted(),"descriptor scan has a fixed cap even when a later sentinel is readable");

  Fixture slots;
  constexpr uint32_t lookup=0x10000, iat=0x100000;
  slots.image.resize(0x200000); slots.U32(kOptional+56,uint32_t(slots.image.size()));
  slots.Descriptor(kImports,lookup,iat); slots.Text(kOtherSymbol+2,"OtherFunction");
  slots.Directory(12,iat,(thunk::detail::kMaxThunks+1)*8); slots.Jump(kThunk,iat);
  for (uint32_t i=0;i<thunk::detail::kMaxThunks;++i) {
    slots.U64(lookup+i*8,i ? kOtherSymbol : kSymbol); slots.U64(iat+i*8,i ? kExport+1 : kExport);
  }
  // One exact clock match, all live entries valid, but terminator is past cap.
  Check(!slots.Verify().accepted(),"SDK ILT scan has a fixed cap without reading for a later favorable terminator");
}
}  // namespace

int main() {
  AcceptExact(); RejectIdentityAndSlots(); RejectThunkShapesAndArithmetic();
  RejectHeadersAndDirectories(); RejectTruncatedTablesAndNames(); FixedScanBounds();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
