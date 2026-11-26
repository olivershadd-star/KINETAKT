// SPDX-License-Identifier: AGPL-3.0-or-later
// ELEKTAKT — Windows application: window, GDI+ drawing, WinMM audio, MIDI in/out.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <avrt.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cwctype>
#include "engine.hpp"
#include "audio_queue.hpp"
#include "ui.hpp"

namespace {
std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring r(size_t(n),0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),r.data(),n);return r;}
std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string r(size_t(n),0);WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),r.data(),n,nullptr,nullptr);return r;}
constexpr UINT MidiInMessage=WM_APP+43;

// ---------------------------------------------------------------- audio output
// Same WinMM design as KICK: four buffers of two engine blocks, event driven.
class AudioOutput {
public:
    explicit AudioOutput(lab::Engine& e):engine(e){}
    ~AudioOutput(){stop();}
    std::atomic<unsigned> error{0};std::atomic<bool> running{false};
    void start(UINT id){stop();error=0;alive=true;thread=std::thread([this,id]{run(id);});}
    void stop(){alive=false;if(thread.joinable())thread.join();running=false;}
    // Engine frame currently leaving the speakers, or 0 when unknown.
    uint64_t playedFrame(){
        std::lock_guard<std::mutex> lock(deviceMutex);if(!device)return 0;
        MMTIME time{};time.wType=TIME_SAMPLES;if(waveOutGetPosition(device,&time,sizeof(time))!=MMSYSERR_NOERROR||time.wType!=TIME_SAMPLES)return 0;
        return baseFrame+time.u.sample;
    }
private:
    lab::Engine& engine;std::thread thread;std::atomic<bool> alive{false};
    std::mutex deviceMutex;HWAVEOUT device{};uint64_t baseFrame=0;
    void run(UINT id){
        HANDLE ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ready){error=MMSYSERR_NOMEM;return;}
        DWORD taskIndex=0;HANDLE task=AvSetMmThreadCharacteristicsW(L"Pro Audio",&taskIndex);if(task)AvSetMmThreadPriority(task,AVRT_PRIORITY_HIGH);
        constexpr int Frames=lab::Block*2,Buffers=4;
        WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=lab::Rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=lab::Rate*4;
        HWAVEOUT handle{};MMRESULT result=waveOutOpen(&handle,id,&fmt,reinterpret_cast<DWORD_PTR>(ready),0,CALLBACK_EVENT);
        if(result){error=result;if(task)AvRevertMmThreadCharacteristics(task);CloseHandle(ready);return;}
        std::array<WAVEHDR,Buffers> headers{};std::array<std::array<short,Frames*2>,Buffers> data{};std::array<bool,Buffers> prepared{};
        auto fill=[&](int b){float block[lab::Block*2];for(int offset=0;offset<Frames;offset+=lab::Block){engine.render(block);for(int i=0;i<lab::Block*2;++i)data[size_t(b)][size_t(offset*2+i)]=short(std::lround(std::clamp(block[i],-1.f,1.f)*32767));}};
        result=waveOutPause(handle);if(result){error=result;alive=false;}
        const uint64_t firstFrame=engine.renderedFrames();
        for(int b=0;b<Buffers&&alive;++b){
            headers[size_t(b)].lpData=reinterpret_cast<char*>(data[size_t(b)].data());headers[size_t(b)].dwBufferLength=sizeof(data[size_t(b)]);
            result=waveOutPrepareHeader(handle,&headers[size_t(b)],sizeof(WAVEHDR));if(result){error=result;alive=false;break;}prepared[size_t(b)]=true;
            fill(b);result=waveOutWrite(handle,&headers[size_t(b)],sizeof(WAVEHDR));if(result){error=result;alive=false;break;}
        }
        {std::lock_guard<std::mutex> lock(deviceMutex);device=handle;baseFrame=firstFrame;}
        if(alive){result=waveOutRestart(handle);if(result){error=result;alive=false;}}
        running=alive.load();lab::AudioQueue<Buffers> queue;
        while(alive){
            if(!queue.refill([&](std::size_t index){return alive&&(headers[index].dwFlags&WHDR_DONE);},[&](std::size_t index){fill(int(index));result=waveOutWrite(handle,&headers[index],sizeof(WAVEHDR));if(result){error=result;return false;}return true;}))alive=false;
            if(alive)WaitForSingleObject(ready,50);
        }
        {std::lock_guard<std::mutex> lock(deviceMutex);device=nullptr;}
        waveOutReset(handle);for(int b=0;b<Buffers;++b)if(prepared[size_t(b)])waveOutUnprepareHeader(handle,&headers[size_t(b)],sizeof(WAVEHDR));waveOutClose(handle);running=false;
        if(task)AvRevertMmThreadCharacteristics(task);CloseHandle(ready);
    }
};

// ---------------------------------------------------------------- MIDI output
// Events carry the engine frame at which they must sound. They are sent when
// that frame reaches the speakers, so MIDI and audio stay aligned.
class MidiOutput {
public:
    MidiOutput(lab::Engine& e,AudioOutput& a):engine(e),audio(a){thread=std::thread([this]{run();});}
    ~MidiOutput(){alive=false;if(thread.joinable())thread.join();close();}
    bool open(int id){
        std::lock_guard<std::mutex> lock(mutex);closeLocked();if(id<0){current=-1;return true;}
        HMIDIOUT handle{};if(midiOutOpen(&handle,UINT(id),0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){current=-1;return false;}
        out=handle;current=id;return true;
    }
    void close(){std::lock_guard<std::mutex> lock(mutex);closeLocked();}
    int device()const{return current;}
    // Stops reading the engine (needed while the project is being replaced).
    void pause(){paused=true;while(!idle&&alive)Sleep(1);}
    void resume(){paused=false;}
private:
    lab::Engine& engine;AudioOutput& audio;std::thread thread;std::atomic<bool> alive{true},paused{false},idle{false};std::mutex mutex;HMIDIOUT out{};std::atomic<int> current{-1};
    void closeLocked(){if(out){midiOutReset(out);midiOutClose(out);out=nullptr;}}
    void send(uint32_t message){std::lock_guard<std::mutex> lock(mutex);if(out)midiOutShortMsg(out,message);}
    void run(){
        timeBeginPeriod(1);std::vector<lab::MidiEvent> pending;
        while(alive){
            if(paused){idle=true;Sleep(1);continue;}idle=false;
            lab::MidiEvent event;while(engine.popMidi(event))pending.push_back(event);
            if(!pending.empty()){
                std::stable_sort(pending.begin(),pending.end(),[](const lab::MidiEvent& a,const lab::MidiEvent& b){return a.frame<b.frame;});
                const uint64_t now=audio.playedFrame();size_t sent=0;
                for(const auto& e:pending){if(e.frame!=0&&now!=0&&e.frame>now)break;send(e.message);++sent;}
                pending.erase(pending.begin(),pending.begin()+long(sent));
            }
            Sleep(1);
        }
        timeEndPeriod(1);
    }
};

// ---------------------------------------------------------------- GDI+ painter
// Fonts are cached across frames; the cache is rebuilt when the scale changes.
struct FontCache { double scale=0;std::map<int,std::unique_ptr<Gdiplus::Font>> fonts; };
class GdiPainter:public trig::Painter {
public:
    GdiPainter(Gdiplus::Graphics& g,double sx,double sy,FontCache& cache):g(g),sx(sx),sy(sy),cache(cache){if(cache.scale!=s()){cache.fonts.clear();cache.scale=s();}g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);}
    void fill(int x,int y,int w,int h,trig::Color c)override{Gdiplus::SolidBrush b(col(c));g.FillRectangle(&b,X(x),Y(y),W(w),H(h));}
    void frame(int x,int y,int w,int h,trig::Color c,int t)override{Gdiplus::Pen p(col(c),Gdiplus::REAL(t*s()));g.DrawRectangle(&p,X(x),Y(y),W(w),H(h));}
    void round(int x,int y,int w,int h,int r,trig::Color c)override{Gdiplus::GraphicsPath path;rounded(path,x,y,w,h,r);Gdiplus::SolidBrush b(col(c));g.FillPath(&b,&path);}
    void roundFrame(int x,int y,int w,int h,int r,trig::Color c,int t)override{Gdiplus::GraphicsPath path;rounded(path,x,y,w,h,r);Gdiplus::Pen p(col(c),Gdiplus::REAL(t*s()));g.DrawPath(&p,&path);}
    void text(int x,int y,int w,int h,const std::string& s8,int size,trig::Color c,trig::Align a,bool bold)override{
        const auto text=wide(s8);Gdiplus::SolidBrush b(col(c));Gdiplus::StringFormat format;
        format.SetAlignment(a==trig::Align::Left?Gdiplus::StringAlignmentNear:a==trig::Align::Center?Gdiplus::StringAlignmentCenter:Gdiplus::StringAlignmentFar);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::RectF rect(X(x),Y(y),W(w),H(h));g.DrawString(text.c_str(),-1,font(size,bold),rect,&format,&b);
    }
    void line(int x1,int y1,int x2,int y2,trig::Color c,int w)override{Gdiplus::Pen p(col(c),Gdiplus::REAL(w*s()));p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&p,X(x1),Y(y1),X(x2),Y(y2));}
    void arc(int cx,int cy,int r,double start,double sweep,trig::Color c,int w)override{Gdiplus::Pen p(col(c),Gdiplus::REAL(w*s()));p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);g.DrawArc(&p,X(cx-r),Y(cy-r),W(2*r),H(2*r),Gdiplus::REAL(start),Gdiplus::REAL(sweep));}
    void disc(int cx,int cy,int r,trig::Color c)override{Gdiplus::SolidBrush b(col(c));g.FillEllipse(&b,X(cx-r),Y(cy-r),W(2*r),H(2*r));}
private:
    Gdiplus::Graphics& g;double sx,sy;FontCache& cache;
    double s()const{return std::min(sx,sy);}
    Gdiplus::REAL X(double v)const{return Gdiplus::REAL(v*sx);}Gdiplus::REAL Y(double v)const{return Gdiplus::REAL(v*sy);}
    Gdiplus::REAL W(double v)const{return Gdiplus::REAL(v*sx);}Gdiplus::REAL H(double v)const{return Gdiplus::REAL(v*sy);}
    static Gdiplus::Color col(trig::Color c){return Gdiplus::Color(255,c.r,c.g,c.b);}
    Gdiplus::Font* font(int size,bool bold){const int key=size*2+(bold?1:0);auto& slot=cache.fonts[key];if(!slot)slot=std::make_unique<Gdiplus::Font>(L"Segoe UI",Gdiplus::REAL(size*1.08*s()),bold?Gdiplus::FontStyleBold:Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);return slot.get();}
    void rounded(Gdiplus::GraphicsPath& path,int x,int y,int w,int h,int r){
        const Gdiplus::REAL d=Gdiplus::REAL(2*std::min(r,std::min(w,h)/2)),px=X(x),py=Y(y),pw=W(w),ph=H(h),dx=Gdiplus::REAL(d*sx),dy=Gdiplus::REAL(d*sy);
        if(d<=0){path.AddRectangle(Gdiplus::RectF(px,py,pw,ph));return;}
        path.AddArc(px,py,dx,dy,180,90);path.AddArc(px+pw-dx,py,dx,dy,270,90);path.AddArc(px+pw-dx,py+ph-dy,dx,dy,0,90);path.AddArc(px,py+ph-dy,dx,dy,90,90);path.CloseFigure();
    }
};

// ---------------------------------------------------------------- application
class App:public trig::Host {
public:
    HWND window{};lab::Engine engine;AudioOutput audio{engine};MidiOutput midiOut{engine,audio};std::filesystem::path base;
    std::unique_ptr<trig::TrigUI> ui;UINT audioId=WAVE_MAPPER;HMIDIIN midiIn{};int midiInId=-1;int width=trig::DesignWidth,height=trig::DesignHeight;
    bool smoke=false;int smokeStep=0;ULONG_PTR gdiplusToken=0;FontCache fontCache;std::vector<std::string> outputCache;ULONGLONG outputCacheTime=0;std::wstring audioName,midiInName,midiOutName;
    App(){
        Gdiplus::GdiplusStartupInput startup;Gdiplus::GdiplusStartup(&gdiplusToken,&startup,nullptr);
        wchar_t path[32768];const DWORD n=GetModuleFileNameW(nullptr,path,32768);base=std::filesystem::path(std::wstring(path,n)).parent_path();
        std::error_code ec;std::filesystem::create_directories(base/L"samples",ec);std::filesystem::create_directories(base/L"projets",ec);
        ui=std::make_unique<trig::TrigUI>(engine,*this,base.u8string());
    }
    ~App(){fontCache.fonts.clear();if(midiIn){midiInStop(midiIn);midiInReset(midiIn);midiInClose(midiIn);}audio.stop();midiOut.close();ui.reset();if(gdiplusToken)Gdiplus::GdiplusShutdown(gdiplusToken);}
    void initialise(HWND h){
        window=h;loadDevices();ui->loadDemo();
        audio.start(audioId);
        if(!smoke){
            if(!midiOutName.empty())selectByName(midiOutName,false);else autoMidiOut();
            if(!midiInName.empty())selectByName(midiInName,true);else autoMidiIn();
        }
        DragAcceptFiles(h,TRUE);SetTimer(h,1,33,nullptr);
    }
    // ----- device persistence -----
    std::filesystem::path devicesFile()const{return base/L"ELEKTAKT-devices.txt";}
    void saveDevices(){std::ofstream f(devicesFile().u8string(),std::ios::trunc);if(!f)return;f<<"audio="<<utf8(audioName)<<"\nmidiin="<<utf8(midiInName)<<"\nmidiout="<<utf8(midiOutName)<<"\n";}
    void loadDevices(){
        std::ifstream f(devicesFile().u8string());std::string line;
        while(std::getline(f,line)){const auto at=line.find('=');if(at==std::string::npos)continue;const auto key=line.substr(0,at);const auto value=wide(line.substr(at+1));if(key=="audio")audioName=value;else if(key=="midiin")midiInName=value;else if(key=="midiout")midiOutName=value;}
        if(!audioName.empty()){for(UINT i=0;i<waveOutGetNumDevs();++i){WAVEOUTCAPSW cap{};waveOutGetDevCapsW(i,&cap,sizeof(cap));if(audioName==cap.szPname)audioId=i;}}
    }
    static std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return wchar_t(towlower(c));});return s;}
    void selectByName(const std::wstring& name,bool input){
        const auto names=input?midiInputs():midiOutputs();for(size_t i=0;i<names.size();++i)if(wide(names[i])==name){input?selectMidiInput(int(i)):selectMidiOutput(int(i));return;}
    }
    void autoMidiOut(){const auto names=midiOutputs();for(size_t i=0;i<names.size();++i)if(lower(wide(names[i])).find(L"loopmidi")!=std::wstring::npos){selectMidiOutput(int(i));return;}}
    void autoMidiIn(){const auto names=midiInputs();for(size_t i=0;i<names.size();++i){const auto n=lower(wide(names[i]));if(n.find(L"loopmidi")==std::wstring::npos&&(n.find(L"mpk")!=std::wstring::npos||n.find(L"lpd")!=std::wstring::npos||n.find(L"maschine")!=std::wstring::npos||n.find(L"keystep")!=std::wstring::npos||n.find(L"launchkey")!=std::wstring::npos)){selectMidiInput(int(i));return;}}}
    // ----- Host -----
    int popup(const std::vector<trig::MenuItem>& items)override{
        HMENU menu=CreatePopupMenu();for(size_t i=0;i<items.size();++i){const auto& item=items[i];if(item.separator){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);continue;}AppendMenuW(menu,MF_STRING|(item.checked?MF_CHECKED:0)|(item.enabled?0:MF_GRAYED),UINT_PTR(i+1),wide(item.label).c_str());}
        POINT p;GetCursorPos(&p);const int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,window,nullptr);DestroyMenu(menu);return choice-1;
    }
    static const wchar_t* filter(const std::string& kind){
        if(kind=="project")return L"Projet ELEKTAKT (*.elektakt)\0*.elektakt\0Tous les fichiers\0*.*\0\0";
        if(kind=="kick")return L"Projet KICK (*.kicklab)\0*.kicklab\0Tous les fichiers\0*.*\0\0";
        if(kind=="kit")return L"Kit KICK / ELEKTAKT (*.kickkit)\0*.kickkit\0Tous les fichiers\0*.*\0\0";
        return L"Audio WAV (*.wav)\0*.wav\0Tous les fichiers\0*.*\0\0";
    }
    std::string openPath(const std::string& title,const std::string& kind)override{
        std::vector<wchar_t> name(32768,0);const auto t=wide(title);const auto initial=(base/(kind=="wav"?L"samples":L"projets")).wstring();
        OPENFILENAMEW d{};d.lStructSize=sizeof(d);d.hwndOwner=window;d.lpstrFile=name.data();d.nMaxFile=DWORD(name.size());d.lpstrFilter=filter(kind);d.lpstrTitle=t.c_str();d.lpstrInitialDir=initial.c_str();d.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
        return GetOpenFileNameW(&d)?utf8(name.data()):std::string();
    }
    std::string savePath(const std::string& title,const std::string& kind,const std::string& suggested)override{
        std::vector<wchar_t> name(32768,0);const auto s=wide(suggested);std::copy(s.begin(),s.end(),name.begin());const auto t=wide(title);const auto initial=(base/L"projets").wstring();
        const wchar_t* ext=kind=="project"?L"elektakt":kind=="kit"?L"kickkit":L"wav";
        OPENFILENAMEW d{};d.lStructSize=sizeof(d);d.hwndOwner=window;d.lpstrFile=name.data();d.nMaxFile=DWORD(name.size());d.lpstrFilter=filter(kind);d.lpstrDefExt=ext;d.lpstrTitle=t.c_str();d.lpstrInitialDir=initial.c_str();d.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
        return GetSaveFileNameW(&d)?utf8(name.data()):std::string();
    }
    bool confirm(const std::string& question)override{return MessageBoxW(window,wide(question).c_str(),L"ELEKTAKT",MB_YESNO|MB_ICONQUESTION)==IDYES;}
    void setTitle(const std::string& title)override{if(window)SetWindowTextW(window,wide(title).c_str());}
    std::vector<std::string> audioOutputs()override{std::vector<std::string> r;for(UINT i=0;i<waveOutGetNumDevs();++i){WAVEOUTCAPSW cap{};waveOutGetDevCapsW(i,&cap,sizeof(cap));r.push_back(utf8(cap.szPname));}return r;}
    int audioOutput()override{return audioId==WAVE_MAPPER?-1:int(audioId);}
    void selectAudioOutput(int index)override{audio.stop();audioId=index<0?WAVE_MAPPER:UINT(index);audioName.clear();if(index>=0){WAVEOUTCAPSW cap{};waveOutGetDevCapsW(UINT(index),&cap,sizeof(cap));audioName=cap.szPname;}audio.start(audioId);saveDevices();}
    std::vector<std::string> midiInputs()override{std::vector<std::string> r;for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSW cap{};midiInGetDevCapsW(i,&cap,sizeof(cap));r.push_back(utf8(cap.szPname));}return r;}
    int midiInput()override{return midiInId;}
    static void CALLBACK midiCallback(HMIDIIN,UINT message,DWORD_PTR instance,DWORD_PTR packed,DWORD_PTR){
        if(message!=MIM_DATA)return;const unsigned status=unsigned(packed&255);if(status>=0xF0)return; // ignore clock / realtime
        PostMessageW(reinterpret_cast<HWND>(instance),MidiInMessage,WPARAM(packed&0xffffff),0);
    }
    void selectMidiInput(int index)override{
        if(midiIn){midiInStop(midiIn);midiInReset(midiIn);midiInClose(midiIn);midiIn=nullptr;}midiInId=-1;midiInName.clear();
        if(index>=0){if(midiInOpen(&midiIn,UINT(index),reinterpret_cast<DWORD_PTR>(&midiCallback),reinterpret_cast<DWORD_PTR>(window),CALLBACK_FUNCTION)==MMSYSERR_NOERROR&&midiInStart(midiIn)==MMSYSERR_NOERROR){midiInId=index;midiInName=wide(midiInputs()[size_t(index)]);ui->status="Entrée MIDI : "+utf8(midiInName)+" — ses notes jouent la piste sélectionnée.";}else{if(midiIn){midiInClose(midiIn);midiIn=nullptr;}ui->status="Entrée MIDI indisponible (déjà utilisée par un autre logiciel ?).";}}
        saveDevices();
    }
    std::vector<std::string> midiOutputs()override{
        // The interface asks for the names on every frame: refresh at most every 3 s.
        if(GetTickCount64()-outputCacheTime<3000&&outputCacheTime)return outputCache;outputCacheTime=GetTickCount64();
        std::vector<std::string> r;for(UINT i=0;i<midiOutGetNumDevs();++i){MIDIOUTCAPSW cap{};midiOutGetDevCapsW(i,&cap,sizeof(cap));r.push_back(utf8(cap.szPname));}outputCache=r;return r;}
    int midiOutput()override{return midiOut.device();}
    void selectMidiOutput(int index)override{
        midiOutName.clear();
        if(midiOut.open(index)){if(index>=0){midiOutName=wide(midiOutputs()[size_t(index)]);ui->status="Sortie MIDI : "+utf8(midiOutName)+". Dans Ableton, active Track (et Sync pour l'horloge) sur ce port.";}}
        else ui->status="Sortie MIDI indisponible : ferme les autres logiciels qui l'utilisent ou crée un port loopMIDI.";
        saveDevices();
    }
    void stopAudio()override{audio.stop();midiOut.pause();}
    void startAudio()override{midiOut.resume();audio.start(audioId);}
    void quit()override{PostMessageW(window,WM_CLOSE,0,0);}
    // ----- window events -----
    int toDesignX(int x)const{return x*trig::DesignWidth/std::max(1,width);}
    int toDesignY(int y)const{return y*trig::DesignHeight/std::max(1,height);}
    static int modifiers(){return ((GetKeyState(VK_SHIFT)&0x8000)?trig::ModShift:0)|((GetKeyState(VK_CONTROL)&0x8000)?trig::ModControl:0);}
    static int keyCode(WPARAM vk){
        switch(vk){case VK_SPACE:return trig::KeySpace;case VK_ESCAPE:return trig::KeyEscape;case VK_DELETE:return trig::KeyDelete;case VK_BACK:return trig::KeyBack;case VK_RETURN:return trig::KeyEnter;
        case VK_LEFT:return trig::KeyLeft;case VK_RIGHT:return trig::KeyRight;case VK_UP:return trig::KeyUp;case VK_DOWN:return trig::KeyDown;default:break;}
        if(vk>=VK_F1&&vk<=VK_F8)return trig::KeyF1+int(vk-VK_F1);
        if((vk>='A'&&vk<='Z')||(vk>='0'&&vk<='9'))return int(vk);return 0;
    }
    void paint(){
        PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT rc;GetClientRect(window,&rc);width=std::max(1L,rc.right);height=std::max(1L,rc.bottom);
        HDC memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,width,height);auto old=SelectObject(memory,bitmap);
        {Gdiplus::Graphics g(memory);GdiPainter painter(g,double(width)/trig::DesignWidth,double(height)/trig::DesignHeight,fontCache);ui->paint(painter);}
        BitBlt(dc,0,0,width,height,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);EndPaint(window,&ps);
    }
    void smokeTest(){
        // Automated check used by the release workflow: click through the main
        // areas, play, lock, switch a track to MIDI, then quit.
        auto click=[&](int x,int y,int button=0){ui->mouseDown(x,y,button,0);ui->mouseUp(x,y,button);};
        switch(smokeStep++){
        case 0:click(232,34);break;                  // PLAY
        case 1:click(100,124+12*46+20);break;        // track 13
        case 2:click(376+3*74+30,280,1);break;       // latch step 4
        case 3:ui->page=1;ui->wheel(376+60,520,3,0);break; // PITCH lock
        case 4:click(100,124+15*46+20);click(376+330+110,148);break; // track 16 → MIDI
        case 5:click(376+20,280);break;              // trig
        case 6:ui->key(trig::KeySpace,true,0);break; // stop
        case 7:InvalidateRect(window,nullptr,FALSE);break;
        default:DestroyWindow(window);break;
        }
    }
};
App* app=nullptr;

LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:app->initialise(h);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:app->paint();return 0;
    case WM_SIZE:InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_TIMER:app->ui->tick();if(app->smoke)app->smokeTest();InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:SetCapture(h);app->ui->mouseDown(app->toDesignX(GET_X_LPARAM(l)),app->toDesignY(GET_Y_LPARAM(l)),m==WM_RBUTTONDOWN?1:0,App::modifiers());InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_MOUSEMOVE:app->ui->mouseMove(app->toDesignX(GET_X_LPARAM(l)),app->toDesignY(GET_Y_LPARAM(l)),App::modifiers());if(w&(MK_LBUTTON|MK_RBUTTON))InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONUP:case WM_RBUTTONUP:ReleaseCapture();app->ui->mouseUp(app->toDesignX(GET_X_LPARAM(l)),app->toDesignY(GET_Y_LPARAM(l)),m==WM_RBUTTONUP?1:0);InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);const int delta=GET_WHEEL_DELTA_WPARAM(w)>0?1:-1;app->ui->wheel(app->toDesignX(p.x),app->toDesignY(p.y),delta,App::modifiers());InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_KEYDOWN:case WM_KEYUP:{const int code=App::keyCode(w);if(code){if(m==WM_KEYDOWN&&code=='F'&&(l&(1<<30)))return 0;app->ui->key(code,m==WM_KEYDOWN,App::modifiers());InvalidateRect(h,nullptr,FALSE);}return 0;}
    case MidiInMessage:app->ui->midiIn(unsigned(w));return 0;
    case WM_DROPFILES:{HDROP drop=reinterpret_cast<HDROP>(w);POINT p{};DragQueryPoint(drop,&p);const UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<count&&i<16;++i){std::vector<wchar_t> name(32768,0);DragQueryFileW(drop,i,name.data(),UINT(name.size()));app->ui->dropFile(app->toDesignX(p.x),app->toDesignY(p.y)+int(i)*46,utf8(name.data()));}DragFinish(drop);InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_GETMINMAXINFO:{auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={960,560};return 0;}
    case WM_CLOSE:if(app->smoke||app->ui->wantsClose())DestroyWindow(h);return 0;
    case WM_DESTROY:KillTimer(h,1);PostQuitMessage(0);return 0;
    default:return DefWindowProcW(h,m,w,l);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show){
    SetProcessDPIAware();
    App application;app=&application;app->smoke=command&&std::wstring(command).find(L"--smoke-test")!=std::wstring::npos;
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"ANKSLabsELEKTAKT";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);RegisterClassW(&wc);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int w=trig::DesignWidth,h=trig::DesignHeight;
    const double fit=std::min(1.,std::min((work.right-work.left)*.95/w,(work.bottom-work.top)*.92/h));
    RECT rect{0,0,int(w*fit),int(h*fit)};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowW(wc.lpszClassName,L"ELEKTAKT — ANKS Labs",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);
    if(!window)return 1;ShowWindow(window,show);UpdateWindow(window);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return 0;
}
