// SPDX-License-Identifier: AGPL-3.0-or-later
// ELEKTAKT — VST3 plugin (JUCE). Same engine and interface as the standalone
// application; the host drives transport, tempo and sample rate.
#include <JuceHeader.h>
#include "engine.hpp"
#include "ui.hpp"
#include "DemoKit.h"
#include "JucePainter.h"
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>

namespace {
constexpr int AuxOutputs = 8; // separate stereo outputs 2–9
constexpr int EngineChannels = 2 + AuxOutputs * 2;

juce::File baseFolder() { return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ELEKTAKT"); }

// Writes the embedded demo kit next to the user's samples the first time.
void installDemoKit() {
    const auto folder = baseFolder().getChildFile("demo-kit");
    folder.createDirectory();
    baseFolder().getChildFile("samples").createDirectory();
    for (int i = 0; i < DemoKit::namedResourceListSize; ++i) {
        const char* original = DemoKit::getNamedResourceOriginalFilename(DemoKit::namedResourceList[i]);
        int size = 0;
        const char* data = DemoKit::getNamedResource(DemoKit::namedResourceList[i], size);
        if (!original || !data) continue;
        const auto file = folder.getChildFile(original);
        if (!file.existsAsFile()) file.replaceWithData(data, size_t(size));
    }
}

// Engine samples (48 kHz) waiting to be converted to the host rate.
struct Fifo {
    std::vector<float> data;
    int read = 0, write = 0;
    void reset(int capacity) {
        data.assign(size_t(capacity), 0.f);
        read = write = 0;
    }
    int available() const { return write - read; }
    void push(const float* samples, int count) {
        if (write + count > int(data.size())) {
            std::copy(data.begin() + read, data.begin() + write, data.begin());
            write -= read;
            read = 0;
        }
        if (write + count > int(data.size())) data.resize(size_t(write + count));
        std::copy(samples, samples + count, data.begin() + write);
        write += count;
    }
    const float* peek() const { return data.data() + read; }
    void consume(int count) { read = std::min(write, read + count); }
};
} // namespace

class ElektaktProcessor;

// ----------------------------------------------------------------- host services
class PluginHost : public trig::Host {
public:
    explicit PluginHost(ElektaktProcessor& p) : processor(p) {}
    int popup(const std::vector<trig::MenuItem>& items) override {
        juce::PopupMenu menu;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            if (item.separator) {
                menu.addSeparator();
                continue;
            }
            menu.addItem(int(i) + 1, juce::String::fromUTF8(item.label.c_str()), item.enabled, item.checked);
        }
        return menu.showMenu(juce::PopupMenu::Options().withParentComponent(editor)) - 1;
    }
    static juce::String pattern(const std::string& kind) {
        if (kind == "project") return "*.elektakt";
        if (kind == "kick") return "*.kicklab";
        if (kind == "kit") return "*.kickkit";
        return "*.wav";
    }
    std::string openPath(const std::string& title, const std::string& kind) override {
        juce::FileChooser chooser(juce::String::fromUTF8(title.c_str()), baseFolder().getChildFile(kind == "wav" ? "samples" : "projets"),
                                  pattern(kind));
        return chooser.browseForFileToOpen() ? chooser.getResult().getFullPathName().toStdString() : std::string();
    }
    std::string savePath(const std::string& title, const std::string& kind, const std::string& suggested) override {
        const auto folder = baseFolder().getChildFile("projets");
        folder.createDirectory();
        juce::FileChooser chooser(juce::String::fromUTF8(title.c_str()), folder.getChildFile(juce::String::fromUTF8(suggested.c_str())),
                                  pattern(kind));
        return chooser.browseForFileToSave(true) ? chooser.getResult().getFullPathName().toStdString() : std::string();
    }
    bool confirm(const std::string& question) override {
        return juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "ELEKTAKT",
                                                  juce::String::fromUTF8(question.c_str()), "Oui", "Non", editor);
    }
    void setTitle(const std::string&) override {}
    std::vector<std::string> audioOutputs() override { return {}; }
    int audioOutput() override { return -1; }
    void selectAudioOutput(int) override {}
    std::vector<std::string> midiInputs() override { return {}; }
    int midiInput() override { return -1; }
    void selectMidiInput(int) override {}
    std::vector<std::string> midiOutputs() override;
    int midiOutput() override;
    void selectMidiOutput(int index) override;
    void stopAudio() override;
    void startAudio() override;
    void quit() override {}
    bool isPlugin() const override { return true; }
    int outputCount() const override { return 1 + AuxOutputs; }
    juce::Component* editor = nullptr;

private:
    ElektaktProcessor& processor;
};

// ----------------------------------------------------------------- processor
class ElektaktProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    ElektaktProcessor() : juce::AudioProcessor(makeBuses()), host(*this) {
        installDemoKit();
        engine = std::make_unique<lab::Engine>();
        engine->setHostSync(true);
        engine->setMidiClockOut(false);
        ui = std::make_unique<trig::TrigUI>(*engine, host, baseFolder().getFullPathName().toStdString());
        ui->loadDemo();
        startTimerHz(30);
    }
    ~ElektaktProcessor() override {
        stopTimer();
        suspend();
        ui.reset();
        engine.reset();
    }
    static BusesProperties makeBuses() {
        BusesProperties buses;
        buses = buses.withOutput("Principale", juce::AudioChannelSet::stereo(), true);
        for (int o = 0; o < AuxOutputs; ++o)
            buses = buses.withOutput("Sortie " + juce::String(o + 2), juce::AudioChannelSet::stereo(), true);
        return buses;
    }
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        if (layout.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
        for (int b = 1; b < layout.outputBuses.size(); ++b) {
            const auto& set = layout.outputBuses.getReference(b);
            if (!set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
        }
        return layout.inputBuses.isEmpty();
    }
    const juce::String getName() const override { return "ELEKTAKT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "ELEKTAKT"; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        tempOut.reserve(size_t(std::max(samplesPerBlock, 4096)));
        hostRate = sampleRate > 0 ? sampleRate : 48000.;
        ratio = lab::Rate / hostRate;
        for (auto& f : fifos) f.reset(lab::Block * 16);
        for (auto& i : interpolators) i.reset();
        pendingCount = 0;
        setLatencySamples(0);
    }
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals noDenormals;
        const int frames = buffer.getNumSamples();
        busy = true;
        if (suspended.load()) {
            buffer.clear();
            midi.clear();
            busy = false;
            return;
        }
        // Notes from Live play the selected track (or the 16 pads); CCs go to the interface.
        for (const auto metadata : midi) {
            const auto message = metadata.getMessage();
            const auto* raw = message.getRawData();
            if (message.getRawDataSize() < 2) continue;
            const unsigned packed =
                unsigned(raw[0]) | (unsigned(raw[1]) << 8) | (message.getRawDataSize() > 2 ? unsigned(raw[2]) << 16 : 0u);
            if (message.isNoteOnOrOff())
                ui->audioThreadNote(packed);
            else if (message.isController()) {
                std::lock_guard<std::mutex> lock(ccMutex);
                if (ccQueue.size() < 256) ccQueue.push_back(packed);
            }
        }
        midi.clear();
        // Host transport.
        bool playing = false;
        double quarters = 0, bpm = 120;
        if (auto* head = getPlayHead())
            if (auto position = head->getPosition()) {
                playing = position->getIsPlaying();
                if (auto q = position->getPpqPosition()) quarters = *q;
                if (auto b = position->getBpm()) bpm = *b;
            }
        // Separate outputs are active only when the host enabled them.
        bool anyAux = false;
        for (int o = 0; o < AuxOutputs; ++o)
            if (auto* bus = getBus(false, o + 1)) anyAux = anyAux || bus->isEnabled();
        engine->setMultiOut(anyAux);
        // Render engine blocks until the conversion has enough input.
        const int needed = int(std::ceil(frames * ratio)) + 4;
        float block[lab::Block * 2];
        float left[lab::Block], right[lab::Block];
        float scratch[EngineChannels][lab::Block];
        const auto snapshotOutputs = trackOutputs.load();
        while (fifos[0].available() < needed) {
            const double offsetSeconds = fifos[0].available() / double(lab::Rate);
            engine->setHostTransport(playing, quarters + offsetSeconds * bpm / 60., bpm);
            engine->render(block);
            for (int n = 0; n < lab::Block; ++n) {
                scratch[0][n] = block[n * 2];
                scratch[1][n] = block[n * 2 + 1];
            }
            for (int c = 2; c < EngineChannels; ++c) std::fill(scratch[c], scratch[c] + lab::Block, 0.f);
            if (anyAux)
                for (int t = 0; t < lab::TrigTracks; ++t) {
                    const int output = int((snapshotOutputs >> (t * 4)) & 15u);
                    if (output < 1 || output > AuxOutputs) continue;
                    engine->trackBlock(t, left, right);
                    for (int n = 0; n < lab::Block; ++n) {
                        scratch[output * 2][n] += left[n];
                        scratch[output * 2 + 1][n] += right[n];
                    }
                }
            for (int c = 0; c < EngineChannels; ++c) fifos[size_t(c)].push(scratch[c], lab::Block);
        }
        // Engine frame at the FIFO read head: MIDI events are placed relative to it.
        const uint64_t readFrame = engine->renderedFrames() - uint64_t(fifos[0].available());
        // Convert 48 kHz → host rate (bypassed at 48 kHz).
        int consumed = frames;
        for (int c = 0; c < EngineChannels; ++c) {
            const int bus = c / 2, channel = c % 2;
            float* out = nullptr;
            if (auto* b = getBus(false, bus))
                if (b->isEnabled()) {
                    auto view = getBusBuffer(buffer, false, bus);
                    if (channel < view.getNumChannels()) out = view.getWritePointer(channel);
                }
            std::vector<float>& temp = tempOut;
            temp.resize(size_t(frames));
            if (std::abs(ratio - 1.) < 1e-9) {
                std::copy(fifos[size_t(c)].peek(), fifos[size_t(c)].peek() + frames, temp.begin());
                consumed = frames;
            } else
                consumed = interpolators[size_t(c)].process(ratio, fifos[size_t(c)].peek(), temp.data(), frames);
            if (out) std::copy(temp.begin(), temp.end(), out);
        }
        for (auto& f : fifos) f.consume(consumed);
        // MIDI tracks: to the plugin MIDI output at their exact sample, or straight to a MIDI port.
        // Events beyond this host block wait in a fixed list (no allocation here).
        lab::MidiEvent event;
        while (pendingCount < int(pending.size()) && engine->popMidi(event)) {
            if (uint8_t(event.message & 0xFF) >= 0xF8) continue;
            pending[size_t(pendingCount++)] = event;
        }
        int kept = 0;
        for (int i = 0; i < pendingCount; ++i) {
            const auto e = pending[size_t(i)];
            const double delta = e.frame > readFrame ? double(e.frame - readFrame) : 0.;
            int offset = int(delta / ratio);
            if (delta > lab::Rate * 10.) offset = 0; // engine restarted: do not hold events forever
            if (offset >= frames) {
                pending[size_t(kept++)] = e;
                continue;
            }
            const juce::MidiMessage message(int(e.message & 0xFF), int((e.message >> 8) & 127), int((e.message >> 16) & 127));
            if (midiMode.load() == 0)
                midi.addEvent(message, std::clamp(offset, 0, std::max(0, frames - 1)));
            else {
                std::lock_guard<std::mutex> lock(portMutex);
                if (port) port->sendMessageNow(message);
            }
        }
        pendingCount = kept;
        busy = false;
    }

    // ----- state saved in the Live set -----
    void getStateInformation(juce::MemoryBlock& data) override {
        const auto temp = juce::File::createTempFile(".elektakt");
        std::string error;
        if (engine->save(temp.getFullPathName().toStdString(), error)) {
            juce::MemoryOutputStream out(data, false);
            out.writeString("ELEKTAKT-STATE-1");
            out.writeInt(midiMode.load());
            out.writeString(portName);
            juce::MemoryBlock file;
            temp.loadFileAsData(file);
            out.writeInt64(juce::int64(file.getSize()));
            out.write(file.getData(), file.getSize());
        }
        temp.deleteFile();
    }
    void setStateInformation(const void* data, int size) override {
        juce::MemoryInputStream in(data, size_t(size), false);
        if (in.readString() != "ELEKTAKT-STATE-1") return;
        const int mode = in.readInt();
        const auto name = in.readString();
        const auto length = in.readInt64();
        if (length <= 0 || length > 64 * 1024 * 1024) return;
        juce::MemoryBlock file;
        in.readIntoMemoryBlock(file, length);
        const auto temp = juce::File::createTempFile(".elektakt");
        temp.replaceWithData(file.getData(), file.getSize());
        suspend();
        std::string error;
        engine->open(temp.getFullPathName().toStdString(), error);
        engine->setHostSync(true);
        engine->setMidiClockOut(false);
        resume();
        temp.deleteFile();
        restoreMidiOutput(mode, name);
    }

    // ----- MIDI output selection -----
    std::vector<std::string> midiOutputNames() {
        std::vector<std::string> names{"MIDI du plugin (vers une piste Live, un seul canal)"};
        devices = juce::MidiOutput::getAvailableDevices();
        for (const auto& d : devices) names.push_back(d.name.toStdString());
        return names;
    }
    int midiOutputIndex() const {
        if (midiMode.load() == 0) return 0;
        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].name == portName) return i + 1;
        return -1;
    }
    void selectMidiOutput(int index) {
        std::lock_guard<std::mutex> lock(portMutex);
        port.reset();
        if (index <= 0) {
            midiMode = 0;
            portName.clear();
            return;
        }
        if (index - 1 < devices.size()) {
            port = juce::MidiOutput::openDevice(devices[index - 1].identifier);
            if (port) {
                midiMode = 1;
                portName = devices[index - 1].name;
            } else
                midiMode = 0;
        }
    }
    void restoreMidiOutput(int mode, const juce::String& name) {
        if (mode != 1 || name.isEmpty()) {
            selectMidiOutput(0);
            return;
        }
        midiOutputNames();
        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].name == name) {
                selectMidiOutput(i + 1);
                return;
            }
        selectMidiOutput(0);
    }
    // ----- audio thread pause (project load, reset, WAV export) -----
    void suspend() {
        suspended = true;
        while (busy.load()) juce::Thread::sleep(1);
    }
    void resume() { suspended = false; }

    std::unique_ptr<lab::Engine> engine;
    std::unique_ptr<trig::TrigUI> ui;
    PluginHost host;
    bool editorOpen = false;

private:
    void timerCallback() override {
        // Track routing for the audio thread (4 bits per track).
        if (engine) {
            const auto outputs = engine->trackOutputs();
            uint64_t packed = 0;
            for (int t = 0; t < lab::TrigTracks; ++t) packed |= uint64_t(outputs[size_t(t)] & 15) << (t * 4);
            trackOutputs = packed;
        }
        std::deque<unsigned> pending;
        {
            std::lock_guard<std::mutex> lock(ccMutex);
            pending.swap(ccQueue);
        }
        for (unsigned packed : pending) ui->midiIn(packed);
        // Without the editor, a slower tick keeps the live-input state current.
        if (!editorOpen && ++idleTicks % 6 == 0) ui->tick();
    }
    int idleTicks = 0;
    double hostRate = 48000, ratio = 1;
    std::array<lab::MidiEvent, 4096> pending{};
    int pendingCount = 0;
    std::array<Fifo, EngineChannels> fifos;
    std::array<juce::LagrangeInterpolator, EngineChannels> interpolators;
    std::vector<float> tempOut;
    std::atomic<bool> suspended{false}, busy{false};
    std::atomic<uint64_t> trackOutputs{0};
    std::mutex ccMutex;
    std::deque<unsigned> ccQueue;
    std::atomic<int> midiMode{0};
    juce::String portName;
    std::mutex portMutex;
    std::unique_ptr<juce::MidiOutput> port;
    juce::Array<juce::MidiDeviceInfo> devices;
};

std::vector<std::string> PluginHost::midiOutputs() { return processor.midiOutputNames(); }
int PluginHost::midiOutput() { return processor.midiOutputIndex(); }
void PluginHost::selectMidiOutput(int index) { processor.selectMidiOutput(index); }
void PluginHost::stopAudio() { processor.suspend(); }
void PluginHost::startAudio() { processor.resume(); }

// ----------------------------------------------------------------- editor
class ElektaktEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    explicit ElektaktEditor(ElektaktProcessor& p) : juce::AudioProcessorEditor(p), processor(p) {
        processor.host.editor = this;
        processor.editorOpen = true;
        setResizable(true, true);
        setResizeLimits(800, 450, 2400, 1350);
        getConstrainer()->setFixedAspectRatio(double(trig::DesignWidth) / trig::DesignHeight);
        setSize(1440, 810);
        setWantsKeyboardFocus(true);
        startTimerHz(30);
    }
    ~ElektaktEditor() override {
        stopTimer();
        processor.host.editor = nullptr;
        processor.editorOpen = false;
    }
    void paint(juce::Graphics& g) override {
        g.addTransform(juce::AffineTransform::scale(float(getWidth()) / trig::DesignWidth, float(getHeight()) / trig::DesignHeight));
        JucePainter painter(g);
        processor.ui->paint(painter);
    }
    int dx(const juce::MouseEvent& e) const { return int(e.position.x * trig::DesignWidth / std::max(1, getWidth())); }
    int dy(const juce::MouseEvent& e) const { return int(e.position.y * trig::DesignHeight / std::max(1, getHeight())); }
    static int mods(const juce::ModifierKeys& m) {
        return (m.isShiftDown() ? trig::ModShift : 0) | (m.isCommandDown() || m.isCtrlDown() ? trig::ModControl : 0);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        grabKeyboardFocus();
        processor.ui->mouseDown(dx(e), dy(e), e.mods.isPopupMenu() ? 1 : 0, mods(e.mods));
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        processor.ui->mouseMove(dx(e), dy(e), mods(e.mods));
        repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override { processor.ui->mouseMove(dx(e), dy(e), mods(e.mods)); }
    void mouseUp(const juce::MouseEvent& e) override {
        processor.ui->mouseUp(dx(e), dy(e), e.mods.isPopupMenu() ? 1 : 0);
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        const float d = std::abs(w.deltaY) > std::abs(w.deltaX) ? w.deltaY : w.deltaX;
        if (d == 0) return;
        processor.ui->wheel(dx(e), dy(e), d > 0 ? 1 : -1, mods(e.mods));
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        int code = 0;
        const int k = key.getKeyCode();
        if (k == juce::KeyPress::escapeKey)
            code = trig::KeyEscape;
        else if (k == juce::KeyPress::deleteKey)
            code = trig::KeyDelete;
        else if (k == juce::KeyPress::backspaceKey)
            code = trig::KeyBack;
        else if (k == juce::KeyPress::leftKey)
            code = trig::KeyLeft;
        else if (k == juce::KeyPress::rightKey)
            code = trig::KeyRight;
        else if (k == juce::KeyPress::upKey)
            code = trig::KeyUp;
        else if (k == juce::KeyPress::downKey)
            code = trig::KeyDown;
        else if (k >= juce::KeyPress::F1Key && k <= juce::KeyPress::F8Key)
            code = trig::KeyF1 + (k - juce::KeyPress::F1Key);
        else {
            const auto c = juce::CharacterFunctions::toUpperCase(key.getTextCharacter());
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                code = int(c);
            else if (k >= 'A' && k <= 'Z')
                code = k;
        }
        if (!code) return false;
        if (code == 'F') { fillHeld = true; }
        processor.ui->key(code, true, mods(key.getModifiers()));
        repaint();
        return true;
    }
    bool keyStateChanged(bool isDown) override {
        if (!isDown && fillHeld && !juce::KeyPress::isKeyCurrentlyDown('F') && !juce::KeyPress::isKeyCurrentlyDown('f')) {
            fillHeld = false;
            processor.ui->key('F', false, 0);
            repaint();
        }
        return false;
    }
    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        for (const auto& f : files)
            if (f.endsWithIgnoreCase(".wav")) return true;
        return false;
    }
    void filesDropped(const juce::StringArray& files, int x, int y) override {
        int offset = 0;
        for (const auto& f : files) {
            processor.ui->dropFile(x * trig::DesignWidth / std::max(1, getWidth()),
                                   y * trig::DesignHeight / std::max(1, getHeight()) + offset, f.toStdString());
            offset += 46;
        }
        repaint();
    }

private:
    void timerCallback() override {
        if (processor.ui->tick()) repaint();
    }
    ElektaktProcessor& processor;
    bool fillHeld = false;
};

juce::AudioProcessorEditor* ElektaktProcessor::createEditor() { return new ElektaktEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ElektaktProcessor(); }
