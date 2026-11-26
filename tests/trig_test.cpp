// SPDX-License-Identifier: AGPL-3.0-or-later
// TRIG sequencer tests: timing, per-track length/speed, conditions, locks,
// MIDI output, clock, persistence and KICK import.
#include "engine.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace lab;
static int failures=0;
static void check(bool ok,const std::string& message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';++failures;}}
static void wav(const std::string& file,int frames){
    std::ofstream f(std::filesystem::u8path(file),std::ios::binary);
    auto u16=[&](unsigned n){char b[2]={char(n),char(n>>8)};f.write(b,2);};auto u32=[&](unsigned n){u16(n&65535);u16(n>>16);};
    f.write("RIFF",4);u32(36+frames*2);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(48000);u32(96000);u16(2);u16(16);f.write("data",4);u32(frames*2);
    for(int i=0;i<frames;++i){const short v=short(12000*std::sin(i*.07));u16(unsigned(v)&65535);}
}
struct Rig {
    std::unique_ptr<Engine> engine=std::make_unique<Engine>();
    std::vector<float> audio;std::vector<MidiEvent> midi;
    void run(int blocks){float block[Block*2];for(int b=0;b<blocks;++b){engine->render(block);for(int n=0;n<Block;++n)audio.push_back(std::abs(block[n*2]));MidiEvent e;while(engine->popMidi(e))midi.push_back(e);}}
    std::vector<MidiEvent> notesOn()const{std::vector<MidiEvent> r;for(auto& e:midi)if((e.message&0xF0)==0x90&&((e.message>>16)&127))r.push_back(e);return r;}
    int count(uint32_t status)const{int n=0;for(auto& e:midi)if((e.message&0xFF)==status)++n;return n;}
};
static double stepFrames(double tempo=120){return 48000*60./tempo/4.;}
static TrigTrack midiTrack(int channel){TrigTrack t;t.type=TrigTrackType::Midi;t.midiChannel=uint8_t(channel);t.defaultNote=60;return t;}
int main(int argc,char** argv){
    const auto dir=std::filesystem::absolute(argc>1?argv[1]:"trig-test-output");std::filesystem::create_directories(dir);
    const std::string sample=(dir/"tone.wav").u8string();wav(sample,4000);
    const double sf=stepFrames();
    // 1. MIDI timing, clock, start/stop.
    {Rig r;r.engine->setTempo(120);r.engine->trigSetTrackConfig(1,midiTrack(2));
     r.engine->trigToggle(1,0);r.engine->trigToggle(1,4);
     r.engine->setPlaying(true);r.run(int(16*sf/Block)-1);r.engine->stop();MidiEvent e;while(r.engine->popMidi(e))r.midi.push_back(e);
     auto on=r.notesOn();check(on.size()==2,"two MIDI notes expected in one bar, got "+std::to_string(on.size()));
     if(on.size()==2){check(on[0].frame<Block,"first note at frame 0");check(std::abs(double(on[1].frame)-4*sf)<2,"second note on step 5 ("+std::to_string(on[1].frame)+")");check((on[0].message&0x0F)==2,"MIDI channel 3");}
     check(r.count(0xFA)==1,"clock start sent");check(r.count(0xFC)==1,"clock stop sent");
     const int pulses=r.count(0xF8);check(pulses>=96&&pulses<=98,"24 ppqn clock: "+std::to_string(pulses));
     bool off=false;for(auto& m:r.midi)if((m.message&0xF0)==0x80)off=true;check(off,"note-off sent");}
    // 2. Per-track length and speed.
    {Rig r;auto t=midiTrack(0);t.length=3;r.engine->trigSetTrackConfig(0,t);r.engine->trigToggle(0,0);
     auto u=midiTrack(1);u.speed=0;r.engine->trigSetTrackConfig(1,u);r.engine->trigToggle(1,0); // 2x: 16 steps last half a bar
     r.engine->setPlaying(true);r.run(int(16*sf/Block)-1);
     int a=0,b=0;for(auto& e:r.notesOn()){if((e.message&15)==0)++a;else ++b;}
     check(a==6,"length-3 track should fire 6 times in 16 steps, got "+std::to_string(a));check(b==2,"2x track should loop twice per bar, got "+std::to_string(b));}
    // 3. Conditions: 1:2, FILL, PRE, NOT FIRST and probability 0.
    {Rig r;for(int t=0;t<5;++t){auto c=midiTrack(t);c.length=4;r.engine->trigSetTrackConfig(t,c);}
     TrigStep st;st.trig=true;st.condition=uint8_t(trigConditionFromCycle(1,2));r.engine->trigSetStep(0,0,st);
     st.condition=TrigCondFill;r.engine->trigSetStep(1,0,st);
     TrigStep pre=st;pre.condition=uint8_t(trigConditionFromCycle(2,2));r.engine->trigSetStep(2,0,pre);pre.condition=TrigCondPre;r.engine->trigSetStep(2,2,pre);
     st.condition=TrigCondNotFirst;r.engine->trigSetStep(3,0,st);
     st.condition=TrigCondNone;st.probability=0;r.engine->trigSetStep(4,0,st);
     r.engine->setPlaying(true);r.run(int(16*sf/Block)-1); // four loops of 4 steps
     int counts[5]={};for(auto& e:r.notesOn())++counts[e.message&15];
     check(counts[0]==2,"1:2 fires on loops 1 and 3: "+std::to_string(counts[0]));
     check(counts[1]==0,"FILL silent without fill");
     check(counts[2]==4,"2:2 + PRE fire together on loops 2 and 4: "+std::to_string(counts[2]));
     check(counts[3]==3,"!1ST skips first loop: "+std::to_string(counts[3]));
     check(counts[4]==0,"probability 0 never fires");
     r.midi.clear();r.engine->setFill(true);r.run(int(4*sf/Block)+1);int fill=0;for(auto& e:r.notesOn())if((e.message&15)==1)++fill;check(fill>=1,"FILL plays while fill is held");}
    // 4. Locks: CC lock + trigless lock on a MIDI track, micro timing.
    {Rig r;auto t=midiTrack(5);t.ccNumber[0]=74;r.engine->trigSetTrackConfig(0,t);
     r.engine->trigToggle(0,0);check(r.engine->trigSetLock(0,0,TrigCcFirst,100/127.),"lock written");
     check(r.engine->trigSetLock(0,2,TrigCcFirst,20/127.),"trigless lock written");
     TrigStep late;late.trig=true;late.micro=12;r.engine->trigSetStep(0,8,late);
     auto snap=std::make_unique<Snapshot>(r.engine->snapshot());check(!snap->sequences[0].trig.tracks[0].steps[2].trig&&snap->sequences[0].trig.tracks[0].steps[2].lockCount==1,"step 3 is a trigless lock");
     r.engine->setPlaying(true);r.run(int(10*sf/Block));
     std::vector<int> ccs;for(auto& e:r.midi)if((e.message&0xF0)==0xB0&&((e.message>>8)&127)==74)ccs.push_back(int((e.message>>16)&127));
     check(ccs.size()>=2&&ccs[0]==100&&ccs[1]==20,"CC lock then trigless CC lock");
     auto on=r.notesOn();check(on.size()==2&&std::abs(double(on[1].frame)-8.5*sf)<2,"micro +12/24 delays half a step");}
    // 5. Audio track: trig, level lock 0 mutes the hit, mute.
    {Rig r;std::string error;check(r.engine->load(0,sample,error),"load sample: "+error);
     r.engine->trigToggle(0,0);r.engine->trigToggle(0,8);r.engine->trigSetLock(0,8,TrigLevel,0);
     r.engine->setPlaying(true);r.run(int(16*sf/Block)-1);
     auto energy=[&](double from,double to){double e=0;for(size_t i=size_t(from);i<size_t(to)&&i<r.audio.size();++i)e+=r.audio[i];return e;};
     check(energy(0,3000)>10,"audio trig on step 1 is audible");check(energy(8*sf,8*sf+3000)<1e-3,"LEVEL lock 0 silences step 9");}
    // 6. Retrig: 4 MIDI hits inside one step.
    {Rig r;r.engine->trigSetTrackConfig(0,midiTrack(0));TrigStep st;st.trig=true;st.retrig=4;r.engine->trigSetStep(0,0,st);
     r.engine->setPlaying(true);r.run(int(2*sf/Block));check(r.notesOn().size()==4,"retrig x4");}
    // 7. Persistence round trip and KICK import.
    {std::string error;auto e=std::make_unique<Engine>();e->trigSetTrackConfig(3,midiTrack(9));TrigStep st;st.trig=true;st.condition=uint8_t(trigConditionFromCycle(3,4));st.micro=-5;st.retrig=2;st.note=67;e->trigSetStep(3,7,st);e->trigSetLock(3,7,TrigCcFirst+2,.5);e->trigSetMasterLength(32);
     const auto file=(dir/"roundtrip.trig").u8string();check(e->save(file,error),"save: "+error);
     auto f=std::make_unique<Engine>();check(f->open(file,error),"open: "+error);auto s=std::make_unique<Snapshot>(f->snapshot());const auto& tr=s->sequences[0].trig.tracks[3];
     check(tr.type==TrigTrackType::Midi&&tr.midiChannel==9,"track type/channel restored");
     check(tr.steps[7].trig&&tr.steps[7].note==67&&tr.steps[7].micro==-5&&tr.steps[7].retrig==2&&tr.steps[7].lockCount==1,"step restored");
     int a=0,b=0;check(trigConditionCycle(tr.steps[7].condition,a,b)&&a==3&&b==4,"condition 3:4 restored");
     check(s->sequences[0].trig.masterLength==32,"master length restored");
     // A pure KICK project: strip the TRIG section, the drum grid is imported.
     auto k=std::make_unique<Engine>();k->toggle(2,4);k->toggle(2,12);const auto kickFile=(dir/"kick.kicklab").u8string();check(k->save(kickFile,error),"kick save");
     std::ifstream in(kickFile);std::stringstream text;text<<in.rdbuf();std::string content=text.str();content=content.substr(0,content.find("TRIGDATA"));in.close();std::ofstream(kickFile,std::ios::trunc)<<content;
     auto g=std::make_unique<Engine>();check(g->open(kickFile,error),"open kick: "+error);auto gs=std::make_unique<Snapshot>(g->snapshot());
     check(gs->sequences[0].trig.tracks[2].steps[4].trig&&gs->sequences[0].trig.tracks[2].steps[12].trig,"KICK steps imported into TRIG");}
    // 8. Inherited KICK core: pad playback, inserts, kits and WAV export.
    {std::string error;auto e=std::make_unique<Engine>();check(e->load(4,sample,error),"core load");
     e->trigger(4);float block[Block*2];double energy=0;e->render(block);for(float v:block)energy+=std::abs(v);check(energy>1,"manual pad trigger is audible");
     e->setInsertEffect(4,0,InsertEffect::Reverb);auto s=std::make_unique<Snapshot>(e->snapshot());check(s->edits[4].inserts[0].effect==InsertEffect::Reverb,"insert effect set");
     PadEdit edit=s->edits[4];edit.gain=.4;e->setEdit(4,edit);const auto kit=(dir/"core.kickkit").u8string();check(e->saveKit(kit,dir.u8string(),error),"kit save: "+error);
     auto f=std::make_unique<Engine>();check(f->openKit(kit,dir.u8string(),error),"kit open: "+error);auto fs=std::make_unique<Snapshot>(f->snapshot());check(std::abs(fs->edits[4].gain-.4)<1e-9&&fs->edits[4].inserts[0].effect==InsertEffect::Reverb,"kit round trip");
     e->trigToggle(4,0);auto t=s->sequences[0].trig.tracks[4];t.length=32;e->trigSetTrackConfig(4,t);
     const auto exported=(dir/"export.wav").u8string();check(e->exportCurrentPatternWav(exported,error),"export: "+error);
     const auto bytes=std::filesystem::file_size(std::filesystem::u8path(exported));check(bytes==44+size_t(std::llround(32*sf))*6,"export covers the longest track (32 steps): "+std::to_string(bytes));
     MidiEvent m;int leaked=0;while(e->popMidi(m))if(m.message==0xF8)++leaked;check(leaked==0,"no MIDI clock during WAV export");}
    // 9. Condition names.
    check(trigConditionName(trigConditionFromCycle(1,4))=="1:4","cycle label");check(trigConditionCount()==44,"condition count");
    for(int code=0;code<trigConditionCount();++code){int a=0,b=0;if(trigConditionCycle(code,a,b))check(trigConditionFromCycle(a,b)==code,"cycle code round trip");}
    if(failures){std::cerr<<failures<<" TRIG test(s) failed\n";return 1;}
    std::cout<<"PASS: TRIG timing, length/speed, conditions, locks, micro, retrig, MIDI/clock, save/open, KICK import\n";return 0;
}
