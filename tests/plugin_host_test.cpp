// SPDX-License-Identifier: AGPL-3.0-or-later
// Loads the built ELEKTAKT.vst3 like a DAW would: buses, sample rates, host
// transport, audio output, separate outputs, state save/restore, editor.
#include <JuceHeader.h>
#include <iostream>
#include "JucePainter.h"
#include "engine.hpp"

namespace {
int failures=0;
void check(bool ok,const juce::String& message){if(!ok){std::cerr<<"FAIL: "<<message<<"\n";++failures;}else std::cout<<"ok   "<<message<<"\n";}

struct TestPlayHead:juce::AudioPlayHead {
    bool playing=true;double ppq=0,bpm=120;
    juce::Optional<PositionInfo> getPosition()const override{PositionInfo info;info.setIsPlaying(playing);info.setPpqPosition(ppq);info.setBpm(bpm);info.setTimeSignature(TimeSignature{4,4});return info;}
};

double renderSeconds(juce::AudioPluginInstance& plugin,TestPlayHead& head,double rate,int block,double seconds,juce::AudioBuffer<float>& buffer,double* auxEnergy=nullptr){
    juce::MidiBuffer midi;double energy=0;const int blocks=int(seconds*rate/block);
    for(int b=0;b<blocks;++b){
        buffer.clear();plugin.processBlock(buffer,midi);
        for(int c=0;c<2;++c){auto* d=buffer.getReadPointer(c);for(int i=0;i<block;++i){if(!std::isfinite(d[i]))return -1;energy+=std::abs(d[i]);}}
        if(auxEnergy)for(int c=2;c<buffer.getNumChannels();++c){auto* d=buffer.getReadPointer(c);for(int i=0;i<block;++i)*auxEnergy+=std::abs(d[i]);}
        head.ppq+=block/rate*head.bpm/60.;
    }
    return energy;
}
}

int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc<2){std::cerr<<"usage: plugin_host_test path/to/ELEKTAKT.vst3\n";return 2;}
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found,juce::String(argv[1]));
    check(found.size()==1,"one plugin found in the bundle");if(found.isEmpty())return 1;
    const auto& d=*found[0];
    check(d.manufacturerName=="ANKS Labs","vendor is ANKS Labs ("+d.manufacturerName+")");
    check(d.isInstrument,"declared as an instrument");
    for(double rate:{44100.,48000.,96000.}){
        juce::String error;auto plugin=format.createInstanceFromDescription(d,rate,512,error);
        check(plugin!=nullptr,"instance at "+juce::String(rate)+" Hz "+error);if(!plugin)continue;
        check(plugin->getBusCount(false)==9,"9 stereo outputs ("+juce::String(plugin->getBusCount(false))+")");
        plugin->enableAllBuses();
        TestPlayHead head;plugin->setPlayHead(&head);plugin->prepareToPlay(rate,512);
        juce::AudioBuffer<float> buffer(plugin->getTotalNumOutputChannels(),512);
        const double energy=renderSeconds(*plugin,head,rate,512,4.,buffer);
        check(energy>10,"demo pattern audible at "+juce::String(rate)+" Hz (energy "+juce::String(energy,1)+")");
        head.playing=false;const double silent=renderSeconds(*plugin,head,rate,512,2.,buffer);
        // After stop only tails remain: much quieter than playback.
        check(silent>=0&&silent<energy*.25,"stopping in the host stops the sequencer");
        if(rate==48000.){
            juce::MemoryBlock state;plugin->getStateInformation(state);check(state.getSize()>1000,"state saved ("+juce::String(int(state.getSize()))+" bytes)");
            auto copy=format.createInstanceFromDescription(d,rate,512,error);
            if(copy){copy->enableAllBuses();copy->setStateInformation(state.getData(),int(state.getSize()));TestPlayHead h2;copy->setPlayHead(&h2);copy->prepareToPlay(rate,512);
                juce::AudioBuffer<float> b2(copy->getTotalNumOutputChannels(),512);check(renderSeconds(*copy,h2,rate,512,3.,b2)>10,"restored state plays");
                juce::MemoryBlock again;copy->getStateInformation(again);check(again.getSize()>1000,"restored state can be saved again");copy->releaseResources();}
            std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());check(editor!=nullptr,"editor opens");
            if(editor){editor->setSize(1600,900);juce::Image image(juce::Image::RGB,1600,900,true);juce::Graphics g(image);editor->paintEntireComponent(g,false);
                juce::PNGImageFormat png;juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile("plugin-editor.png"));if(out.openedOk()){out.setPosition(0);out.truncate();png.writeImageToStream(image,out);}
                plugin->editorBeingDeleted(editor.get());editor.reset();}
        }
        plugin->releaseResources();
    }
    // The interface drawn with the plugin's JUCE painter (same code as the editor).
    {
        struct NullHost:trig::Host{int popup(const std::vector<trig::MenuItem>&)override{return -1;}std::string openPath(const std::string&,const std::string&)override{return {};}std::string savePath(const std::string&,const std::string&,const std::string&)override{return {};}
            bool confirm(const std::string&)override{return true;}void setTitle(const std::string&)override{}std::vector<std::string> audioOutputs()override{return {};}int audioOutput()override{return -1;}void selectAudioOutput(int)override{}
            std::vector<std::string> midiInputs()override{return {};}int midiInput()override{return -1;}void selectMidiInput(int)override{}std::vector<std::string> midiOutputs()override{return {"MIDI du plugin (vers une piste Live, un seul canal)"};}int midiOutput()override{return 0;}void selectMidiOutput(int)override{}
            void stopAudio()override{}void startAudio()override{}void quit()override{}bool isPlugin()const override{return true;}int outputCount()const override{return 9;}} host;
        auto engine=std::make_unique<lab::Engine>();engine->setHostSync(true);
        const auto base=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ELEKTAKT");
        trig::TrigUI ui(*engine,host,base.getFullPathName().toStdString());ui.loadDemo();
        float block[lab::Block*2];for(int b=0;b<120;++b){engine->setHostTransport(true,b*lab::Block/24000.,120);engine->render(block);}ui.tick();
        juce::Image image(juce::Image::RGB,1600,900,true);{juce::Graphics g(image);JucePainter painter(g);ui.paint(painter);}
        juce::PNGImageFormat png;const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("plugin-interface.png");file.deleteFile();juce::FileOutputStream out(file);if(out.openedOk())png.writeImageToStream(image,out);
        check(image.getPixelAt(30,880)!=juce::Colours::black,"interface drawn with the JUCE painter");
    }
    if(failures){std::cerr<<failures<<" plugin check(s) failed\n";return 1;}
    std::cout<<"PASS: ELEKTAKT.vst3 loads, plays in sync, stops, saves and restores its state, opens its editor\n";return 0;
}
