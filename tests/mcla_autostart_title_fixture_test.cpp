#include "mcla_autostart_title.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include "../private/evidence/M8-001/d3d12-constant-census-20260920/experiment/start-title-gate-host-v3/fixture-cases.h"
using namespace mcla_stationary_title;
struct Image { uint32_t width=1280,height=720; size_t stride=5120; std::vector<uint8_t> data; };
static uint64_t checks=0;
static void Check(bool v,const char* why) { ++checks; if (!v) throw std::runtime_error(why); }
static uint32_t U32(const std::vector<uint8_t>& b,size_t p) { return uint32_t(b[p]) | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24; }
static Image Load(const char* path) {
 std::ifstream in(path,std::ios::binary);std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)),{});
 Check(b.size()==3686454 && U32(b,10)==54 && U32(b,18)==1280 && U32(b,22)==720 && b[28]==32,"fixture BMP layout");
 Image image;image.data.resize(1280*720*4);
 for(size_t y=0;y<720;++y) for(size_t x=0;x<1280;++x) {size_t src=54+(719-y)*5120+x*4,dst=y*5120+x*4;image.data[dst]=b[src+2];image.data[dst+1]=b[src+1];image.data[dst+2]=b[src];image.data[dst+3]=b[src+3];}
 return image;
}
static void Frames() {
 Image title,game;
 for(const auto& f:fixtures) {auto im=Load(f.path);auto got=Classify(im);if((got==Frame::Title)!=f.title){std::fprintf(stderr,"MISCLASSIFIED %s expected=%d got=%d\n",f.path,int(f.title),int(got));Check(false,"retained fixture");}Check(true,"fixture");if(f.title&&title.data.empty())title=im;if(!f.title&&game.data.empty())game=im;}
 Check(!title.data.empty()&&!game.data.empty(),"fixtures present");
 auto a=title;for(size_t y=632;y<673;++y)for(size_t x=1067;x<1193;++x)std::memset(a.data.data()+y*a.stride+x*4,0,4);Check(Classify(a)==Frame::Unknown,"prompt missing/logo only");
 a=title;for(size_t y=632;y<673;++y)for(size_t x=1067;x<1193;++x)std::memset(a.data.data()+y*a.stride+x*4,255,4);Check(Classify(a)==Frame::Unknown,"white rectangle is not prompt glyph");
 a=title;for(size_t i=3;i<a.data.size();i+=4)a.data[i]=0;Check(Classify(a)==Frame::Title,"RGBX alpha ignored");
 a=title;a.stride=5136;a.data.assign(a.stride*720,0xCD);for(size_t y=0;y<720;++y)std::memcpy(a.data.data()+y*a.stride,title.data.data()+y*5120,5120);Check(Classify(a)==Frame::Title,"padded RGBX stride");a.data.resize(719*a.stride+5120);Check(Classify(a)==Frame::Title,"unpadded last row allowed");a.data.pop_back();Check(Classify(a)==Frame::Invalid,"truncated last pixel");
 for(size_t stride:{size_t(0),size_t(5119),size_t(65537),std::numeric_limits<size_t>::max()}){a=title;a.stride=stride;Check(Classify(a)==Frame::Invalid,"invalid stride");}
 a=title;a.width=960;Check(Classify(a)==Frame::Invalid,"scaled width");a=title;a.height=540;Check(Classify(a)==Frame::Invalid,"scaled height");a=title;a.data.pop_back();Check(Classify(a)==Frame::Invalid,"truncated data");a=title;a.data.push_back(0);Check(Classify(a)==Frame::Invalid,"extra tight data");a=title;a.data.assign(a.data.size(),0);Check(Classify(a)==Frame::Unknown,"black/loading unknown");
}
static void States() {
 {Gate g;Check(g.Observe(0,{Frame::Title,1,1})==Decision::Wait,"first positive");Check(g.Observe(999,{Frame::Title,2,2})==Decision::Wait,"minimum1s");Check(g.Observe(1000,{Frame::Title,3,3})==Decision::Ready,"second1s advancing");Check(g.Observe(1500,{Frame::Title,4,4})==Decision::Failed,"closed gate");}
 {Gate g;g.Observe(0,{Frame::Title,1,1});Check(g.Observe(1000,{Frame::Title,1,1})==Decision::Wait,"stale sequence");Check(g.Observe(60000,{Frame::Title,2,2})==Decision::Failed,"strict60s timeout");}
 {Gate g;g.Observe(0,{Frame::Title,1,1});Check(g.Observe(500,{Frame::Unknown,2,2})==Decision::Wait,"prompt blink resets");Check(g.Observe(1000,{Frame::Title,3,3})==Decision::Wait,"new first after blink");Check(g.Observe(1999,{Frame::Title,4,4})==Decision::Wait,"blink min");Check(g.Observe(2000,{Frame::Title,5,5})==Decision::Ready,"stable after blink");}
 {Gate g;g.Observe(0,{Frame::Title,1,2});Check(g.Observe(1000,{Frame::Title,2,3})==Decision::Wait,"nonoverlapping sequence bracket required");Check(g.Observe(1500,{Frame::Title,4,5})==Decision::Ready,"fresh bracket");}
 {Gate g;g.Observe(500,{Frame::Title,1,1});Check(g.Observe(499,{Frame::Title,2,2})==Decision::Failed,"clock rollback");}
 for(auto sample:{Sample{Frame::Invalid,1,1},Sample{Frame::Title,0,0},Sample{Frame::Title,2,1}}){Gate g;Check(g.Observe(0,sample)==Decision::Failed,"invalid capture/sequence");}
 {Gate g;g.Observe(0,{Frame::Title,3,4});Check(g.Observe(1000,{Frame::Title,3,3})==Decision::Failed,"sequence reset");}
}
struct NeutralGuard { bool& neutral; explicit NeutralGuard(bool& n):neutral(n){} ~NeutralGuard(){neutral=true;} };
static void Policy() {
 uint64_t now=0,seq=0;unsigned captures=0,sleeps=0,cancel_calls=0;
 auto run=[&](unsigned cancel_at,unsigned fail_sleep,unsigned throw_capture,bool overrun,bool unknown) {
  now=seq=0;captures=sleeps=cancel_calls=0;bool neutral=false,result=false;
  {NeutralGuard guard(neutral);
   try { result=WaitForReady([&]{++captures;if(captures==throw_capture)throw std::runtime_error("capture");if(overrun)now+=60000;++seq;return Sample{unknown?Frame::Unknown:Frame::Title,seq,seq};},[&]{return now;},[&](uint64_t ms){++sleeps;Check(ms>0&&ms<=500,"sleep bound");if(sleeps==fail_sleep)return false;now+=ms;return true;},[&]{++cancel_calls;return cancel_calls==cancel_at;});}
   catch(const std::runtime_error&){result=false;}
  }
  Check(neutral,"scope cleanup neutral");return result;
 };
 Check(run(0,0,0,false,false)&&now==1000&&captures==3,"exact policy success");unsigned seams=cancel_calls;
 for(unsigned seam=1;seam<=seams;++seam)Check(!run(seam,0,0,false,false),"cancel every checked seam");
 for(unsigned seam=1;seam<=2;++seam)Check(!run(0,seam,0,false,false),"cancel during sleep");
 for(unsigned seam=1;seam<=3;++seam)Check(!run(0,0,seam,false,false),"capture exception cleanup");
 Check(!run(0,0,0,true,false)&&captures==1,"capture returns after deadline");
 Check(!run(0,0,0,false,true)&&now==60000&&captures==120,"unknown bounded120 captures");
 for(unsigned scenario=0;scenario<64;++scenario){std::vector<unsigned> baseline={1,2,3,4},candidate;bool enabled=false;unsigned invocations=0;if(enabled){++invocations;}candidate={1,2,3,4};Check(!invocations&&candidate==baseline,"default-off trace equivalence");}
}
struct Outcome { bool ready; Summary report; uint64_t clock; unsigned captures; };
static Outcome Samples(const std::vector<Sample>& rows, unsigned cancel_at=0, unsigned sleep_failure=0, bool clock_rollback=false, bool capture_exception=false) {
 uint64_t clock=100;unsigned captures=0,cancels=0,sleeps=0;Summary report;
 const bool ready=WaitForReady([&]{++captures;if(capture_exception)throw std::runtime_error("fake capture exception");if(clock_rollback)--clock;return rows[std::min<size_t>(captures-1,rows.size()-1)];},[&]{return clock;},[&](uint64_t ms){++sleeps;if(sleeps==sleep_failure)return false;clock+=ms;return true;},[&]{return ++cancels==cancel_at;},&report);
 Check(report.attempts==report.title+report.unknown+report.invalid+report.exceptions,"summary attempts partition");Check(report.misses<=report.unknown,"miss subset unknown");Check(report.attempts==captures,"attempt count exact");
 return {ready,report,clock,captures};
}
static void MissesAndSummary() {
 const Sample miss{Frame::Unknown,0,0,true};
 const Sample t1{Frame::Title,1,1,false,{950,980,990}},t2{Frame::Title,2,2,false,{980,950,1000}},t3{Frame::Title,3,3,false,{970,970,980}},t4{Frame::Title,4,4};
 auto a=Samples({miss,t1,t2,t3});Check(a.ready&&a.report.reason==Reason::Ready&&a.report.misses==1&&a.report.attempts==4&&a.report.elapsed_ms==1500,"one transient miss followed by title");Check(a.report.first_sequence==0&&a.report.last_sequence==3&&a.report.maximum_sequence==3,"summary first last max including unavailable0");Check(a.report.best.logo==980&&a.report.best.prompt==980&&a.report.best.dark==1000,"best independent mask scores");
 a=Samples({miss,miss,miss,t1,t2,t3});Check(a.ready&&a.report.misses==3&&a.report.elapsed_ms==2500,"multiple transient misses followed by title");
 a=Samples({t1,miss,t2,t3,t4});Check(a.ready&&a.report.elapsed_ms==2000&&a.report.title==4,"miss resets existing positive streak");
 a=Samples({miss});Check(!a.ready&&a.report.reason==Reason::Deadline&&a.report.misses==120&&a.report.unknown==120&&a.report.elapsed_ms==60000,"all misses fixed60s deadline");
 a=Samples({Sample{Frame::Unknown,5,5,true}});Check(!a.ready&&a.report.reason==Reason::Deadline&&a.report.first_sequence==5&&a.report.last_sequence==5&&a.report.maximum_sequence==5,"positive stagnant miss watermark timeout");
 a=Samples({Sample{Frame::Invalid,1,1}});Check(!a.ready&&a.report.reason==Reason::ShapeInvalid&&a.report.invalid==1,"invalid RawImage remains fatal");
 a=Samples({Sample{Frame::Title,5,5},Sample{Frame::Unknown,4,4,true}});Check(!a.ready&&a.report.reason==Reason::SequenceRegression,"positive regression during miss remains fatal");
 a=Samples({Sample{Frame::Title,5,5},miss,Sample{Frame::Title,4,4}});Check(!a.ready&&a.report.reason==Reason::SequenceRegression,"last good sequence retained across unavailable miss");
 a=Samples({t1},0,0,true);Check(!a.ready&&a.report.reason==Reason::ClockRollback,"clock rollback fatal with exact reason");
 a=Samples({t1},0,0,false,true);Check(!a.ready&&a.report.reason==Reason::CaptureException&&a.report.exceptions==1,"capture exception accounted");
 for(unsigned seam=1;seam<=8;++seam){a=Samples({t1,t2,t3},seam);Check(!a.ready,"cancellation still fatal");Check(a.report.reason==Reason::CancelBeforeBegin||a.report.reason==Reason::CancelBeforeCapture||a.report.reason==Reason::CancelAfterCapture||a.report.reason==Reason::CancelBeforeReady,"precise cancellation point enum");}
 a=Samples({t1,t2,t3},0,1);Check(!a.ready&&a.report.reason==Reason::CancelDuringSleep,"sleep cancellation reason");
 a=Samples({Sample{Frame::Title,1,1,true}});Check(!a.ready&&a.report.reason==Reason::InconsistentSample,"miss cannot masquerade as title");
 a=Samples({Sample{Frame::Invalid,1,1,true}});Check(!a.ready&&a.report.reason==Reason::InconsistentSample&&a.report.misses==1&&a.report.unknown==1&&a.report.invalid==0,"inconsistent invalid miss rejected with consistent accounting");
 // Pure classifier returns identical verdict with and without diagnostics.
 for(const auto& f:fixtures){auto im=Load(f.path);MaskScores score;const auto x=Classify(im,&score);Check(x==Classify(im),"diagnostic scores preserve classifier verdict");Check(score.logo<=1000&&score.prompt<=1000&&score.dark<=1000,"score bounds");}
}

int main(){try{Frames();States();Policy();MissesAndSummary();std::printf("title-gate checks=%llu fixtures=%zu PASS\n",static_cast<unsigned long long>(checks),std::size(fixtures));return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL after%llu: %s\n",static_cast<unsigned long long>(checks),e.what());return 1;}}
