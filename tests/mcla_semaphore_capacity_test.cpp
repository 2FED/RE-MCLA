#include "../src/mcla_semaphore_observer_core.h"
#include "../src/mcla_semaphore_prefix_snapshot.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
  namespace sem = mcla::semaphore_observer;
  auto journal = std::make_unique<sem::Journal>(sem::Window{1, 1000000, 10000}, true);
  if (journal->create(1, 2, 0, 1).disposition != sem::Disposition::kRecorded) return 2;
  // Minimal owner5 failure: operation 4097 loses its ticket after retained 4096.
  for (size_t i = 0; i < 4097; ++i) {
    auto op = journal->begin_wait(1, 1, {}, 10 + i * 2, sem::TimeoutKind::kZero);
    if (op.disposition != sem::Disposition::kRecorded || !op.ticket.valid()) {
      std::printf("FAIL operation %zu has no retained ticket; capacity=%zu\n", i + 1, sem::kOperationCapacity);
      return 1;
    }
    journal->end_wait(op.ticket, 11 + i * 2, sem::WaitResult::kTimeout);
  }
  if (journal->counts().overflow_operations != 0 || journal->counts().completed_operations != 4097) return 3;
  std::puts("PASS operation4097 retains exact ticket and completion");
  namespace snap = mcla::semaphore_prefix_snapshot;
  journal->stop_admissions(10000);
  constexpr auto token = "44444444444444444444444444444444";
  const snap::EpochCoverage minimal_epoch{1, 1, 2, snap::Coverage::kCompleteThroughFreeze,
                                         snap::Coverage::kCompleteThroughFreeze};
  const snap::AdmissionCoverage minimal_coverage{token, 1, 10000, 10000,
      snap::Coverage::kCompleteThroughFreeze, false, false, {&minimal_epoch, 1}};
  auto minimal = snap::Snapshot::Capture(*journal, token, minimal_coverage);
  if (!minimal.snapshot) return 9;
  auto minimal_json = minimal.snapshot->PrefixJson();
  if (minimal_json.error != snap::Error::kNone) return 10;
  // Maximum-width retained fields exercise both serializers at actual capacity.
  auto full = std::make_unique<sem::Journal>(sem::Window{1, sem::kMaxQpc, 10000000}, true);
  full->create(UINT64_MAX, 2, 0, sem::kMaxCount);
  const uint64_t start = sem::kMaxQpc - 1000000;
  for (size_t i = 0; i < sem::kOperationCapacity; ++i) {
    auto op = full->begin_release(UINT64_MAX, UINT32_MAX, UINT32_MAX, start + i * 4,
        1000000000, {sem::ContextKind::kUpdate, {UINT64_MAX, UINT64_MAX}});
    if (op.disposition != sem::Disposition::kRecorded || !op.ticket.valid()) return 4;
    full->end_release(op.ticket, start + i * 4 + 1, true, 1000000000);
  }
  const uint64_t cut = start + sem::kOperationCapacity * 4 + 10;
  full->stop_admissions(cut);
  const snap::EpochCoverage epoch{1, UINT64_MAX, 2, snap::Coverage::kCompleteThroughFreeze,
                                 snap::Coverage::kCompleteThroughFreeze};
  const snap::AdmissionCoverage coverage{token, 1, 10000000, cut, snap::Coverage::kCompleteThroughFreeze,
                                        false, false, {&epoch, 1}};
  auto captured = snap::Snapshot::Capture(*full, token, coverage);
  if (!captured.snapshot) return 5;
  auto prefix = captured.snapshot->PrefixJson(), raw = captured.snapshot->RawCoreJson();
  if (prefix.error != snap::Error::kNone || raw.error != snap::Error::kNone ||
      prefix.json.size() > snap::kMaxJsonBytes || raw.json.size() > snap::kMaxJsonBytes) return 6;
  std::printf("FULL_CAPACITY %zu prefix_bytes=%zu raw_bytes=%zu journal_bytes=%zu snapshot_bytes=%zu\n",
      sem::kOperationCapacity, prefix.json.size(), raw.json.size(), sizeof(sem::Journal), sizeof(snap::Snapshot));
  if (argc == 2) {
    std::ofstream p(std::string(argv[1]) + "/prefix.json", std::ios::binary);
    std::ofstream r(std::string(argv[1]) + "/raw.json", std::ios::binary);
    std::ofstream m(std::string(argv[1]) + "/minimal-prefix.json", std::ios::binary);
    p << prefix.json; r << raw.json;
    m << minimal_json.json;
    if (!p || !r || !m) return 7;
  } else if (argc != 1) return 8;
  return 0;
}
