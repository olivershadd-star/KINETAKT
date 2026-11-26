// SPDX-License-Identifier: AGPL-3.0-or-later
// TRIG — Elektron-style sequencer interface (platform independent).
#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <initializer_list>

namespace trig {
namespace {
using namespace lab;
constexpr Color Bg{16,21,30},Panel{24,30,42},Panel2{31,39,54},Line{46,57,76},Ink{236,241,248},Muted{140,154,176},Dim{86,98,120};
constexpr Color Blue{25,185,255},Red{239,52,62},Yellow{245,195,67},Orange{255,138,76},Green{76,217,140},White{255,255,255};
Color mix(Color a,Color b,double t){t=std::clamp(t,0.,1.);return {(unsigned char)(a.r+(b.r-a.r)*t),(unsigned char)(a.g+(b.g-a.g)*t),(unsigned char)(a.b+(b.b-a.b)*t)};}
struct Rect { int x=0,y=0,w=0,h=0; bool in(int a,int b)const{return a>=x&&a<x+w&&b>=y&&b<y+h;} };
std::string noteName(int note){static const char* names[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};note=std::clamp(note,0,127);return std::string(names[note%12])+std::to_string(note/12-1);}
std::string patternName(int index){return std::string(1,char('A'+index));}
std::string fileStem(const std::string& path){return std::filesystem::u8path(path).stem().u8string();}

// ----- layout -----
const Rect HeaderPlay{196,12,74,44},HeaderStop{276,12,74,44},HeaderRec{356,12,74,44};
const Rect HeaderTempo{448,12,118,44},HeaderSwing{574,12,96,44},HeaderMaster{678,12,110,44},HeaderFill{796,12,92,44};
const Rect HeaderClock{896,12,112,44},HeaderMidiOut{1016,12,300,44},HeaderFile{1324,12,128,44},HeaderSettings{1460,12,124,44};
constexpr int PatternY=72,PatternH=38,PatternX=110,PatternW=50,PatternGap=5;
Rect patternRect(int i){return {PatternX+i*(PatternW+PatternGap),PatternY,PatternW,PatternH};}
const Rect PatternSong{1000,72,110,38},PatternKit{1118,72,110,38},PatternMenu{1236,72,110,38};
constexpr int TrackX=16,TrackY=124,TrackW=344,TrackH=43,TrackGap=3;
Rect trackRect(int t){return {TrackX,TrackY+t*(TrackH+TrackGap),TrackW,TrackH};}
Rect trackMute(int t){auto r=trackRect(t);return {r.x+r.w-70,r.y+9,30,26};}
Rect trackSolo(int t){auto r=trackRect(t);return {r.x+r.w-36,r.y+9,30,26};}
Rect trackPlay(int t){auto r=trackRect(t);return {r.x+6,r.y+6,32,32};}
constexpr int MainX=376,MainW=1208;
const Rect TrackType{MainX+330,128,150,40},TrackLength{MainX+490,128,110,40},TrackSpeed{MainX+608,128,96,40},TrackChannel{MainX+712,128,96,40},TrackSample{MainX+712,128,250,40};
const Rect TrackCopy{MainX+972,128,74,40},TrackPaste{MainX+1052,128,74,40},TrackClear{MainX+1132,128,76,40};
constexpr int PagesY=180;
Rect stepPageRect(int p){return {MainX+p*96,PagesY,90,30};}
const Rect FollowRect{MainX+392,PagesY,104,30},PageCopy{MainX+504,PagesY,120,30},PagePaste{MainX+630,PagesY,120,30};
constexpr int StepY=232,StepH=96,StepW=70,StepGapSmall=4,StepGapBeat=12;
Rect stepRect(int i){const int beat=i/4,inBeat=i%4;const int x=MainX+beat*(4*StepW+3*StepGapSmall+StepGapBeat)+inBeat*(StepW+StepGapSmall);return {x,StepY,StepW,StepH};}
const Rect LatchInfo{MainX,346,520,30},LatchCopy{MainX+544,346,160,30},LatchPaste{MainX+712,346,160,30},LatchClear{MainX+880,346,160,30},LatchRelease{MainX+1048,346,160,30};
constexpr int TabY=392,TabH=36;
constexpr int KnobY=440,KnobH=196,KnobW=151;
Rect knobRect(int i){return {MainX+i*KnobW,KnobY,KnobW-6,KnobH};}
const Rect FxEffect{MainX+620,TabY,360,TabH},FxBypass{MainX+988,TabY,104,TabH};
constexpr int MacroY=660,MacroH=196;
Rect macroRect(int i){return {MainX+i*KnobW,MacroY+34,KnobW-6,MacroH-34};}
const Rect ViewMacros{MainX,MacroY,120,28},ViewScenes{MainX+126,MacroY,120,28};
Rect sceneRect(int i){return {MainX+i*75,MacroY+42,70,42};}
const Rect Crossfader{MainX+90,MacroY+104,MainW-180,40};

enum class Field { Note=0, Velocity, Length, Probability, Condition, Micro, Retrig, Slide, Count };
struct Knob {
    std::string label,display;double value=0;bool enabled=false,locked=false,scene=false;
    int kind=0;     // 0 none, 1 lockable parameter, 2 trig field
    int id=0;       // TrigParam or Field
    int ccSlot=-1;  // MIDI CC page: slot index (its CC number is editable)
};
struct Macro { int track=-1,param=-1;double value=.5;int midiChannel=-1,midiCc=-1; };
}

struct TrigUI::Impl {
    TrigUI& ui;Engine& engine;Host& host;std::filesystem::path base,project;
    std::unique_ptr<Snapshot> snap=std::make_unique<Snapshot>();
    bool playing=false,record=false,follow=true,fillKey=false,fillMouse=false;
    int dragKnob=-1,dragMacro=-1,dragHeader=-1,dragStartY=0,mouseX=0,mouseY=0;double dragStart=0;
    int paintStep=-1;bool paintValue=false;
    int lastParam=-1,lastTrack=-1;
    std::array<Macro,8> macros{};int learnMacro=-1;
    std::unique_ptr<TrigTrack> trackClipboard;std::unique_ptr<TrigPattern> patternClipboard;
    int tickCount=0;
    // v0.2: scenes, clipboards, undo.
    bool sceneView=false;int editScene=-1;bool dragFader=false;int faderChannel=-1,faderCc=-1;bool learnFader=false;
    std::vector<std::pair<int,TrigStep>> stepClipboard;std::unique_ptr<std::array<TrigStep,16>> pageClipboard;std::unique_ptr<TrigScene> sceneClipboard;
    std::vector<std::pair<int,TrigPattern>> undoStack,redoStack;std::unique_ptr<TrigPattern> committed;int committedSequence=-1;
    bool undoCheck=false,gestureOpen=false,gesturePushed=false;int lastChangeTick=-100;std::string lastGesture,gesture;
    Impl(TrigUI& owner,Engine& e,Host& h,std::filesystem::path dir):ui(owner),engine(e),host(h),base(std::move(dir)){loadSettings();}

    // ----- state helpers -----
    void refresh(){*snap=engine.snapshot();playing=snap->playing;if(undoCheck){undoCheck=false;trackUndo();}else if(committedSequence!=snap->sequence)commit();}
    // ----- undo: the sequencer data of the current pattern is versioned -----
    static bool sameStep(const TrigStep& a,const TrigStep& b){
        if(a.trig!=b.trig||a.note!=b.note||a.velocity!=b.velocity||a.length!=b.length||a.probability!=b.probability||a.condition!=b.condition||a.micro!=b.micro||a.retrig!=b.retrig||a.slide!=b.slide||a.lockCount!=b.lockCount)return false;
        for(int i=0;i<a.lockCount;++i)if(a.locks[size_t(i)].param!=b.locks[size_t(i)].param||a.locks[size_t(i)].value!=b.locks[size_t(i)].value)return false;return true;
    }
    static bool samePattern(const TrigPattern& a,const TrigPattern& b){
        if(a.masterLength!=b.masterLength||a.swing!=b.swing||a.sceneA!=b.sceneA||a.sceneB!=b.sceneB)return false;
        for(int sc=0;sc<TrigScenes;++sc){const auto& x=a.scenes[size_t(sc)];const auto& y=b.scenes[size_t(sc)];if(x.count!=y.count)return false;for(int i=0;i<x.count;++i)if(x.locks[size_t(i)].track!=y.locks[size_t(i)].track||x.locks[size_t(i)].param!=y.locks[size_t(i)].param||x.locks[size_t(i)].value!=y.locks[size_t(i)].value)return false;}
        for(int t=0;t<TrigTracks;++t){const auto& x=a.tracks[size_t(t)];const auto& y=b.tracks[size_t(t)];
            if(x.type!=y.type||x.length!=y.length||x.speed!=y.speed||x.midiChannel!=y.midiChannel||x.defaultNote!=y.defaultNote||x.defaultVelocity!=y.defaultVelocity||x.defaultLength!=y.defaultLength||x.ccNumber!=y.ccNumber||x.ccValue!=y.ccValue)return false;
            for(int i=0;i<TrigMaxSteps;++i)if(!sameStep(x.steps[size_t(i)],y.steps[size_t(i)]))return false;}
        return true;
    }
    void commit(){committed=std::make_unique<TrigPattern>(pattern());committedSequence=snap->sequence;}
    void trackUndo(){
        if(!committed||committedSequence!=snap->sequence){commit();return;}
        if(samePattern(*committed,pattern()))return;
        const bool coalesce=(gestureOpen&&gesturePushed)||(!gestureOpen&&!gesture.empty()&&gesture==lastGesture&&tickCount-lastChangeTick<20);
        if(!coalesce){undoStack.emplace_back(committedSequence,*committed);if(undoStack.size()>64)undoStack.erase(undoStack.begin());redoStack.clear();}
        if(gestureOpen)gesturePushed=true;lastGesture=gesture;lastChangeTick=tickCount;commit();
    }
    void undo(bool redo){
        auto& from=redo?redoStack:undoStack;auto& to=redo?undoStack:redoStack;if(from.empty()){status(redo?"Rien à rétablir.":"Rien à annuler.");return;}
        refresh();auto entry=std::move(from.back());from.pop_back();
        if(entry.first!=snap->sequence){engine.selectSequence(entry.first);refresh();}
        to.emplace_back(snap->sequence,pattern());engine.trigSetPattern(entry.first,entry.second);ui.latched.clear();
        *snap=engine.snapshot();commit();ui.dirty=true;updateTitle();status(redo?"Rétabli.":"Annulé (Ctrl+Y pour rétablir).");
    }
    const TrigPattern& pattern()const{return snap->sequences[snap->sequence].trig;}
    const TrigTrack& track(int t)const{return pattern().tracks[std::clamp(t,0,TrigTracks-1)];}
    const TrigTrack& current()const{return track(ui.selectedTrack);}
    bool isMidi(int t)const{return track(t).type==TrigTrackType::Midi;}
    void changed(){ui.dirty=true;undoCheck=true;updateTitle();}
    void updateTitle(){host.setTitle("ELEKTAKT — "+(project.empty()?std::string("Nouveau projet"):project.filename().u8string())+(ui.dirty?" *":""));}
    std::string trackName(int t)const{
        if(isMidi(t))return "MIDI · canal "+std::to_string(track(t).midiChannel+1);
        const auto& name=snap->names[t];return name.empty()?"Vide":name;
    }
    int pages()const{return (current().length+15)/16;}
    void selectTrack(int t){t=std::clamp(t,0,TrigTracks-1);if(t!=ui.selectedTrack)ui.latched.clear();ui.selectedTrack=t;engine.selectTrack(t);ui.stepPage=std::min(ui.stepPage,std::max(0,pages()-1));if(isMidi(t)&&ui.page>1)ui.page=0;}
    std::vector<std::string> pageNames()const{
        if(isMidi(ui.selectedTrack))return {"TRIG","CC MIDI"};
        std::vector<std::string> names={"TRIG","SRC","AMP"};
        for(int slot=0;slot<InsertSlots;++slot){const auto effect=snap->edits[ui.selectedTrack].inserts[slot].effect;names.push_back("FX"+std::to_string(slot+1)+(effect==InsertEffect::None?"":" · "+std::string(insertEffectName(effect))));}
        return names;
    }
    const TrigStep* firstLatched()const{return ui.latched.empty()?nullptr:&current().steps[size_t(ui.latched.front())];}

    // ----- knobs -----
    std::vector<Knob> knobs()const{
        std::vector<Knob> result(8);const int t=ui.selectedTrack;const auto& tr=current();const TrigStep* step=firstLatched();
        auto field=[&](int slot,Field f,const std::string& label){
            Knob& k=result[size_t(slot)];k.kind=2;k.id=int(f);k.label=label;k.enabled=true;
            int value=0,minimum=0,maximum=1;std::string text;
            switch(f){
            case Field::Note:minimum=0;maximum=127;value=step?(step->note>=0?step->note:tr.defaultNote):tr.defaultNote;text=noteName(value);if(!isMidi(t)){k.enabled=false;text="SRC → PITCH";}break;
            case Field::Velocity:minimum=1;maximum=127;value=step?step->velocity:tr.defaultVelocity;text=std::to_string(value);break;
            case Field::Length:minimum=1;maximum=64;value=step?step->length:tr.defaultLength;text=std::to_string(value)+(value>1?" pas":" pas");break;
            case Field::Probability:minimum=0;maximum=100;value=step?step->probability:100;text=std::to_string(value)+" %";k.enabled=step!=nullptr;break;
            case Field::Condition:minimum=0;maximum=trigConditionCount()-1;value=step?step->condition:0;text=trigConditionName(value);k.enabled=step!=nullptr;break;
            case Field::Micro:minimum=-23;maximum=23;value=step?step->micro:0;{char buffer[16];std::snprintf(buffer,sizeof(buffer),"%+d/24",value);text=value==0?"0":buffer;}k.enabled=step!=nullptr;break;
            case Field::Retrig:minimum=1;maximum=8;value=step?step->retrig:1;text="x"+std::to_string(value);k.enabled=step!=nullptr;break;
            case Field::Slide:minimum=0;maximum=1;value=step&&step->slide?1:0;text=value?"OUI":"NON";k.enabled=step!=nullptr;break;
            default:break;
            }
            k.value=maximum>minimum?double(value-minimum)/(maximum-minimum):0;k.display=k.enabled?text:"—";
        };
        auto param=[&](int slot,int id){
            Knob& k=result[size_t(slot)];k.kind=1;k.id=id;k.enabled=true;
            const PadEdit& edit=snap->edits[t];
            double base=id>=TrigCcFirst?std::max(0,int(tr.ccValue[size_t(id-TrigCcFirst)]))/127.:trigParamBase(edit,id);
            k.label=trigParamName(id,&edit);
            if(id>=TrigCcFirst){k.ccSlot=id-TrigCcFirst;k.label="CC "+std::to_string(tr.ccNumber[size_t(k.ccSlot)]);}
            double value=base;
            if(editScene>=0){const auto& sc=pattern().scenes[size_t(editScene)];const int at=sc.find(t,id);if(at>=0){value=sc.locks[size_t(at)].value/double(TrigLockResolution);k.scene=true;}}
            else if(step){const int index=step->findLock(id);if(index>=0){value=step->locks[size_t(index)].value/double(TrigLockResolution);k.locked=true;}}
            k.value=value;k.display=trigParamDisplay(id,value);
            if(id>=TrigCcFirst&&!k.locked&&tr.ccValue[size_t(id-TrigCcFirst)]<0)k.display="—";
        };
        if(ui.page==0){field(0,Field::Note,"NOTE");field(1,Field::Velocity,"VÉLOCITÉ");field(2,Field::Length,"LONGUEUR");field(3,Field::Probability,"PROBABILITÉ");field(4,Field::Condition,"CONDITION");field(5,Field::Micro,"MICRO");field(6,Field::Retrig,"RETRIG");field(7,Field::Slide,"SLIDE");}
        else if(isMidi(t)){for(int c=0;c<TrigMidiCcs;++c)param(c,TrigCcFirst+c);}
        else if(ui.page==1){param(0,TrigPitch);param(1,TrigStart);param(2,TrigEnd);param(3,TrigLevel);param(4,TrigPan);}
        else if(ui.page==2){param(0,TrigAttack);param(1,TrigDecay);param(2,TrigSustain);param(3,TrigRelease);}
        else{
            const int slot=ui.page-3;const auto effect=snap->edits[t].inserts[size_t(slot)].effect;const int count=std::min(6,insertParameterCount(effect));
            for(int p=0;p<count;++p)param(p,trigFxParam(slot,p));
        }
        return result;
    }
    static int fieldRange(Field f,int& minimum){switch(f){case Field::Note:minimum=0;return 127;case Field::Velocity:minimum=1;return 127;case Field::Length:minimum=1;return 64;case Field::Probability:minimum=0;return 100;case Field::Condition:minimum=0;return trigConditionCount()-1;case Field::Micro:minimum=-23;return 23;case Field::Retrig:minimum=1;return 8;case Field::Slide:minimum=0;return 1;default:minimum=0;return 1;}}
    static double paramResolution(int id){if(id==TrigPitch)return 48;if(id>=TrigCcFirst)return 127;return 100;}
    // Sets a lockable parameter: locks on latched steps, otherwise the sound itself.
    void setParam(int t,int id,double v,bool allowLocks=true){
        v=std::clamp(v,0.,1.);lastParam=id;lastTrack=t;
        if(allowLocks&&editScene>=0){
            if(engine.trigSetSceneValue(editScene,t,id,v))status("SCÈNE "+std::to_string(editScene+1)+" : T"+std::to_string(t+1)+" "+trigParamName(id,&snap->edits[t])+" = "+trigParamDisplay(id,v));
            else status("La scène est pleine (64 valeurs).");
        }else if(allowLocks&&t==ui.selectedTrack&&!ui.latched.empty()){
            if(id>=TrigFxFirst&&id<TrigCcFirst){const int slot=(id-TrigFxFirst)/6;if(snap->edits[t].inserts[size_t(slot)].bypass)engine.setInsertBypass(t,slot,false);}
            bool full=false;for(int s:ui.latched)if(!engine.trigSetLock(t,s,id,v))full=true;
            status(full?"Un pas a déjà 8 locks : supprime-en un avant d'en ajouter.":"LOCK "+trigParamName(id,&snap->edits[t])+" = "+trigParamDisplay(id,v)+" sur "+std::to_string(ui.latched.size())+" pas.");
        }else if(id<TrigFxFirst){PadEdit edit=snap->edits[t];trigParamApply(edit,id,v);engine.setEdit(t,edit);}
        else if(id<TrigCcFirst){const int slot=(id-TrigFxFirst)/6;if(snap->edits[t].inserts[size_t(slot)].bypass){engine.setInsertBypass(t,slot,false);status("FX"+std::to_string(slot+1)+" activé.");}engine.setInsertParameter(t,slot,(id-TrigFxFirst)%6,v);}
        else engine.trigSetCcBase(t,id-TrigCcFirst,int(std::lround(v*127)));
        changed();refresh();
    }
    void setField(Field f,int value){
        int minimum=0;const int maximum=fieldRange(f,minimum);value=std::clamp(value,minimum,maximum);const int t=ui.selectedTrack;
        if(!ui.latched.empty()){
            for(int s:ui.latched){TrigStep st=current().steps[size_t(s)];if(!st.trig&&f!=Field::Slide)continue;
                switch(f){case Field::Slide:st.slide=value!=0;break;case Field::Note:st.note=int8_t(value);break;case Field::Velocity:st.velocity=uint8_t(value);break;case Field::Length:st.length=uint8_t(value);break;case Field::Probability:st.probability=uint8_t(value);break;case Field::Condition:st.condition=uint8_t(value);break;case Field::Micro:st.micro=int8_t(value);break;case Field::Retrig:st.retrig=uint8_t(value);break;default:break;}
                engine.trigSetStep(t,s,st);}
        }else{
            TrigTrack config=current();
            if(f==Field::Note)config.defaultNote=uint8_t(value);else if(f==Field::Velocity)config.defaultVelocity=uint8_t(value);else if(f==Field::Length)config.defaultLength=uint8_t(value);else return;
            engine.trigSetTrackConfig(t,config);
        }
        changed();refresh();
    }
    void knobDelta(int index,double units){
        const auto list=knobs();if(index<0||index>=int(list.size()))return;const auto& k=list[size_t(index)];if(!k.enabled||!k.kind)return;
        if(k.kind==2){int minimum=0;const int maximum=fieldRange(Field(k.id),minimum);const int value=int(std::lround(minimum+k.value*(maximum-minimum)));setField(Field(k.id),value+int(units));}
        else setParam(ui.selectedTrack,k.id,k.value+units/paramResolution(k.id));
    }
    void knobAbsolute(int index,double start,int pixels,bool fine){
        const auto list=knobs();if(index<0||index>=int(list.size()))return;const auto& k=list[size_t(index)];if(!k.enabled||!k.kind)return;
        const double v=std::clamp(start+pixels/(fine?800.:200.),0.,1.);
        if(k.kind==2){int minimum=0;const int maximum=fieldRange(Field(k.id),minimum);setField(Field(k.id),int(std::lround(minimum+v*(maximum-minimum))));}
        else{double value=v;if(k.id==TrigPitch||k.id>=TrigCcFirst){const double r=paramResolution(k.id);value=std::round(v*r)/r;}setParam(ui.selectedTrack,k.id,value);}
    }
    void setMacro(int index,double value){
        auto& m=macros[size_t(index)];m.value=std::clamp(value,0.,1.);
        if(m.track<0||m.param<0){status("M"+std::to_string(index+1)+" n'est pas assignée : touche un potard, puis clic droit sur la macro.");return;}
        setParam(m.track,m.param,m.value,false);
    }

    // ----- status / settings -----
    void status(const std::string& text){ui.status=text;}
    std::filesystem::path settingsPath()const{return base/"ELEKTAKT-settings.txt";}
    void saveSettings(){
        std::ofstream f(settingsPath().u8string(),std::ios::trunc);if(!f)return;f<<"TRIGSETTINGS 1\n";
        for(const auto& m:macros)f<<"M "<<m.track<<' '<<m.param<<' '<<m.value<<' '<<m.midiChannel<<' '<<m.midiCc<<'\n';
        f<<"FOLLOW "<<follow<<'\n';f<<"FADER "<<faderChannel<<' '<<faderCc<<'\n';
    }
    void loadSettings(){
        std::ifstream f(settingsPath().u8string());std::string tag;int version=0;if(!f||!(f>>tag>>version)||tag!="TRIGSETTINGS")return;
        for(auto& m:macros){Macro parsed;if(!(f>>tag>>parsed.track>>parsed.param>>parsed.value>>parsed.midiChannel>>parsed.midiCc)||tag!="M")return;if(parsed.track>=TrigTracks||parsed.param>=TrigParamCount)parsed=Macro{};m=parsed;}
        int value=1;if(f>>tag>>value&&tag=="FOLLOW")follow=value!=0;
        int channel=-1,cc=-1;if(f>>tag>>channel>>cc&&tag=="FADER"){faderChannel=channel;faderCc=std::clamp(cc,-1,127);}
    }

    // ----- project -----
    bool saveProject(bool ask){
        auto path=project;
        if(ask||path.empty()){const auto chosen=host.savePath("Enregistrer le projet ELEKTAKT","project",project.empty()?"projet.elektakt":project.filename().u8string());if(chosen.empty())return false;path=std::filesystem::u8path(chosen);if(path.extension()!=".elektakt")path+=".elektakt";}
        std::string error;if(!engine.save(path.u8string(),error)){status("Enregistrement impossible : "+error);return false;}
        project=path;ui.dirty=false;updateTitle();status("Projet enregistré : "+path.filename().u8string());return true;
    }
    bool confirmDiscard(){if(!ui.dirty)return true;return host.confirm("Le projet a des modifications non enregistrées. Les abandonner ?");}
    void openProject(const std::string& kind){
        if(!confirmDiscard())return;const auto chosen=host.openPath(kind=="kick"?"Importer un projet KICK":"Ouvrir un projet ELEKTAKT",kind);if(chosen.empty())return;
        host.stopAudio();std::string error;const bool ok=engine.open(chosen,error);host.startAudio();
        if(!ok){status("Ouverture impossible : "+error);return;}
        project=kind=="kick"?std::filesystem::path():std::filesystem::u8path(chosen);ui.dirty=kind=="kick";ui.latched.clear();refresh();updateTitle();
        status(error.empty()?(kind=="kick"?"Projet KICK importé : ses pas sont devenus des trigs. Enregistre-le en .elektakt.":"Projet ouvert : "+fileStem(chosen)):error);
    }
    void newProject(){
        if(!confirmDiscard())return;host.stopAudio();engine.resetProject();loadKit();host.startAudio();
        project.clear();ui.dirty=false;ui.latched.clear();ui.selectedTrack=0;ui.page=0;ui.stepPage=0;refresh();updateTitle();status("Nouveau projet : kit de démonstration chargé, pattern vide.");
    }
    void loadKit(){
        static const char* files[16]={"01-Kick.wav","02-Snare.wav","03-Closed-Hat.wav","04-Open-Hat.wav","05-Clap.wav","06-Low-Tom.wav","07-Mid-Tom.wav","08-High-Tom.wav","09-Rim.wav","10-Shaker.wav","11-Claves.wav","12-Cowbell.wav","13-Bass-C.wav","14-Bass-E.wav","15-Bass-G.wav","16-Chime.wav"};
        std::string error;for(int p=0;p<16;++p){const auto path=base/"demo-kit"/files[p];if(std::filesystem::exists(path))engine.load(p,path.u8string(),error);}
    }
    void exportWav(){
        const auto chosen=host.savePath("Exporter le pattern en WAV","wav",patternName(snap->sequence)+".wav");if(chosen.empty())return;
        host.stopAudio();std::string error;const bool ok=engine.exportCurrentPatternWav(chosen,error);host.startAudio();
        status(ok?"WAV exporté (48 kHz / 24 bits, un cycle du pattern) : "+std::filesystem::u8path(chosen).filename().u8string():"Export impossible : "+error);
    }

    // ----- menus -----
    void fileMenu(){
        const int choice=host.popup({{"Nouveau projet"},{"Ouvrir un projet ELEKTAKT…"},{"Enregistrer"},{"Enregistrer sous…"},{"",false,true,true},{"Importer un projet KICK (.kicklab)…"},{"",false,true,true},{"Charger un kit…"},{"Enregistrer le kit…"},{"",false,true,true},{"Exporter le pattern en WAV…"},{"",false,true,true},{"Quitter"}});
        switch(choice){
        case 0:newProject();break;case 1:openProject("project");break;case 2:saveProject(false);break;case 3:saveProject(true);break;
        case 5:openProject("kick");break;
        case 7:{const auto chosen=host.openPath("Charger un kit","kit");if(chosen.empty())break;std::string error;if(engine.openKit(chosen,(base/"samples").u8string(),error)){changed();status("Kit chargé : "+fileStem(chosen));}else status("Kit illisible : "+error);break;}
        case 8:{const auto chosen=host.savePath("Enregistrer le kit","kit","kit.kickkit");if(chosen.empty())break;std::string error;status(engine.saveKit(chosen,(base/"samples").u8string(),error)?"Kit enregistré.":"Impossible : "+error);break;}
        case 10:exportWav();break;case 12:host.quit();break;default:break;
        }
    }
    void deviceMenu(const std::vector<std::string>& names,int current,const std::string& none,std::function<void(int)> select){
        std::vector<MenuItem> items{{none,current<0}};for(size_t i=0;i<names.size();++i)items.push_back({names[i],int(i)==current});
        const int choice=host.popup(items);if(choice<0)return;select(choice-1);
    }
    void settingsMenu(){
        const int choice=host.popup({{"Sortie audio…"},{"Entrée MIDI (clavier / pads / potards)…"},{"Sortie MIDI vers Ableton…"},{"",false,true,true},{"Suivre la tête de lecture",follow}});
        if(choice==0){const auto outs=host.audioOutputs();std::vector<MenuItem> items{{"Sortie Windows par défaut",host.audioOutput()<0}};for(size_t i=0;i<outs.size();++i)items.push_back({outs[i],int(i)==host.audioOutput()});const int pick=host.popup(items);if(pick>=0)host.selectAudioOutput(pick-1);}
        else if(choice==1)deviceMenu(host.midiInputs(),host.midiInput(),"Aucune entrée MIDI",[&](int i){host.selectMidiInput(i);});
        else if(choice==2)deviceMenu(host.midiOutputs(),host.midiOutput(),"Aucune sortie MIDI",[&](int i){host.selectMidiOutput(i);});
        else if(choice==4){follow=!follow;saveSettings();}
    }
    void trackMenu(int t){
        selectTrack(t);const bool midi=isMidi(t);const auto& seq=snap->sequences[snap->sequence];
        const int choice=host.popup({{"Piste SON (sample)",!midi},{"Piste MIDI (vers Ableton)",midi},{"",false,true,true},{"Charger un sample…",false,!midi},{"Canal MIDI…",false,midi},{"",false,true,true},{"Muet",seq.tracks[t].mute},{"Solo",seq.tracks[t].solo},{"",false,true,true},{"Copier la piste"},{"Coller la piste",false,trackClipboard!=nullptr},{"Effacer les pas de la piste"}});
        TrigTrack config=current();
        switch(choice){
        case 0:config.type=TrigTrackType::Audio;engine.trigSetTrackConfig(t,config);changed();break;
        case 1:config.type=TrigTrackType::Midi;engine.trigSetTrackConfig(t,config);if(ui.page>1)ui.page=0;changed();status("Piste "+std::to_string(t+1)+" en MIDI : choisis la sortie MIDI dans Réglages (loopMIDI pour Ableton).");break;
        case 3:loadSample(t);break;
        case 4:channelMenu(t);break;
        case 6:engine.setTrackMute(t,!seq.tracks[t].mute);changed();break;
        case 7:engine.setTrackSolo(t,!seq.tracks[t].solo);changed();break;
        case 9:trackClipboard=std::make_unique<TrigTrack>(current());status("Piste copiée.");break;
        case 10:if(trackClipboard){engine.trigSetTrack(t,*trackClipboard);changed();status("Piste collée.");}break;
        case 11:engine.trigClearTrack(t);ui.latched.clear();changed();status("Pas de la piste effacés.");break;
        default:break;
        }
        refresh();
    }
    void channelMenu(int t){std::vector<MenuItem> items;for(int c=0;c<16;++c)items.push_back({"Canal "+std::to_string(c+1),track(t).midiChannel==c});const int choice=host.popup(items);if(choice<0)return;TrigTrack config=track(t);config.midiChannel=uint8_t(choice);engine.trigSetTrackConfig(t,config);changed();refresh();}
    void loadSample(int t){const auto chosen=host.openPath("Charger un sample WAV","wav");if(!chosen.empty())loadSampleFile(t,chosen);}
    void loadSampleFile(int t,const std::string& path){
        std::string error;if(isMidi(t)){TrigTrack config=track(t);config.type=TrigTrackType::Audio;engine.trigSetTrackConfig(t,config);}
        if(engine.load(t,path,error)){changed();status("Piste "+std::to_string(t+1)+" : "+fileStem(path));}else status("WAV refusé : "+error);refresh();
    }
    void lengthMenu(){
        static const int lengths[]={4,6,8,12,16,24,32,48,64};std::vector<MenuItem> items;for(int l:lengths)items.push_back({std::to_string(l)+" pas",current().length==l});
        const int choice=host.popup(items);if(choice<0)return;setTrackLength(lengths[choice]);
    }
    void setTrackLength(int length){TrigTrack config=current();config.length=uint8_t(std::clamp(length,1,TrigMaxSteps));engine.trigSetTrackConfig(ui.selectedTrack,config);ui.latched.erase(std::remove_if(ui.latched.begin(),ui.latched.end(),[&](int s){return s>=config.length;}),ui.latched.end());changed();refresh();ui.stepPage=std::min(ui.stepPage,pages()-1);}
    void speedMenu(){std::vector<MenuItem> items;for(int s=0;s<TrigSpeeds;++s)items.push_back({std::string("Vitesse ")+trigSpeedName(s),current().speed==s});const int choice=host.popup(items);if(choice<0)return;TrigTrack config=current();config.speed=uint8_t(choice);engine.trigSetTrackConfig(ui.selectedTrack,config);changed();refresh();}
    void effectMenu(int slot){
        const auto& catalog=insertEffectCatalog();std::vector<MenuItem> items{{"Aucun effet"}};const auto currentEffect=snap->edits[ui.selectedTrack].inserts[size_t(slot)].effect;
        for(const auto& info:catalog)items.push_back({std::string(info.category)+" · "+info.name,info.effect==currentEffect});
        const int choice=host.popup(items);if(choice<0)return;
        engine.setInsertEffect(ui.selectedTrack,slot,choice==0?InsertEffect::None:catalog[size_t(choice-1)].effect);engine.setInsertBypass(ui.selectedTrack,slot,false);
        // Old locks of this slot belong to the previous effect: remove them.
        for(int s=0;s<TrigMaxSteps;++s)for(int p=0;p<6;++p)if(current().steps[size_t(s)].findLock(trigFxParam(slot,p))>=0)engine.trigClearLock(ui.selectedTrack,s,trigFxParam(slot,p));
        changed();refresh();
    }
    void patternMenu(int index){
        const int choice=host.popup({{"Copier le pattern "+patternName(index)},{"Coller dans le pattern "+patternName(index),false,patternClipboard!=nullptr},{"Effacer le pattern "+patternName(index)}});
        if(choice<0)return;if(index!=snap->sequence){engine.selectSequence(index);refresh();}
        if(choice==0){patternClipboard=std::make_unique<TrigPattern>(pattern());status("Pattern "+patternName(index)+" copié.");}
        else if(choice==1&&patternClipboard){for(int t=0;t<TrigTracks;++t)engine.trigSetTrack(t,patternClipboard->tracks[size_t(t)]);engine.trigSetMasterLength(patternClipboard->masterLength);engine.trigSetSwing(patternClipboard->swing);changed();status("Collé dans "+patternName(index)+".");}
        else if(choice==2){for(int t=0;t<TrigTracks;++t)engine.trigClearTrack(t);ui.latched.clear();changed();status("Pattern "+patternName(index)+" effacé.");}
        refresh();
    }
    void songMenu(){
        const int choice=host.popup({{"Ajouter "+patternName(snap->sequence)+" ×1"},{"Ajouter "+patternName(snap->sequence)+" ×2"},{"Ajouter "+patternName(snap->sequence)+" ×4"},{"Retirer le dernier",false,snap->songLength>0},{"Effacer le song",false,snap->songLength>0},{"",false,true,true},{"Jouer le song",false,snap->songLength>0}});
        if(choice>=0&&choice<=2)engine.addSongStep(snap->sequence,choice==0?1:choice==1?2:4);
        else if(choice==3)engine.deleteSongStep(snap->songLength-1);
        else if(choice==4)engine.clearSong();
        else if(choice==6){ui.latched.clear();engine.playSong();status("Song : chaque pattern dure sa longueur maître (16 pas si INF).");}
        if(choice>=0&&choice!=6)changed();refresh();
    }
    void kitMenu(){
        const int program=snap->program;const bool own=program!=0;
        const int choice=host.popup({{"Kit propre à ce pattern (copie du kit actuel)",false,!own},{"Revenir au kit A",false,own},{"",false,true,true},{"Charger un kit…"},{"Enregistrer le kit…"}});
        if(choice==0){const int copy=engine.duplicateProgram(program,"Kit "+patternName(snap->sequence));if(copy>=0){engine.assignProgramToSequence(snap->sequence,copy);changed();status("Le pattern "+patternName(snap->sequence)+" a maintenant son propre kit.");}else status("Plus de place pour un nouveau kit (16 maximum).");}
        else if(choice==1){engine.assignProgramToSequence(snap->sequence,0);changed();}
        else if(choice==3||choice==4){const bool load=choice==3;const auto chosen=load?host.openPath("Charger un kit","kit"):host.savePath("Enregistrer le kit","kit","kit.kickkit");if(!chosen.empty()){std::string error;const bool ok=load?engine.openKit(chosen,(base/"samples").u8string(),error):engine.saveKit(chosen,(base/"samples").u8string(),error);status(ok?(load?"Kit chargé.":"Kit enregistré."):"Impossible : "+error);if(ok&&load)changed();}}
        refresh();
    }
    void macroMenu(int index){
        auto& m=macros[size_t(index)];const bool canAssign=lastParam>=0&&lastTrack>=0;
        const std::string target=canAssign?"T"+std::to_string(lastTrack+1)+" "+trigParamName(lastParam,&snap->edits[lastTrack]):"(touche d'abord un potard)";
        const int choice=host.popup({{"Assigner à "+target,false,canAssign},{"MIDI Learn (tourne un potard de ton contrôleur)"},{"Désassigner",false,m.track>=0}});
        if(choice==0){m.track=lastTrack;m.param=lastParam;const auto& edit=snap->edits[size_t(lastTrack)];m.value=lastParam>=TrigCcFirst?std::max(0,int(track(lastTrack).ccValue[size_t(lastParam-TrigCcFirst)]))/127.:trigParamBase(edit,lastParam);status("M"+std::to_string(index+1)+" → "+target);}
        else if(choice==1){learnMacro=index;status("MIDI Learn M"+std::to_string(index+1)+" : tourne un potard de ton contrôleur.");}
        else if(choice==2){m=Macro{};}
        saveSettings();
    }

    // ----- transport -----
    void togglePlay(){if(snap->playing){engine.stop();}else{engine.setPlaying(true);}refresh();}
    void stop(){engine.stop();refresh();}
    void selectPattern(int index){
        if(snap->playing){engine.queueSequence(index);status("Pattern "+patternName(index)+" en attente : il démarre à la fin du cycle.");}
        else{engine.selectSequence(index);ui.latched.clear();}
        refresh();
    }
    void toggleLatch(int step){
        auto it=std::find(ui.latched.begin(),ui.latched.end(),step);
        if(it==ui.latched.end())ui.latched.push_back(step);else ui.latched.erase(it);
        if(!ui.latched.empty()&&ui.page!=0&&false)ui.page=0;
        const auto& st=current().steps[size_t(step)];
        status(ui.latched.empty()?"Sélection relâchée.":std::to_string(ui.latched.size())+" pas sélectionné(s) : tourne un potard pour poser un lock"+(st.trig?"":" (pas vide = trigless lock)")+". Échap pour relâcher.");
    }
    void clearLatchedLocks(){for(int s:ui.latched)engine.trigClearLocks(ui.selectedTrack,s);if(!ui.latched.empty()){changed();status("Locks effacés.");}refresh();}

    // ----- painting -----
    void button(Painter& p,const Rect& r,const std::string& label,bool active,Color color=Blue,int size=15,bool enabled=true){
        p.round(r.x,r.y,r.w,r.h,7,active?color:(enabled?Panel2:Panel));
        if(!active)p.roundFrame(r.x,r.y,r.w,r.h,7,enabled?Line:Panel2);
        p.text(r.x,r.y,r.w,r.h,label,size,active?Bg:(enabled?Ink:Dim),Align::Center,true);
    }
    void valueBox(Painter& p,const Rect& r,const std::string& caption,const std::string& value,Color accent=Ink){
        p.round(r.x,r.y,r.w,r.h,7,Panel2);p.roundFrame(r.x,r.y,r.w,r.h,7,Line);
        p.text(r.x+8,r.y+3,r.w-16,16,caption,11,Muted,Align::Left,true);
        p.text(r.x+8,r.y+17,r.w-16,r.h-19,value,17,accent,Align::Left,true);
    }
    void drawKnob(Painter& p,const Rect& r,const Knob& k,bool hot){
        p.round(r.x,r.y,r.w,r.h,10,hot?Panel2:Panel);
        if(!k.kind){p.roundFrame(r.x,r.y,r.w,r.h,10,Panel2);return;}
        const Color mark=k.scene?Green:Red;const bool marked=k.locked||k.scene;
        p.roundFrame(r.x,r.y,r.w,r.h,10,marked?mark:Line,marked?2:1);
        p.text(r.x+6,r.y+10,r.w-12,20,k.label,13,k.enabled?(marked?mark:Muted):Dim,Align::Center,true);
        const bool large=r.h>180;const int cx=r.x+r.w/2,cy=r.y+(large?92:70),radius=large?42:32;
        p.arc(cx,cy,radius,135,270,Line,8);
        if(k.enabled){const Color c=marked?mark:(ui.page==0?Yellow:Blue);p.arc(cx,cy,radius,135,std::max(1.,270*k.value),c,8);
            const double angle=(135+270*k.value)*3.14159265/180.;p.line(cx,cy,cx+int(std::cos(angle)*(radius-14)),cy+int(std::sin(angle)*(radius-14)),Ink,3);}
        p.disc(cx,cy,7,k.enabled?Ink:Dim);
        p.text(r.x+4,r.y+r.h-(large?50:44),r.w-8,24,k.display,large?16:14,k.enabled?Ink:Dim,Align::Center,true);
        if(k.locked)p.text(r.x+4,r.y+r.h-26,r.w-8,18,"LOCK · clic droit = retirer",10,Red,Align::Center);
        else if(k.scene)p.text(r.x+4,r.y+r.h-26,r.w-8,18,"SCÈNE · clic droit = retirer",10,Green,Align::Center);
        else if(k.ccSlot>=0&&large)p.text(r.x+4,r.y+r.h-26,r.w-8,18,"molette sur le titre = n° CC",10,Dim,Align::Center);
    }
    void paintScenes(Painter& p){
        const auto& pat=pattern();
        p.text(MainX+262,MacroY,946,28,editScene>=0?"SCÈNE "+std::to_string(editScene+1)+" EN ÉDITION : les potards de toutes les pistes écrivent dans la scène":"clic = éditer une scène · clic droit = A / B, copier, effacer · le crossfader passe de A à B",11,editScene>=0?Green:Muted,Align::Left,editScene>=0);
        for(int i=0;i<TrigScenes;++i){
            const auto r=sceneRect(i);const bool editing=editScene==i;const auto& sc=pat.scenes[size_t(i)];
            button(p,r,std::to_string(i+1),editing,Green,15);
            if(sc.count)p.text(r.x+4,r.y+r.h-15,r.w-8,13,std::to_string(sc.count)+" val.",9,editing?Bg:Muted,Align::Center);
            if(pat.sceneA==i){p.round(r.x+3,r.y+3,16,16,4,Blue);p.text(r.x+3,r.y+3,16,16,"A",10,Bg,Align::Center,true);}
            if(pat.sceneB==i){p.round(r.x+r.w-19,r.y+3,16,16,4,Yellow);p.text(r.x+r.w-19,r.y+3,16,16,"B",10,Bg,Align::Center,true);}
        }
        const auto& f=Crossfader;const double x=std::clamp(snap->crossfader,0.,1.);
        p.text(MainX,f.y,84,f.h,"A · "+std::to_string(pat.sceneA+1),15,Blue,Align::Left,true);
        p.text(f.x+f.w+6,f.y,84,f.h,std::to_string(pat.sceneB+1)+" · B",15,Yellow,Align::Right,true);
        p.round(f.x,f.y+16,f.w,8,4,Line);if(x>0)p.round(f.x,f.y+16,std::max(8,int(f.w*x)),8,4,mix(Blue,Yellow,x));
        const int hx=f.x+int(f.w*x);p.round(hx-14,f.y,28,f.h,6,Ink);p.fill(hx-1,f.y+8,2,f.h-16,Bg);
        std::string info="CROSSFADER "+std::to_string(int(std::lround(x*100)))+" %";if(faderCc>=0)info+=" · CC "+std::to_string(faderCc);if(learnFader)info+=" · LEARN : tourne un potard";
        p.text(MainX,f.y+f.h+6,MainW,20,info+" · glisser ou molette · clic droit = MIDI Learn",11,Muted,Align::Center);
    }
    void sceneMenu(int index){
        const auto& pat=pattern();const bool has=pat.scenes[size_t(index)].count>0;
        const int choice=host.popup({{"Mettre en A (crossfader à gauche)",pat.sceneA==index},{"Mettre en B (crossfader à droite)",pat.sceneB==index},{"",false,true,true},{"Copier la scène",false,has},{"Coller dans la scène "+std::to_string(index+1),false,sceneClipboard!=nullptr},{"Effacer la scène",false,has}});
        if(choice==0)engine.trigSetSceneAB(index,pat.sceneB);else if(choice==1)engine.trigSetSceneAB(pat.sceneA,index);
        else if(choice==3)sceneClipboard=std::make_unique<TrigScene>(pat.scenes[size_t(index)]);
        else if(choice==4&&sceneClipboard){engine.trigClearScene(index);for(int i=0;i<sceneClipboard->count;++i){const auto& l=sceneClipboard->locks[size_t(i)];engine.trigSetSceneValue(index,l.track,l.param,l.value/double(TrigLockResolution));}}
        else if(choice==5)engine.trigClearScene(index);
        if(choice>=0&&choice!=3)changed();refresh();
    }
    void copySteps(){
        stepClipboard.clear();if(ui.latched.empty())return;auto sorted=ui.latched;std::sort(sorted.begin(),sorted.end());
        for(int s:sorted)stepClipboard.emplace_back(s-sorted.front(),current().steps[size_t(s)]);status(std::to_string(stepClipboard.size())+" pas copiés (avec leurs locks).");
    }
    void pasteSteps(){
        if(stepClipboard.empty()||ui.latched.empty())return;const int start=*std::min_element(ui.latched.begin(),ui.latched.end());int pasted=0;
        for(const auto& entry:stepClipboard){const int at=start+entry.first;if(at>=current().length)break;engine.trigSetStep(ui.selectedTrack,at,entry.second);++pasted;}
        changed();refresh();status(std::to_string(pasted)+" pas collés à partir du pas "+std::to_string(start+1)+".");
    }
    void copyPage(){pageClipboard=std::make_unique<std::array<TrigStep,16>>();for(int i=0;i<16;++i)(*pageClipboard)[size_t(i)]=current().steps[size_t(ui.stepPage*16+i)];status("Page "+std::to_string(ui.stepPage+1)+" copiée.");}
    void pastePage(){if(!pageClipboard)return;for(int i=0;i<16;++i){const int at=ui.stepPage*16+i;if(at>=current().length)break;engine.trigSetStep(ui.selectedTrack,at,(*pageClipboard)[size_t(i)]);}changed();refresh();status("Page collée sur les pas "+std::to_string(ui.stepPage*16+1)+"–"+std::to_string(ui.stepPage*16+16)+".");}
    void setFader(double v){engine.setCrossfader(v);refresh();}
    void paint(Painter& p){
        refresh();const auto& pat=pattern();const auto& seq=snap->sequences[snap->sequence];const int sel=ui.selectedTrack;const auto& tr=current();
        p.fill(0,0,DesignWidth,DesignHeight,Bg);
        // Header
        p.text(16,8,176,36,"ELEKTAKT",25,Ink,Align::Left,true);p.text(20,42,170,18,"ANKS LABS · SÉQUENCEUR",10,Blue,Align::Left,true);
        button(p,HeaderPlay,"▶ PLAY",snap->playing,Green,15);
        button(p,HeaderStop,"■ STOP",false);
        button(p,HeaderRec,"● REC",record,Red);
        char text[64];std::snprintf(text,sizeof(text),"%.1f",snap->tempo);valueBox(p,HeaderTempo,"TEMPO BPM",text);
        valueBox(p,HeaderSwing,"SWING",std::to_string(pat.swing)+" %");
        valueBox(p,HeaderMaster,"LONG. MAÎTRE",pat.masterLength==0?"INF":std::to_string(pat.masterLength)+" pas");
        button(p,HeaderFill,"FILL",snap->fill,Yellow);
        button(p,HeaderClock,"CLOCK OUT",snap->midiClockOut,Blue,13);
        const auto outs=host.midiOutputs();const int out=host.midiOutput();
        valueBox(p,HeaderMidiOut,"SORTIE MIDI (vers Ableton)",out>=0&&out<int(outs.size())?outs[size_t(out)]:"aucune — clic pour choisir",out>=0?Blue:Muted);
        button(p,HeaderFile,"FICHIER",false);button(p,HeaderSettings,"RÉGLAGES",false);
        // Patterns
        p.text(16,PatternY,90,PatternH,"PATTERN",13,Muted,Align::Left,true);
        for(int i=0;i<16;++i){
            const auto r=patternRect(i);const bool active=snap->sequence==i,queued=snap->queuedSequence==i;bool content=false;
            for(const auto& t:snap->sequences[size_t(i)].trig.tracks){for(const auto& st:t.steps)if(!st.empty()){content=true;break;}if(content)break;}
            button(p,r,patternName(i),active,Blue,16);
            if(queued&&(tickCount/8)%2==0)p.roundFrame(r.x-2,r.y-2,r.w+4,r.h+4,8,Yellow,2);
            if(content&&!active)p.disc(r.x+r.w-9,r.y+9,3,Blue);
        }
        std::string chain;for(int i=0;i<snap->songLength;++i){if(i)chain+=" ";chain+=patternName(snap->song[size_t(i)].sequence)+"×"+std::to_string(snap->song[size_t(i)].repeats);}
        button(p,PatternSong,snap->songPlaying?"SONG ▶":"SONG",snap->songPlaying,Green,13);
        button(p,PatternKit,snap->program==0?"KIT A":"KIT "+patternName(snap->sequence),false,Blue,13);
        button(p,PatternMenu,"COPIER…",false,Blue,13);
        p.text(1354,PatternY,230,PatternH,chain.empty()?"song vide":chain,11,Muted,Align::Left);
        // Tracks
        bool anySolo=false;for(const auto& t:seq.tracks)anySolo=anySolo||t.solo;
        for(int t=0;t<TrigTracks;++t){
            const auto r=trackRect(t);const bool selected=t==sel,midi=isMidi(t);const Color typeColor=midi?Yellow:Blue;
            const bool silent=seq.tracks[t].mute||(anySolo&&!seq.tracks[t].solo);
            p.round(r.x,r.y,r.w,r.h,8,selected?Panel2:Panel);if(selected)p.roundFrame(r.x,r.y,r.w,r.h,8,typeColor,2);
            const auto play=trackPlay(t);p.round(play.x,play.y,play.w,play.h,6,snap->trigFlash[t]?typeColor:mix(Panel2,typeColor,.25));
            p.text(play.x,play.y,play.w,play.h,std::to_string(t+1),14,snap->trigFlash[t]?Bg:Ink,Align::Center,true);
            p.text(r.x+46,r.y+4,190,20,trackName(t),14,silent?Dim:Ink,Align::Left,true);
            const auto& tt=track(t);p.text(r.x+46,r.y+23,200,16,std::string(midi?"MIDI":"SON")+" · "+std::to_string(tt.length)+" pas · "+trigSpeedName(tt.speed),11,midi?Yellow:Muted,Align::Left);
            button(p,trackMute(t),"M",seq.tracks[t].mute,Red,13);button(p,trackSolo(t),"S",seq.tracks[t].solo,Yellow,13);
        }
        // Track header
        p.text(MainX,124,320,28,"PISTE "+std::to_string(sel+1)+" · "+trackName(sel),20,Ink,Align::Left,true);
        p.text(MainX,152,320,20,isMidi(sel)?"Clic droit sur une piste : type, canal, copier…":"Glisse un WAV sur la piste ou clic droit → Charger",11,Muted,Align::Left);
        {const Rect a{TrackType.x,TrackType.y,TrackType.w/2,TrackType.h},b{TrackType.x+TrackType.w/2,TrackType.y,TrackType.w/2,TrackType.h};button(p,a,"SON",!isMidi(sel),Blue,13);button(p,b,"MIDI",isMidi(sel),Yellow,13);}
        valueBox(p,TrackLength,"LONGUEUR",std::to_string(tr.length)+" pas");
        valueBox(p,TrackSpeed,"VITESSE",trigSpeedName(tr.speed));
        if(isMidi(sel))valueBox(p,TrackChannel,"CANAL MIDI",std::to_string(tr.midiChannel+1),Yellow);
        else valueBox(p,TrackSample,"SAMPLE (clic = charger)",trackName(sel));
        button(p,TrackCopy,"COPIER",false,Blue,12);button(p,TrackPaste,"COLLER",false,Blue,12,trackClipboard!=nullptr);button(p,TrackClear,"EFFACER",false,Red,12);
        // Step pages
        const int playStep=snap->trigPlayStep[sel];
        for(int page=0;page<4;++page){const auto r=stepPageRect(page);const bool usable=page<pages();button(p,r,std::to_string(page*16+1)+"–"+std::to_string(page*16+16),ui.stepPage==page,Blue,12,usable);if(playStep>=0&&playStep/16==page)p.fill(r.x+8,r.y+r.h-4,r.w-16,3,Green);}
        button(p,FollowRect,"SUIVRE",follow,Green,12);
        button(p,PageCopy,"COPIER PAGE",false,Blue,11);button(p,PagePaste,"COLLER PAGE",false,Blue,11,pageClipboard!=nullptr);
        p.text(MainX+762,PagesY,446,30,"Clic = trig · Clic droit = sélection pour lock · Ctrl+Z annuler",11,Muted,Align::Left);
        // Steps
        const Color trigColor=isMidi(sel)?Yellow:Blue;
        for(int i=0;i<16;++i){
            const int step=ui.stepPage*16+i;const auto r=stepRect(i);const bool exists=step<tr.length;const auto& st=tr.steps[size_t(step)];
            const bool latched=std::find(ui.latched.begin(),ui.latched.end(),step)!=ui.latched.end();const bool head=snap->playing&&playStep==step;
            Color fillColor=!exists?Bg:(st.trig?trigColor:Panel2);
            if(st.trig&&st.condition!=TrigCondNone)fillColor=mix(trigColor,Yellow,.35);
            p.round(r.x,r.y,r.w,r.h,9,fillColor);
            if(!exists){p.roundFrame(r.x,r.y,r.w,r.h,9,Panel);continue;}
            if(!st.trig&&st.lockCount>0){p.roundFrame(r.x+4,r.y+4,r.w-8,r.h-8,7,Red,2);}
            p.roundFrame(r.x,r.y,r.w,r.h,9,latched?White:(i%4==0?Dim:Line),latched?3:1);
            const Color label=st.trig?Bg:Muted;
            p.text(r.x+8,r.y+6,40,18,std::to_string(step+1),12,label,Align::Left,true);
            if(st.lockCount>0){p.disc(r.x+r.w-14,r.y+14,9,Red);p.text(r.x+r.w-23,r.y+5,18,18,std::to_string(st.lockCount),10,White,Align::Center,true);}
            std::string info;if(st.trig){if(st.condition!=TrigCondNone)info=trigConditionName(st.condition);else if(st.probability<100)info=std::to_string(st.probability)+"%";}
            if(!info.empty())p.text(r.x+4,r.y+r.h-28,r.w-8,20,info,13,label,Align::Center,true);
            std::string flags;if(st.slide)flags+="SLIDE ";if(st.trig&&st.retrig>1)flags+="x"+std::to_string(st.retrig);if(st.trig&&isMidi(sel)&&st.note>=0){if(!flags.empty())flags+=" ";flags+=noteName(st.note);}
            if(!flags.empty())p.text(r.x+4,r.y+30,r.w-8,18,flags,11,label,Align::Center,true);
            if(st.micro!=0){const int x=r.x+r.w/2+st.micro*(r.w/2-6)/23;p.fill(x-1,r.y+r.h-6,3,5,st.trig?Bg:Orange);}
            if(head)p.fill(r.x,r.y+r.h+5,r.w,5,Green);
        }
        // Latch info
        if(ui.latched.empty())p.text(LatchInfo.x,LatchInfo.y,LatchInfo.w+500,LatchInfo.h,"Aucun pas sélectionné : les potards règlent le son de la piste. Sélectionne des pas (clic droit) pour poser des parameter locks.",12,Muted,Align::Left);
        else{
            int locks=0;for(int s:ui.latched)locks+=tr.steps[size_t(s)].lockCount;
            std::string list;for(size_t i=0;i<ui.latched.size()&&i<8;++i){if(i)list+=", ";list+=std::to_string(ui.latched[i]+1);}if(ui.latched.size()>8)list+="…";
            p.text(LatchInfo.x,LatchInfo.y,LatchInfo.w,LatchInfo.h,editScene>=0?"Pas "+list+" sélectionnés — les potards écrivent dans la SCÈNE "+std::to_string(editScene+1):"SÉLECTION : pas "+list+" · "+std::to_string(locks)+" lock(s) → potards = LOCKS",13,editScene>=0?Green:Red,Align::Left,true);
            button(p,LatchCopy,"COPIER LES PAS",false,Blue,12);button(p,LatchPaste,"COLLER ICI",false,Blue,12,!stepClipboard.empty());
            button(p,LatchClear,"EFFACER LOCKS",false,Red,12);button(p,LatchRelease,"RELÂCHER (Échap)",false,Blue,12);
        }
        // Tabs
        const auto names=pageNames();int tabX=MainX;
        for(int i=0;i<int(names.size());++i){const int w=i==0?92:(i<3?86:(names[size_t(i)].size()>4?150:86));const Rect r{tabX,TabY,w,TabH};button(p,r,names[size_t(i)],ui.page==i,i==0?Yellow:Blue,12);tabX+=w+6;}
        if(!isMidi(sel)&&ui.page>=3){const int slot=ui.page-3;const auto& insert=snap->edits[sel].inserts[size_t(slot)];
            const Rect fx{std::max(tabX+10,FxEffect.x),FxEffect.y,FxEffect.w-(std::max(tabX+10,FxEffect.x)-FxEffect.x),FxEffect.h};
            button(p,fx,insert.effect==InsertEffect::None?"Choisir un effet…":std::string("Effet : ")+insertEffectName(insert.effect)+" ▾",false,Blue,12);
            button(p,FxBypass,insert.bypass?"BYPASS":"ACTIF",!insert.bypass,Green,12);}
        // Knobs
        const auto list=knobs();for(int i=0;i<8;++i)drawKnob(p,knobRect(i),list[size_t(i)],dragKnob==i||knobRect(i).in(mouseX,mouseY));
        if(ui.page==0&&ui.latched.empty())p.text(MainX,KnobY+KnobH+2,MainW,16,"Sans sélection : NOTE, VÉLOCITÉ et LONGUEUR sont les valeurs des nouveaux trigs de la piste. Sélectionne un pas pour régler sa probabilité, sa condition, son micro-timing et son retrig.",10,Muted,Align::Left);
        else if(ui.page==0)p.text(MainX,KnobY+KnobH+2,MainW,16,"CONDITION : 1:4 = joue au 1er passage sur 4 · FILL = seulement avec FILL · PRE/NEI = selon le trig précédent / la piste voisine · 1ST = premier passage",10,Muted,Align::Left);
        // Macros / scenes
        button(p,ViewMacros,"MACROS",!sceneView,Blue,12);button(p,ViewScenes,"SCÈNES",sceneView,Green,12);
        if(sceneView)paintScenes(p);
        else{
        p.text(MainX+262,MacroY,946,28,"touche un potard puis clic droit sur une macro pour l'assigner · MIDI Learn pour ton LPD8 / MPK",11,Muted,Align::Left);
        for(int i=0;i<8;++i){
            const auto r=macroRect(i);const auto& m=macros[size_t(i)];Knob k;k.kind=1;k.enabled=m.track>=0;k.value=m.value;
            k.label="M"+std::to_string(i+1)+(m.track>=0?" · T"+std::to_string(m.track+1)+" "+trigParamName(m.param,&snap->edits[size_t(m.track)]):"");
            k.display=m.track>=0?trigParamDisplay(m.param,m.value):"non assignée";if(learnMacro==i){k.display="LEARN…";}
            if(m.midiCc>=0)k.label+=" ·CC"+std::to_string(m.midiCc);
            Rect small{r.x,r.y,r.w,r.h};drawKnob(p,small,k,dragMacro==i);
        }
        }
        // Status bar
        p.fill(0,DesignHeight-30,DesignWidth,30,Panel);p.line(0,DesignHeight-30,DesignWidth,DesignHeight-30,Line);
        p.text(16,DesignHeight-30,1180,30,ui.status,12,Ink,Align::Left);
        std::string right=std::string(record?"REC armé · ":"")+(snap->songPlaying?"SONG · ":"")+"Espace = play/stop · F = fill · ←/→ pages · ↑/↓ pistes";
        p.text(1000,DesignHeight-30,584,30,right,11,Muted,Align::Right);
    }

    // ----- input -----
    int tabAt(int x,int y)const{
        if(y<TabY||y>=TabY+TabH)return -1;const auto names=pageNames();int tabX=MainX;
        for(int i=0;i<int(names.size());++i){const int w=i==0?92:(i<3?86:(names[size_t(i)].size()>4?150:86));if(x>=tabX&&x<tabX+w)return i;tabX+=w+6;}
        return -1;
    }
    int stepAt(int x,int y)const{for(int i=0;i<16;++i)if(stepRect(i).in(x,y))return ui.stepPage*16+i;return -1;}
    void mouseDown(int x,int y,int button,int modifiers){
        refresh();mouseX=x;mouseY=y;const bool right=button==1;const int sel=ui.selectedTrack;gestureOpen=!right;gesturePushed=false;gesture=right?"":"mouse";
        if(!right){
            if(HeaderPlay.in(x,y)){togglePlay();return;}
            if(HeaderStop.in(x,y)){stop();return;}
            if(HeaderRec.in(x,y)){record=!record;status(record?"REC armé : joue avec ton clavier MIDI ou les numéros de piste pendant la lecture.":"REC désarmé.");return;}
            if(HeaderFill.in(x,y)){fillMouse=true;engine.setFill(true);return;}
            if(HeaderClock.in(x,y)){engine.setMidiClockOut(!snap->midiClockOut);refresh();status(snap->midiClockOut?"Horloge MIDI envoyée : dans Ableton, active Sync sur l'entrée loopMIDI.":"Horloge MIDI coupée.");return;}
            if(HeaderMidiOut.in(x,y)){deviceMenu(host.midiOutputs(),host.midiOutput(),"Aucune sortie MIDI",[&](int i){host.selectMidiOutput(i);});return;}
            if(HeaderFile.in(x,y)){fileMenu();return;}
            if(HeaderSettings.in(x,y)){settingsMenu();return;}
            if(HeaderTempo.in(x,y)){dragHeader=0;dragStart=snap->tempo;dragStartY=y;return;}
            if(HeaderSwing.in(x,y)){dragHeader=1;dragStart=pattern().swing;dragStartY=y;return;}
            if(HeaderMaster.in(x,y)){dragHeader=2;dragStart=pattern().masterLength;dragStartY=y;return;}
        }
        for(int i=0;i<16;++i)if(patternRect(i).in(x,y)){if(right)patternMenu(i);else selectPattern(i);return;}
        if(PatternSong.in(x,y)){songMenu();return;}
        if(PatternKit.in(x,y)){kitMenu();return;}
        if(PatternMenu.in(x,y)){patternMenu(snap->sequence);return;}
        for(int t=0;t<TrigTracks;++t){
            if(!trackRect(t).in(x,y))continue;
            if(right){trackMenu(t);return;}
            if(trackMute(t).in(x,y)){engine.setTrackMute(t,!snap->sequences[snap->sequence].tracks[t].mute);changed();refresh();return;}
            if(trackSolo(t).in(x,y)){engine.setTrackSolo(t,!snap->sequences[snap->sequence].tracks[t].solo);changed();refresh();return;}
            selectTrack(t);
            if(trackPlay(t).in(x,y)){engine.trigPlayLive(t,-1,track(t).defaultVelocity,true,record&&snap->playing);engine.trigPlayLive(t,-1,0,false,false);if(record&&snap->playing)changed();}
            refresh();return;
        }
        if(TrackType.in(x,y)){TrigTrack config=current();config.type=x<TrackType.x+TrackType.w/2?TrigTrackType::Audio:TrigTrackType::Midi;engine.trigSetTrackConfig(sel,config);if(isMidi(sel)&&ui.page>1)ui.page=0;changed();refresh();if(config.type==TrigTrackType::Midi&&ui.page>1)ui.page=0;return;}
        if(TrackLength.in(x,y)){if(right)lengthMenu();else{dragHeader=3;dragStart=current().length;dragStartY=y;}return;}
        if(TrackSpeed.in(x,y)){speedMenu();return;}
        if(isMidi(sel)&&TrackChannel.in(x,y)){channelMenu(sel);return;}
        if(!isMidi(sel)&&TrackSample.in(x,y)){loadSample(sel);return;}
        if(TrackCopy.in(x,y)){trackClipboard=std::make_unique<TrigTrack>(current());status("Piste copiée.");return;}
        if(TrackPaste.in(x,y)&&trackClipboard){engine.trigSetTrack(sel,*trackClipboard);changed();refresh();status("Piste collée.");return;}
        if(TrackClear.in(x,y)){engine.trigClearTrack(sel);ui.latched.clear();changed();refresh();status("Pas de la piste effacés.");return;}
        for(int page=0;page<4;++page)if(stepPageRect(page).in(x,y)){if(page<pages())ui.stepPage=page;return;}
        if(FollowRect.in(x,y)){follow=!follow;saveSettings();return;}
        if(PageCopy.in(x,y)){copyPage();return;}
        if(PagePaste.in(x,y)){pastePage();return;}
        const int step=stepAt(x,y);
        if(step>=0){
            if(step>=current().length)return;
            if(right||(modifiers&ModShift)){toggleLatch(step);return;}
            const bool newValue=!current().steps[size_t(step)].trig;engine.trigToggle(sel,step);paintStep=step;paintValue=newValue;
            if(!newValue){auto it=std::find(ui.latched.begin(),ui.latched.end(),step);if(it!=ui.latched.end()&&current().steps[size_t(step)].lockCount==0)ui.latched.erase(it);}
            changed();refresh();return;
        }
        if(!ui.latched.empty()&&LatchClear.in(x,y)){clearLatchedLocks();return;}
        if(!ui.latched.empty()&&LatchCopy.in(x,y)){copySteps();return;}
        if(!ui.latched.empty()&&LatchPaste.in(x,y)){pasteSteps();return;}
        if(!ui.latched.empty()&&LatchRelease.in(x,y)){ui.latched.clear();status("Sélection relâchée.");return;}
        const int tab=tabAt(x,y);if(tab>=0){ui.page=tab;return;}
        if(!isMidi(sel)&&ui.page>=3){const int slot=ui.page-3;if(FxBypass.in(x,y)){engine.setInsertBypass(sel,slot,!snap->edits[sel].inserts[size_t(slot)].bypass);changed();refresh();return;}if(FxEffect.in(x,y)||(y>=TabY&&y<TabY+TabH&&x>FxEffect.x-200&&x<FxBypass.x)){effectMenu(slot);return;}}
        for(int i=0;i<8;++i){
            const auto r=knobRect(i);if(!r.in(x,y))continue;const auto list=knobs();const auto& k=list[size_t(i)];if(!k.kind||!k.enabled)return;
            if(right){
                if(k.scene){engine.trigClearSceneValue(editScene,sel,k.id);changed();refresh();status("Valeur retirée de la scène.");}
                else if(k.locked){for(int s:ui.latched)engine.trigClearLock(sel,s,k.id);changed();refresh();status("Lock retiré.");}
                else if(k.ccSlot>=0&&ui.latched.empty()){engine.trigSetCcBase(sel,k.ccSlot,-1);changed();refresh();status("Ce CC n'est plus envoyé.");}
                else if(k.kind==1){lastParam=k.id;lastTrack=sel;status("Potard mémorisé : clic droit sur une macro pour l'y assigner.");}
                return;
            }
            dragKnob=i;dragStart=k.value;dragStartY=y;if(k.kind==1){lastParam=k.id;lastTrack=sel;}return;
        }
        if(ViewMacros.in(x,y)){sceneView=false;return;}
        if(ViewScenes.in(x,y)){sceneView=true;return;}
        if(sceneView){
            for(int i=0;i<TrigScenes;++i)if(sceneRect(i).in(x,y)){if(right)sceneMenu(i);else{editScene=editScene==i?-1:i;status(editScene>=0?"Scène "+std::to_string(i+1)+" en édition : tourne les potards de n'importe quelle piste.":"Édition de scène terminée.");}return;}
            const Rect grab{Crossfader.x-20,Crossfader.y,Crossfader.w+40,Crossfader.h};
            if(grab.in(x,y)){if(right){learnFader=true;status("MIDI Learn du crossfader : tourne un potard de ton contrôleur.");}else{dragFader=true;setFader(double(x-Crossfader.x)/Crossfader.w);}return;}
            return;
        }
        for(int i=0;i<8;++i){const auto r=macroRect(i);if(!r.in(x,y))continue;if(right)macroMenu(i);else{dragMacro=i;dragStart=macros[size_t(i)].value;dragStartY=y;}return;}
        if(!right&&modifiers==0&&!ui.latched.empty()&&y>StepY+StepH&&y<TabY){ui.latched.clear();status("Sélection relâchée.");}
    }
    void mouseMove(int x,int y,int modifiers){
        mouseX=x;mouseY=y;const int dy=dragStartY-y;const bool fine=(modifiers&ModShift)!=0;
        if(dragKnob>=0){knobAbsolute(dragKnob,dragStart,dy,fine);return;}
        if(dragMacro>=0){setMacro(dragMacro,dragStart+dy/(fine?800.:200.));return;}
        if(dragFader){setFader(double(x-Crossfader.x)/Crossfader.w);return;}
        if(dragHeader==0){engine.setTempo(std::round((dragStart+dy*(fine?.05:.5))*10)/10);changed();return;}
        if(dragHeader==1){engine.trigSetSwing(int(std::lround(dragStart+dy/6.)));changed();return;}
        if(dragHeader==2){engine.trigSetMasterLength(int(std::lround(dragStart+dy/5.)));changed();return;}
        if(dragHeader==3){refresh();setTrackLength(int(std::lround(dragStart+dy/5.)));return;}
        if(paintStep>=0){const int step=stepAt(x,y);if(step>=0&&step!=paintStep&&step<current().length){refresh();if(current().steps[size_t(step)].trig!=paintValue){engine.trigToggle(ui.selectedTrack,step);changed();}paintStep=step;}}
    }
    void mouseUp(){if(fillMouse){fillMouse=false;engine.setFill(fillKey);}const bool macro=dragMacro>=0;dragKnob=dragMacro=dragHeader=paintStep=-1;dragFader=false;gestureOpen=false;gesture.clear();if(macro)saveSettings();}
    void wheel(int x,int y,int delta,int modifiers){
        refresh();gestureOpen=false;const bool fine=(modifiers&ModShift)!=0;gesture="wheel "+std::to_string(x/40)+","+std::to_string(y/40);
        if(sceneView&&Rect{Crossfader.x-20,Crossfader.y,Crossfader.w+40,Crossfader.h}.in(x,y)){setFader(snap->crossfader+delta*(fine?.01:.05));return;}
        if(HeaderTempo.in(x,y)){engine.setTempo(snap->tempo+delta*(fine?.1:1.));changed();return;}
        if(HeaderSwing.in(x,y)){engine.trigSetSwing(pattern().swing+delta);changed();return;}
        if(HeaderMaster.in(x,y)){engine.trigSetMasterLength(pattern().masterLength+delta);changed();return;}
        if(TrackLength.in(x,y)){setTrackLength(current().length+delta);return;}
        for(int i=0;i<8;++i){
            const auto r=knobRect(i);if(!r.in(x,y))continue;const auto list=knobs();const auto& k=list[size_t(i)];
            if(k.ccSlot>=0&&y<r.y+34){TrigTrack config=current();config.ccNumber[size_t(k.ccSlot)]=uint8_t(std::clamp(int(config.ccNumber[size_t(k.ccSlot)])+delta,0,127));engine.trigSetTrackConfig(ui.selectedTrack,config);changed();refresh();return;}
            knobDelta(i,delta*(fine?.25:1.));return;
        }
        for(int i=0;i<8;++i)if(macroRect(i).in(x,y)){setMacro(i,macros[size_t(i)].value+delta/(fine?400.:100.));saveSettings();return;}
        if(y>=StepY&&y<StepY+StepH){ui.stepPage=std::clamp(ui.stepPage-delta,0,pages()-1);return;}
        if(x<MainX&&y>=TrackY)selectTrack(ui.selectedTrack-delta);
    }
    void key(int code,bool down,int modifiers){
        refresh();gestureOpen=false;gesture.clear();
        if(code=='F'){fillKey=down;engine.setFill(down||fillMouse);return;}
        if(!down)return;
        if(modifiers&ModControl){
            if(code=='S'){saveProject((modifiers&ModShift)!=0);return;}
            if(code=='O'){openProject("project");return;}
            if(code=='N'){newProject();return;}
            if(code=='Z'){undo((modifiers&ModShift)!=0);return;}
            if(code=='Y'){undo(true);return;}
            if(code=='C'){if(!ui.latched.empty())copySteps();else{trackClipboard=std::make_unique<TrigTrack>(current());status("Piste copiée.");}return;}
            if(code=='V'){if(!ui.latched.empty()&&!stepClipboard.empty())pasteSteps();else if(trackClipboard){engine.trigSetTrack(ui.selectedTrack,*trackClipboard);changed();status("Piste collée.");}return;}
            return;
        }
        switch(code){
        case KeySpace:togglePlay();break;
        case KeyEscape:ui.latched.clear();if(editScene>=0){editScene=-1;status("Édition de scène terminée.");}else status("Sélection relâchée.");break;
        case KeyDelete:case KeyBack:clearLatchedLocks();break;
        case KeyLeft:ui.stepPage=std::max(0,ui.stepPage-1);break;
        case KeyRight:ui.stepPage=std::min(pages()-1,ui.stepPage+1);break;
        case KeyUp:selectTrack(ui.selectedTrack-1);break;
        case KeyDown:selectTrack(ui.selectedTrack+1);break;
        case 'R':record=!record;status(record?"REC armé.":"REC désarmé.");break;
        default:if(code>=KeyF1&&code<KeyF1+8)ui.page=std::min(code-KeyF1,int(pageNames().size())-1);break;
        }
    }
    void midiIn(unsigned packed){
        const int statusByte=packed&255,type=statusByte&0xF0,channel=statusByte&15,data1=(packed>>8)&127,data2=(packed>>16)&127;
        if(type==0xB0){
            if(learnFader){faderChannel=channel;faderCc=data1;learnFader=false;saveSettings();status("Crossfader appris sur CC "+std::to_string(data1)+".");return;}
            if(faderCc==data1&&faderChannel==channel){setFader(data2/127.);return;}
            if(learnMacro>=0){auto& m=macros[size_t(learnMacro)];m.midiChannel=channel;m.midiCc=data1;status("M"+std::to_string(learnMacro+1)+" apprise sur CC "+std::to_string(data1)+".");learnMacro=-1;saveSettings();return;}
            for(int i=0;i<8;++i)if(macros[size_t(i)].midiCc==data1&&macros[size_t(i)].midiChannel==channel){refresh();setMacro(i,data2/127.);}
            return;
        }
        if(type==0x90||type==0x80){
            refresh();const bool pressed=type==0x90&&data2>0;const bool rec=record&&snap->playing;
            engine.trigPlayLive(ui.selectedTrack,data1,pressed?data2:64,pressed,rec);if(pressed&&rec)changed();
        }
    }
    void tick(){
        ++tickCount;refresh();
        if(follow&&snap->playing){const int step=snap->trigPlayStep[ui.selectedTrack];if(step>=0&&paintStep<0)ui.stepPage=std::clamp(step/16,0,pages()-1);}
    }
};

TrigUI::TrigUI(Engine& engine,Host& host,std::string baseDirectory):impl(std::make_unique<Impl>(*this,engine,host,std::filesystem::u8path(baseDirectory))){status="Bienvenue dans ELEKTAKT : clique sur les pas pour poser des trigs, clic droit pour les sélectionner et poser des locks.";}
TrigUI::~TrigUI()=default;
void TrigUI::paint(Painter& p){impl->paint(p);}
void TrigUI::mouseDown(int x,int y,int b,int m){impl->mouseDown(x,y,b,m);}
void TrigUI::mouseMove(int x,int y,int m){impl->mouseMove(x,y,m);}
void TrigUI::mouseUp(int,int,int){impl->mouseUp();}
void TrigUI::wheel(int x,int y,int d,int m){impl->wheel(x,y,d,m);}
void TrigUI::key(int c,bool d,int m){impl->key(c,d,m);}
void TrigUI::midiIn(unsigned packed){impl->midiIn(packed);}
void TrigUI::tick(){impl->tick();}
bool TrigUI::wantsClose(){return impl->confirmDiscard();}
void TrigUI::dropFile(int x,int y,const std::string& path){
    impl->refresh();int target=selectedTrack;for(int t=0;t<TrigTracks;++t)if(trackRect(t).in(x,y))target=t;
    const auto extension=std::filesystem::u8path(path).extension().u8string();std::string lower=extension;for(auto& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));
    if(lower==".wav"){impl->selectTrack(target);impl->loadSampleFile(target,path);}
    else status="Seuls les fichiers WAV peuvent être glissés sur une piste.";
}
void TrigUI::loadDemo(){
    auto& e=impl->engine;impl->loadKit();e.setTempo(122);
    auto set=[&](int t,std::initializer_list<int> steps){for(int s:steps)e.trigToggle(t,s);};
    set(0,{0,4,8,12});
    TrigStep ghost;ghost.trig=true;ghost.velocity=90;ghost.condition=uint8_t(trigConditionFromCycle(2,2));e.trigSetStep(0,14,ghost);
    set(1,{4,12});TrigStep fill;fill.trig=true;fill.condition=TrigCondFill;fill.retrig=3;e.trigSetStep(1,15,fill);
    for(int s=0;s<16;s+=2){TrigStep hat;hat.trig=true;hat.velocity=uint8_t(s%4?80:110);if(s%4)hat.probability=70;e.trigSetStep(2,s,hat);}
    e.trigSetLock(2,6,TrigDecay,.08);
    TrigStep open;open.trig=true;open.condition=uint8_t(trigConditionFromCycle(4,4));e.trigSetStep(3,10,open);
    TrigStep clap;clap.trig=true;clap.condition=TrigCondNotFirst;clap.micro=6;e.trigSetStep(4,12,clap);
    // Bass on a 12-step track: polyrhythm against the 16-step drums, pitch locks.
    {lab::TrigTrack bass;bass.length=12;e.trigSetTrackConfig(12,bass);}
    const int notes[4]={0,3,7,10};for(int i=0;i<4;++i){e.trigToggle(12,i*3);e.trigSetLock(12,i*3,TrigPitch,(notes[i]+24)/48.);}
    e.trigSetLock(12,9,TrigDecay,.35);
    impl->refresh();dirty=false;impl->updateTitle();
    status="Pattern de démo : grosse caisse, caisse claire avec FILL, charleston à 70 %, basse sur 12 pas avec locks de hauteur. Espace pour jouer.";
}
}
