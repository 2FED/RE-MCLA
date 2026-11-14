#include "../src/mcla_ppc_fault_json.h"
#include <iostream>
using namespace mcla::ppc_fault;
int main() {
  unsigned checks=0, failures=0;
  auto check=[&](bool ok){++checks;if(!ok)++failures;};
  PpcFaultRecord record{};
  check(Serialize(record).has_value());
  record.status=1; check(Serialize(record).has_value());
  record.thread_id=8; check(!Serialize(record));
  record.status=2; record.wrapper_entry.kind=2;
  record.requested_mask=record.read_mask=kAllFields;
  record.requested_bytes=record.read_bytes=kRequestedBytes;
  record.wrapper_entry.r3=UINT64_MAX;
  for(size_t i=0;i<kFieldCount;++i)record.observed_fields[i]=i+1;
  const auto complete=record;
  check(Serialize(record).has_value());
  check(Serialize(record)->find("18446744073709551615")!=std::string::npos);
  check(Serialize(record)->find("\"architectural_state_proven\":false")!=std::string::npos);
  for(size_t i=0;i<kFieldCount;++i){
    record=complete; record.status=3; record.read_mask &= ~(1u<<i);
    record.read_bytes -= i<9 ? 8 : 4;
    check(!Serialize(record)); // Failed fields cannot retain rejected bytes.
    record.observed_fields[i]=0; check(Serialize(record).has_value());
  }
  record=complete; record.schema=2;check(!Serialize(record));
  record=complete; record.reserved=1;check(!Serialize(record));
  record=complete; record.wrapper_entry.reserved=1;check(!Serialize(record));
  record=complete; record.wrapper_entry.kind=3;check(!Serialize(record));
  record=complete; record.thread_id=0;check(!Serialize(record));
  record=complete; record.status=5;check(!Serialize(record));
  record=complete; record.requested_bytes++;check(!Serialize(record));
  record=complete; record.read_bytes--;check(!Serialize(record));
  record=complete; record.read_mask|=1u<<11;check(!Serialize(record));
  record=complete; record.requested_mask--;check(!Serialize(record));
  record=complete; record.observed_fields[9]=UINT64_MAX;check(!Serialize(record));
  record=complete; record.status=3;check(!Serialize(record));
  record=complete; record.status=4;check(!Serialize(record));
  record=complete; record.status=4; record.read_mask=record.read_bytes=0;
  for(auto& v:record.observed_fields)v=0;
  check(Serialize(record).has_value());
  record.status=3;check(!Serialize(record));
  std::cout<<checks<<" checks, "<<failures<<" failures\n";
  return failures?1:0;
}
