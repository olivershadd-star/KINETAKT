// SPDX-License-Identifier: AGPL-3.0-or-later
// TRIG — platform-independent interface. Windows draws it with GDI/GDI+,
// the test harness draws it with Cairo to check the layout.
#pragma once
#include "engine.hpp"
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace trig {
constexpr int DesignWidth=1600, DesignHeight=900;
struct Color { unsigned char r=0,g=0,b=0; };
enum class Align { Left, Center, Right };
struct Painter {
    virtual ~Painter()=default;
    virtual void fill(int x,int y,int w,int h,Color c)=0;
    virtual void frame(int x,int y,int w,int h,Color c,int thickness=1)=0;
    virtual void round(int x,int y,int w,int h,int radius,Color c)=0;
    virtual void roundFrame(int x,int y,int w,int h,int radius,Color c,int thickness=1)=0;
    virtual void text(int x,int y,int w,int h,const std::string& utf8,int size,Color c,Align align=Align::Left,bool bold=false)=0;
    virtual void line(int x1,int y1,int x2,int y2,Color c,int width=1)=0;
    // Angles in degrees, 0 = 3 o'clock, clockwise.
    virtual void arc(int cx,int cy,int radius,double start,double sweep,Color c,int width)=0;
    virtual void disc(int cx,int cy,int radius,Color c)=0;
};
struct MenuItem { std::string label; bool checked=false, enabled=true, separator=false; };
// Services that depend on the operating system.
struct Host {
    virtual ~Host()=default;
    virtual int popup(const std::vector<MenuItem>& items)=0;                 // −1 = cancelled
    virtual std::string openPath(const std::string& title,const std::string& kind)=0; // kind: project, kick, kit, wav
    virtual std::string savePath(const std::string& title,const std::string& kind,const std::string& suggested)=0;
    virtual bool confirm(const std::string& question)=0;
    virtual void setTitle(const std::string& title)=0;
    virtual std::vector<std::string> audioOutputs()=0; virtual int audioOutput()=0; virtual void selectAudioOutput(int index)=0;
    virtual std::vector<std::string> midiInputs()=0; virtual int midiInput()=0; virtual void selectMidiInput(int index)=0;
    virtual std::vector<std::string> midiOutputs()=0; virtual int midiOutput()=0; virtual void selectMidiOutput(int index)=0;
    virtual void stopAudio()=0; virtual void startAudio()=0;
    virtual void quit()=0;
};
enum Key { KeySpace=32, KeyEscape=27, KeyDelete=127, KeyBack=8, KeyEnter=13, KeyLeft=1000, KeyRight, KeyUp, KeyDown, KeyF1 };
enum Modifier { ModShift=1, ModControl=2 };

class TrigUI {
public:
    TrigUI(lab::Engine& engine,Host& host,std::string baseDirectory);
    ~TrigUI();
    void loadDemo();                   // demo kit + demo pattern
    void paint(Painter& painter);
    void mouseDown(int x,int y,int button,int modifiers); // button 0 = left, 1 = right
    void mouseMove(int x,int y,int modifiers);
    void mouseUp(int x,int y,int button);
    void wheel(int x,int y,int delta,int modifiers);   // delta ±1 per notch
    void key(int code,bool down,int modifiers);
    void midiIn(unsigned packed);                      // from the MIDI input callback
    void dropFile(int x,int y,const std::string& path); // WAV dropped on a track
    void tick();                                       // ~30 Hz
    bool wantsClose();                                 // asks about unsaved changes
    std::string status;
    // Exposed for tests.
    int selectedTrack=0,page=0,stepPage=0;
    std::vector<int> latched;          // latched steps of the selected track (absolute indices)
    bool dirty=false;
private:
    struct Impl; std::unique_ptr<Impl> impl;
};
}
