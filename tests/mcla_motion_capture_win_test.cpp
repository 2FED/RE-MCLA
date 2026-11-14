// Guest-free integration: include the frozen production module exactly once.
// Compile C++23 with MCLA_AUTOSTART_PROBE=1, MCLA_MOTION_CALIBRATION_PROBE=1,
// matching private provenance SDK headers/libs and nlohmann/json.hpp. Do not
// define SCOUT or link another autostart TU. Link mcla_launcher_fs_win.cpp.
// Usage: test.exe MODE ABSOLUTE_FRESH_ROOT [96|255]. One scenario/process; no cleanup.
// No Runtime::Setup, windows, GPU, OS input, game, or existing saves.
#include "mcla_autostart_win.cpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string_view>

#if !defined(MCLA_MOTION_CALIBRATION_PROBE) || defined(MCLA_GUEST_SOURCE_SCOUT_PROBE)
#error This test requires motion+autostart only, not scout.
#endif

namespace {
namespace a = mcla::autostart;
using Json = nlohmann::json;
using namespace std::chrono_literals;
unsigned checks{};
void Need(bool ok, const char* why) {
  ++checks;
  if (!ok) throw std::runtime_error(why);
}
uint64_t Now() {
  return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::vector<uint8_t> Read(const std::filesystem::path& p, size_t maximum) {
  const auto length = std::filesystem::file_size(p);
  Need(length <= maximum, "bounded retained artifact");
  std::ifstream f(p, std::ios::binary);
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
  Need(f.good() || (f.eof() && size_t(f.gcount()) == bytes.size()), "complete artifact read");
  return bytes;
}
uint32_t Le(const std::vector<uint8_t>& b, size_t at, unsigned n) {
  uint32_t v{};
  for (unsigned i=0;i<n;++i) v |= uint32_t(b.at(at+i)) << (8*i);
  return v;
}
struct Reader {
  std::atomic<bool> pause{false}, paused{false}, failed{false};
  std::jthread thread;
  explicit Reader(a::Driver& driver, uint32_t amplitude) : thread([this,&driver,amplitude](std::stop_token stop) {
    try {
      while (!stop.stop_requested()) {
        if (pause.load()) { paused.store(true); std::this_thread::sleep_for(2ms); continue; }
        paused.store(false);
        rex::input::X_INPUT_STATE state{};
        if (driver.GetState(0,&state) != rex::X_RESULT{0} ||
            state.gamepad.buttons != 0 || state.gamepad.left_trigger != 0 ||
            state.gamepad.thumb_lx != 0 || state.gamepad.thumb_ly != 0 ||
            state.gamepad.thumb_rx != 0 || state.gamepad.thumb_ry != 0 ||
            (state.gamepad.right_trigger != 0 && state.gamepad.right_trigger != amplitude)) {
          failed.store(true); return;
        }
        std::this_thread::sleep_for(2ms);
      }
    } catch (...) { failed.store(true); }
  }) {}
  void PauseAndWait() {
    pause.store(true);
    const auto begin=Now();
    while (!paused.load() && !failed.load() && Now()-begin<500)
      std::this_thread::sleep_for(2ms);
    Need(paused.load() && !failed.load(), "guest reader paused before blocked capture");
  }
  ~Reader() { if (thread.joinable()) { thread.request_stop(); thread.join(); } }
};

class SyntheticPresenter final : public rex::ui::Presenter {
 public:
  SyntheticPresenter(std::string mode, a::Driver& driver, Reader& reader,
                     std::stop_source& stop)
      : Presenter({}), mode_(std::move(mode)), driver_(driver), reader_(reader), stop_(stop) {}
  rex::ui::Surface::TypeFlags GetSupportedSurfaceTypes() const override { return 0; }
  bool CaptureGuestOutput(rex::ui::RawImage&) override { throw std::runtime_error("wrong capture API"); }
  unsigned calls{};
  bool release_observed_independently=false;
  rex::ui::GuestCaptureStatus CaptureGuestOutputWithProvenance(rex::ui::GuestOutputCapture& out) override {
    ++calls;
    if (calls==2) {
      if (mode_=="capture-fail") return rex::ui::GuestCaptureStatus::kFailed;
      if (mode_=="late") std::this_thread::sleep_for(1600ms);
      if (mode_=="poll-release" || mode_=="stop-block") {
        reader_.PauseAndWait();
        const auto before=driver_.PollMotion();
        Need(before.down_ack_ms.has_value() && before.published_ms.has_value(), "real RT read before block");
        Need(before.status==mcla::motion_input::Status::HoldingRightTrigger, "block starts during hold");
        if (mode_=="stop-block") {
          stop_.request_stop();
          std::this_thread::sleep_for(1000ms);
        } else std::this_thread::sleep_for(3500ms);
        const auto observed_at=Now();
        // This Poll cannot itself satisfy the assertion: it would set release
        // to observed_at, whereas a much earlier timestamp is required below.
        const auto after=driver_.PollMotion();
        Need(after.released_ms.has_value() && after.released_clock_valid,
             "independent poller retained an actual valid release");
        Need(*after.released_ms+250<observed_at, "release occurred while capture and guest reads blocked");
        Need(!after.neutral_ack_ms, "poll release does not fabricate guest neutral ACK");
        if (mode_=="poll-release") {
          Need(*after.released_ms>=*before.published_ms+3000 &&
               *after.released_ms<=*before.published_ms+3500, "bounded independent deadline poll");
          Need(after.status==mcla::motion_input::Status::WaitingNeutralAck, "independent release waiting for guest");
        } else {
          Need(after.status==mcla::motion_input::Status::Failed &&
               after.error==mcla::motion_input::Error::Cancelled, "independent cancellation while capture blocked");
        }
        release_observed_independently=true;
      }
    }
    out={}; out.image.width=1280;out.image.height=720;out.image.stride=5120;
    out.image.data.resize(size_t(1280)*720*4);
    for(size_t i=0;i<out.image.data.size();i+=4) {
      out.image.data[i]=uint8_t(calls);out.image.data[i+1]=37;
      out.image.data[i+2]=83;out.image.data[i+3]=255;
    }
    // Explicit synthetic provenance, never a real presenter/clock claim. The
    // production structural validator is still called by RunMotion.
    auto& p=out.provenance;p.presenter_epoch=1;p.acquired_sequence=calls;p.qpc_frequency=10000000;
    p.refresh_cpu_begin_qpc=p.refresh_cpu_end_qpc=p.acquire_before_qpc=p.acquire_after_qpc=
      p.copy_submit_before_qpc=p.copy_completion_observed_qpc=p.pixels_ready_qpc=100+calls;
    return rex::ui::GuestCaptureStatus::kSuccess;
  }
 protected:
  SurfacePaintConnectResult ConnectOrReconnectPaintingToSurfaceFromUIThread(
      rex::ui::Surface&,uint32_t,uint32_t,bool,bool&) override {
    throw std::runtime_error("no surface connection permitted");
  }
  void DisconnectPaintingFromSurfaceFromUIThreadImpl() override {}
  bool RefreshGuestOutputImpl(uint32_t,uint32_t,uint32_t,
      std::function<bool(GuestOutputRefreshContext&)>,bool&) override {
    throw std::runtime_error("no guest refresh permitted");
  }
  PaintResult PaintAndPresentImpl(bool) override { throw std::runtime_error("no presentation permitted"); }
 private:
  std::string mode_;a::Driver& driver_;Reader& reader_;std::stop_source& stop_;
};

void CompleteStart(a::Driver& driver) {
  Need(driver.Arm(), "real START arm");
  const auto begin=Now();bool saw_start=false;
  while(Now()-begin<7000) {
    rex::input::X_INPUT_STATE state{};
    Need(driver.GetState(0,&state)==rex::X_RESULT{0}, "real START read");
    Need(state.gamepad.right_trigger==0, "no RT before motion arm");
    saw_start |= state.gamepad.buttons==mcla::autostart_input::kStartButton;
    const auto s=driver.Poll();
    if(s.status==mcla::autostart_input::Status::Succeeded) {
      Need(saw_start && s.neutral_ack_ms.has_value() && state.gamepad.buttons==0,
           "real five-second START and separate neutral ACK");return;
    }
    Need(s.status!=mcla::autostart_input::Status::Failed, "START must not fail");
    std::this_thread::sleep_for(5ms);
  }
  throw std::runtime_error("bounded START setup timeout");
}
void Verify(const std::filesystem::path& root,const std::string& mode,
            bool returned,unsigned calls,bool independent,uint32_t amplitude) {
  const auto folder=root/L"motion-calibration";
  if(mode=="disabled" || mode=="missing-start" || mode=="invalid-trigger") {
    Need(!returned && calls==0 && !std::filesystem::exists(folder), "rejected before capture/output creation");return;
  }
  const auto bytes=Read(folder/L"result.json",65536);
  const auto j=Json::parse(bytes.begin(),bytes.end());
  Need(j.at("schema")=="mcla-motion-calibration-v2" &&
       j.at("token")=="19da1e9c3517474b92abe8a11a7af5f0", "receipt schema/token");
  Need(j.at("right_trigger").is_number_unsigned() && j.at("right_trigger")==amplitude &&
       j.at("input_policy")=="exclusive_internal_rt"+std::to_string(amplitude)+"_3000ms_after_start_neutral",
       "selected amplitude and policy retained exactly");
  for(const auto* key:{"os_input","desktop_capture","scene_proven","movement_proven",
      "physical_speed_proven","fps_proven","physical_hold_duration_proven","cleanup_proven"})
    Need(j.at(key).is_boolean() && !j.at(key).get<bool>(), "proof flags remain false");
  Need(j.at("capture_retry_count")==0, "no capture retry");
  const uint64_t publication=j.at("published_ms").get<uint64_t>();
  Need(j.at("release_deadline_ms")==publication+3000 &&
       j.at("neutral_ack_deadline_ms")==publication+8000, "actual publication anchored deadlines");
  const bool success=mode=="success";
  Need(returned==success && j.at("status")==
    (success?"MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING":"FAILED"), "terminal outcome");
  if(success) {
    Need(calls==6 && j.at("frames").size()==6 && j.at("failure")=="", "six exact successful captures");
    Need(!j.at("down_ack_ms").is_null() && !j.at("neutral_ack_ms").is_null(), "real guest ACKs retained");
  } else if(mode=="capture-fail") Need(calls==2 && j.at("failure")=="MOTION_CAPTURE_FAILED", "deliberate capture failure");
  else if(mode=="late" || mode=="poll-release") Need(calls==2 && j.at("failure")=="MOTION_SLOT_LATE", "next fixed slot fails, no catch-up search");
  else Need(calls==2 && j.at("failure")=="MOTION_STOP_OR_POLLER_FAILED", "blocked capture cancellation retained");
  if(mode=="poll-release" || mode=="stop-block") Need(independent, "independent poll/cancel assertion actually ran");
  std::set<std::wstring> expected{L"result.json"};unsigned saved{};
  for(const auto& frame:j.at("frames")) {
    if(!frame.at("saved").get<bool>()) continue;
    const auto slot=frame.at("slot").get<unsigned>();
    const std::wstring name=slot?L"motion-"+std::to_wstring(slot)+L".bmp":L"before.bmp";
    expected.insert(name);++saved;const auto bmp=Read(folder/name,4*1024*1024);
    Need(bmp.size()==54+size_t(1280)*720*3 && bmp[0]=='B' && bmp[1]=='M' &&
         Le(bmp,18,4)==1280 && Le(bmp,22,4)==720 && Le(bmp,28,2)==24, "retained BMP header/size");
    Need(bmp[54]==83 && bmp[55]==37 && bmp[56]==uint8_t(slot+1), "actual synthetic RGB to BMP BGR");
    Need(frame.at("provenance").at("acquired_sequence")==slot+1, "captured provenance identity");
  }
  Need(saved==(success?6u:(mode=="late" || mode=="poll-release"?2u:1u)), "no failed capture invented pixels");
  std::set<std::wstring> actual;
  for(const auto& item:std::filesystem::directory_iterator(folder)) {
    Need(item.is_regular_file(), "only owned output files");actual.insert(item.path().filename().wstring());
  }
  Need(actual==expected, "exact retained file set");
}
} // namespace

int wmain(int argc,wchar_t** argv) {
  // Parent must also enforce a process timeout; this watchdog never controls a
  // game process. On a hang it preserves partial owned files and exits failure.
  std::jthread watchdog([](std::stop_token stop) {
    const auto begin=Now();
    while(!stop.stop_requested() && Now()-begin<25000) std::this_thread::sleep_for(20ms);
    if(!stop.stop_requested()) std::_Exit(4);
  });
  int code=1;
  try {
    Need(argc==3 || argc==4,"MODE ABSOLUTE_FRESH_ROOT [96|255] required");
    uint32_t amplitude=96;
    if(argc==4) {
      const std::wstring_view requested=argv[3];
      Need(requested==L"96" || requested==L"255", "exact test amplitude only");
      amplitude=requested==L"255"?255u:96u;
    }
    const std::wstring wide=argv[1];const std::string mode(wide.begin(),wide.end());
    Need(std::set<std::string>{"success","capture-fail","late","poll-release","stop-block","disabled","missing-start","invalid-trigger"}.contains(mode),"known mode");
    const std::filesystem::path root=argv[2];
    Need(root.is_absolute() && root==root.lexically_normal() &&
         !root.filename().empty() && !std::filesystem::exists(root),"fresh absolute canonical test root");
    auto parent=mcla::launcher::fs::OpenDirectory(root.parent_path().wstring());
    Need(parent.has_value(),"existing safe parent only");
    Need(std::filesystem::create_directory(root),"exclusive fresh test directory");
    Need(!a::Enabled() && !a::MotionEnabled(),"compiled defaults remain OFF");
    Need(REXCVAR_GET(mcla_motion_calibration_trigger)==96,"compiled default amplitude remains 96");
    REXCVAR_SET(mcla_autostart_probe,true);
    REXCVAR_SET(mcla_autostart_token,"19da1e9c3517474b92abe8a11a7af5f0");
    if(mode!="disabled")REXCVAR_SET(mcla_motion_calibration_probe,true);
    if(mode=="invalid-trigger") amplitude=352;
    REXCVAR_SET(mcla_motion_calibration_trigger,amplitude);
    a::Driver driver;Need(driver.Setup()==rex::X_STATUS{0},"actual driver setup");
    if(mode!="disabled" && mode!="missing-start" && mode!="invalid-trigger")CompleteStart(driver);
    Reader reader(driver,amplitude);std::stop_source stop;SyntheticPresenter presenter(mode,driver,reader,stop);
    const bool result=a::RunMotion(stop.get_token(),presenter,driver,root);
    reader.thread.request_stop();reader.thread.join();
    Need(!reader.failed.load(),"guest responses only authorized channels");
    rex::input::X_INPUT_STATE state{};
    Need(driver.GetState(0,&state)==rex::X_RESULT{0} && state.gamepad.right_trigger==0,
         "terminal response neutral even after failure");
    Verify(root,mode,result,presenter.calls,presenter.release_observed_independently,amplitude);
    std::cout<<"motion capture "<<mode<<": "<<checks<<" checks, 0 failures; SYNTHETIC_ONLY rt="<<amplitude<<"\n";code=0;
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; }
  watchdog.request_stop();watchdog.join();return code;
}
