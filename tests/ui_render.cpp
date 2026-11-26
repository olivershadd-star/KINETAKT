// SPDX-License-Identifier: AGPL-3.0-or-later
// Linux test harness: renders the TRIG interface with Cairo and replays
// clicks, so the layout and interactions can be checked without Windows.
#include "ui.hpp"
#include <cairo.h>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>
using namespace trig;
struct CairoPainter:Painter{
    cairo_t* c;explicit CairoPainter(cairo_t* cr):c(cr){}
    void set(Color k){cairo_set_source_rgb(c,k.r/255.,k.g/255.,k.b/255.);}
    void path(int x,int y,int w,int h,int r){r=std::min(r,std::min(w,h)/2);cairo_new_sub_path(c);cairo_arc(c,x+w-r,y+r,r,-M_PI/2,0);cairo_arc(c,x+w-r,y+h-r,r,0,M_PI/2);cairo_arc(c,x+r,y+h-r,r,M_PI/2,M_PI);cairo_arc(c,x+r,y+r,r,M_PI,1.5*M_PI);cairo_close_path(c);}
    void fill(int x,int y,int w,int h,Color k)override{set(k);cairo_rectangle(c,x,y,w,h);cairo_fill(c);}
    void frame(int x,int y,int w,int h,Color k,int t)override{set(k);cairo_set_line_width(c,t);cairo_rectangle(c,x+.5,y+.5,w-1,h-1);cairo_stroke(c);}
    void round(int x,int y,int w,int h,int r,Color k)override{set(k);path(x,y,w,h,r);cairo_fill(c);}
    void roundFrame(int x,int y,int w,int h,int r,Color k,int t)override{set(k);cairo_set_line_width(c,t);path(x,y,w,h,r);cairo_stroke(c);}
    void text(int x,int y,int w,int h,const std::string& s,int size,Color k,Align a,bool bold)override{
        cairo_save(c);cairo_rectangle(c,x,y,w,h);cairo_clip(c);set(k);cairo_select_font_face(c,"DejaVu Sans",CAIRO_FONT_SLANT_NORMAL,bold?CAIRO_FONT_WEIGHT_BOLD:CAIRO_FONT_WEIGHT_NORMAL);cairo_set_font_size(c,size*1.05);
        cairo_text_extents_t e;cairo_text_extents(c,s.c_str(),&e);double tx=a==Align::Left?x:a==Align::Center?x+(w-e.x_advance)/2:x+w-e.x_advance;cairo_font_extents_t fe;cairo_font_extents(c,&fe);
        cairo_move_to(c,tx,y+(h+fe.ascent-fe.descent)/2);cairo_show_text(c,s.c_str());cairo_restore(c);}
    void line(int x1,int y1,int x2,int y2,Color k,int w)override{set(k);cairo_set_line_width(c,w);cairo_set_line_cap(c,CAIRO_LINE_CAP_ROUND);cairo_move_to(c,x1,y1);cairo_line_to(c,x2,y2);cairo_stroke(c);}
    void arc(int cx,int cy,int r,double s,double sw,Color k,int w)override{set(k);cairo_set_line_width(c,w);cairo_set_line_cap(c,CAIRO_LINE_CAP_ROUND);cairo_new_path(c);cairo_arc(c,cx,cy,r,s*M_PI/180,(s+sw)*M_PI/180);cairo_stroke(c);}
    void disc(int cx,int cy,int r,Color k)override{set(k);cairo_new_path(c);cairo_arc(c,cx,cy,r,0,2*M_PI);cairo_fill(c);}
};
struct FakeHost:Host{
    std::vector<int> answers;std::vector<std::string> titles;
    int popup(const std::vector<MenuItem>& items)override{std::cout<<"  [menu]";for(auto& i:items)std::cout<<" | "<<(i.separator?"---":i.label);std::cout<<"\n";if(answers.empty())return -1;int a=answers.front();answers.erase(answers.begin());return a;}
    std::string openPath(const std::string&,const std::string&)override{return {};}
    std::string savePath(const std::string&,const std::string&,const std::string&)override{return {};}
    bool confirm(const std::string&)override{return true;}
    void setTitle(const std::string& t)override{titles.push_back(t);}
    std::vector<std::string> audioOutputs()override{return {"Haut-parleurs"};} int audioOutput()override{return -1;} void selectAudioOutput(int)override{}
    std::vector<std::string> midiInputs()override{return {"LPD8","MPK mini 3"};} int midiInput()override{return 1;} void selectMidiInput(int)override{}
    std::vector<std::string> midiOutputs()override{return {"loopMIDI Port"};} int midiOutput()override{return 0;} void selectMidiOutput(int)override{}
    void stopAudio()override{} void startAudio()override{} void quit()override{}
};
static void shot(TrigUI& ui,const char* file){
    auto* surface=cairo_image_surface_create(CAIRO_FORMAT_RGB24,DesignWidth,DesignHeight);auto* cr=cairo_create(surface);CairoPainter p(cr);ui.paint(p);cairo_surface_write_to_png(surface,file);cairo_destroy(cr);cairo_surface_destroy(surface);std::cout<<"wrote "<<file<<"\n";
}
int main(int argc,char** argv){
    const std::string base=argc>1?argv[1]:".";
    auto engine=std::make_unique<lab::Engine>();FakeHost host;TrigUI ui(*engine,host,base);ui.loadDemo();
    float block[lab::Block*2];
    shot(ui,"ui-1-demo.png");
    // Play a little so playheads show, then latch steps of the bass and turn a knob.
    ui.key(KeySpace,true,0);for(int b=0;b<140;++b)engine->render(block);ui.tick();
    ui.mouseDown(100,124+12*46+20,0,0);ui.mouseUp(0,0,0); // select track 13
    ui.mouseDown(376+3*74+30,280,1,0);                    // right-click step 4 → latch
    ui.page=1;ui.mouseDown(376+20,500,0,0);ui.mouseMove(376+20,460,0);ui.mouseUp(0,0,0); // drag PITCH knob up
    shot(ui,"ui-2-locks.png");
    auto s=std::make_unique<lab::Snapshot>(engine->snapshot());const auto& st=s->sequences[0].trig.tracks[12].steps[3];
    std::cout<<"bass step 4: trig="<<st.trig<<" locks="<<int(st.lockCount)<<" latched="<<ui.latched.size()<<"\n";
    // Trig page with a condition.
    ui.page=0;ui.wheel(376+4*151+60,520,5,0);shot(ui,"ui-3-trigpage.png");
    // MIDI track: track 16 → MIDI via the SON|MIDI switch.
    ui.mouseDown(100,124+15*46+20,0,0);ui.mouseUp(0,0,0);ui.mouseDown(376+330+110,148,0,0);ui.mouseUp(0,0,0);
    ui.mouseDown(376+10,280,0,0);ui.mouseUp(0,0,0);ui.mouseDown(376+74*2+10,280,0,0);ui.mouseUp(0,0,0);ui.page=1;
    shot(ui,"ui-4-midi.png");
    s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"track16 type="<<int(s->sequences[0].trig.tracks[15].type)<<" step1="<<s->sequences[0].trig.tracks[15].steps[0].trig<<"\n";
    // FX page.
    ui.mouseDown(100,124+0*46+20,0,0);ui.mouseUp(0,0,0);ui.page=3;shot(ui,"ui-5-fx.png");
    // Macro: touch DECAY on track 1 (AMP page), assign it to M1, then turn M1.
    ui.page=2;ui.mouseDown(376+151+40,500,0,0);ui.mouseUp(0,0,0);
    host.answers={0};ui.mouseDown(376+40,760,1,0);
    ui.mouseDown(376+40,760,0,0);ui.mouseMove(376+40,700,0);ui.mouseUp(0,0,0);
    s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"track1 decay after M1="<<s->edits[0].decay<<"\n";
    // Trigless lock on an empty step of track 1 (step 3), then a trig-condition change.
    ui.mouseDown(376+2*74+20,280,1,0);ui.page=2;ui.wheel(376+151+40,500,10,0);
    s=std::make_unique<lab::Snapshot>(engine->snapshot());const auto& tl=s->sequences[0].trig.tracks[0].steps[2];std::cout<<"step3 trig="<<tl.trig<<" locks="<<int(tl.lockCount)<<"\n";
    shot(ui,"ui-6-macro-trigless.png");
    // v0.2: slide on a latched step, copy/paste, undo, scenes + crossfader.
    ui.key(KeyEscape,true,0);ui.mouseDown(100,124+12*46+20,0,0);ui.mouseUp(0,0,0);ui.page=0;
    ui.mouseDown(376+20,280,1,0);ui.wheel(376+7*151+60,520,1,0);   // latch step 1, SLIDE = OUI
    s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"slide step1="<<s->sequences[0].trig.tracks[12].steps[0].slide<<"\n";
    ui.key('C',true,ModControl);ui.key(KeyEscape,true,0);ui.mouseDown(376+74*1+20,280,1,0);ui.key('V',true,ModControl); // paste on step 2
    s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"pasted step2 trig="<<s->sequences[0].trig.tracks[12].steps[1].trig<<" slide="<<s->sequences[0].trig.tracks[12].steps[1].slide<<"\n";
    ui.key('Z',true,ModControl);s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"after undo step2 trig="<<s->sequences[0].trig.tracks[12].steps[1].trig<<"\n";
    ui.key('Y',true,ModControl);s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"after redo step2 trig="<<s->sequences[0].trig.tracks[12].steps[1].trig<<"\n";
    shot(ui,"ui-7-slide.png");
    ui.key(KeyEscape,true,0);ui.mouseDown(376+126+20,670,0,0);ui.mouseUp(0,0,0);          // SCÈNES view
    ui.mouseDown(376+1*75+20,720,0,0);ui.mouseUp(0,0,0);                                  // edit scene 2
    ui.page=2;ui.wheel(376+151+40,500,-30,0);                                              // track 13 DECAY into scene 2
    ui.mouseDown(376+90+400,784,0,0);std::cout<<"fader click: "<<ui.status<<"\n";ui.mouseUp(0,0,0);                                   // crossfader
    s=std::make_unique<lab::Snapshot>(engine->snapshot());std::cout<<"scene2 values="<<int(s->sequences[0].trig.scenes[1].count)<<" fader="<<s->crossfader<<"\n";
    shot(ui,"ui-8-scenes.png");
    std::cout<<"status: "<<ui.status<<"\n";
    return 0;
}
