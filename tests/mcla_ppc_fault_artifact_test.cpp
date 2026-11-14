#include "../src/mcla_ppc_fault_artifact.h"
#include <iostream>
#include <stdexcept>
using namespace mcla::ppc_fault;
struct Operations {
  int failure=0;
  bool exists=false;
  unsigned writes=0, removals=0;
  uint64_t bytes=0;
  std::optional<std::string> Serialize(const PpcFaultRecord& r) {
    if(failure==1)throw std::bad_alloc();
    return mcla::ppc_fault::Serialize(r);
  }
  bool Write(const std::string& text) {
    ++writes; exists=true;
    bytes=text.size();
    if(failure==2||failure==11)return false;
    if(failure==3)throw std::runtime_error("partial write");
    return true;
  }
  std::string Hash() {
    if(failure==4)return {};
    if(failure==5)throw std::runtime_error("hash");
    if(failure==12)return std::string(64,'Z');
    return std::string(64,'A');
  }
  std::optional<uint64_t> Size() {
    if(failure==6)return {};
    if(failure==7)throw std::runtime_error("size");
    return bytes+(failure==13?1:0);
  }
  bool Remove() noexcept {++removals;if(failure==11)return false;exists=false;return true;}
};
int main() {
  unsigned checks=0,failures=0;
  auto check=[&](bool ok){++checks;if(!ok)++failures;};
  for(int mode=0;mode<=13;++mode) {
    Operations ops;ops.failure=mode;
    PpcFaultRecord record{};record.status=1;
    if(mode==8) {record.status=4;record.thread_id=99;record.requested_mask=kAllFields;record.requested_bytes=kRequestedBytes;}
    if(mode==9)record.schema=100;
    if(mode==10)record.status=0;
    bool ordinary_package_continued=false,escaped=false;
    ArtifactResult result;
    try {result=WriteOptionalArtifact(record,42,ops);ordinary_package_continued=true;}
    catch(...) {escaped=true;}
    check(!escaped);check(ordinary_package_continued);
    if(mode==0){check(result.requested&&result.written);check(ops.exists);check(result.bytes==ops.bytes&&result.hash.size()==64);}
    else {check(!result.written);check(ops.exists==(mode==11));check(result.bytes==0&&result.hash.empty());if(!escaped)check(result.requested==(mode!=10));}
    check(result.cleanup_failed==(mode==11));
    if(mode>=8&&mode<=10)check(ops.writes==0);
  }
  std::cout<<checks<<" checks, "<<failures<<" failures\n";
  return failures?1:0;
}
