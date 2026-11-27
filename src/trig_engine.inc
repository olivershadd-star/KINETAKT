// SPDX-License-Identifier: AGPL-3.0-or-later
// TRIG — Elektron-style sequencer layer for the KICK engine.
// Included at the end of engine.cpp, inside namespace lab.

// ---------------------------------------------------------------- helpers
int trigConditionCount() {
    int count = TrigCondCycleFirst;
    for (int b = 2; b <= 8; ++b) count += b;
    return count;
}
bool trigConditionCycle(int code, int& a, int& b) {
    if (code < TrigCondCycleFirst) return false;
    int index = code - TrigCondCycleFirst;
    for (int length = 2; length <= 8; ++length) {
        if (index < length) {
            a = index + 1;
            b = length;
            return true;
        }
        index -= length;
    }
    return false;
}
int trigConditionFromCycle(int a, int b) {
    b = std::clamp(b, 2, 8);
    a = std::clamp(a, 1, b);
    int code = TrigCondCycleFirst;
    for (int length = 2; length < b; ++length) code += length;
    return code + a - 1;
}
std::string trigConditionName(int code) {
    static const char* names[] = {"—", "FILL", "!FILL", "PRE", "!PRE", "NEI", "!NEI", "1ST", "!1ST"};
    if (code >= 0 && code < TrigCondCycleFirst) return names[code];
    int a = 0, b = 0;
    if (trigConditionCycle(code, a, b)) return std::to_string(a) + ":" + std::to_string(b);
    return "—";
}
double trigLfoCycleSteps(int speed) {
    static const double steps[TrigLfoSpeeds] = {.5, 1, 2, 4, 8, 16, 32, 64, 128, 256};
    return steps[std::clamp(speed, 0, TrigLfoSpeeds - 1)];
}
const char* trigLfoSpeedName(int speed) {
    static const char* names[TrigLfoSpeeds] = {"1/2 pas", "1 pas",  "2 pas",  "4 pas",   "8 pas",
                                               "16 pas",  "32 pas", "64 pas", "128 pas", "256 pas"};
    return names[std::clamp(speed, 0, TrigLfoSpeeds - 1)];
}
const char* trigLfoWaveName(int wave) {
    static const char* names[LfoWaves] = {"SINUS", "TRIANGLE", "RAMPE +", "RAMPE −", "CARRÉ", "ALÉATOIRE"};
    return names[std::clamp(wave, 0, LfoWaves - 1)];
}
const char* trigLfoModeName(int mode) {
    static const char* names[LfoModes] = {"LIBRE", "TRIG", "UNE FOIS"};
    return names[std::clamp(mode, 0, LfoModes - 1)];
}
double trigSpeedFactor(int speed) {
    static const double factors[TrigSpeeds] = {2., 1., .5, .25, .125};
    return factors[std::clamp(speed, 0, TrigSpeeds - 1)];
}
const char* trigSpeedName(int speed) {
    static const char* names[TrigSpeeds] = {"2x", "1x", "1/2x", "1/4x", "1/8x"};
    return names[std::clamp(speed, 0, TrigSpeeds - 1)];
}

double trigParamBase(const PadEdit& edit, int param) {
    switch (param) {
    case TrigPitch: return std::clamp((edit.semitones + 24.) / 48., 0., 1.);
    case TrigStart: return edit.start;
    case TrigEnd: return edit.end;
    case TrigAttack: return edit.attack;
    case TrigDecay: return edit.decay;
    case TrigSustain: return edit.sustain;
    case TrigRelease: return edit.release;
    case TrigLevel: return std::clamp(edit.gain / 2., 0., 1.);
    case TrigPan: return std::clamp((edit.pan + 1.) / 2., 0., 1.);
    default: break;
    }
    if (param >= TrigFxFirst && param < TrigCcFirst) {
        const int slot = (param - TrigFxFirst) / 6, parameter = (param - TrigFxFirst) % 6;
        return edit.inserts[slot].parameters[parameter];
    }
    return 0;
}
void trigParamApply(PadEdit& edit, int param, double v) {
    v = std::clamp(v, 0., 1.);
    switch (param) {
    case TrigPitch: edit.semitones = std::round(v * 48. - 24.); return;
    case TrigStart:
        edit.start = std::min(v, .999);
        if (edit.end < edit.start + .001) edit.end = std::min(1., edit.start + .001);
        return;
    case TrigEnd: edit.end = std::max(v, edit.start + .001); return;
    case TrigAttack: edit.attack = v; return;
    case TrigDecay: edit.decay = v; return;
    case TrigSustain: edit.sustain = v; return;
    case TrigRelease: edit.release = v; return;
    case TrigLevel: edit.gain = v * 2.; return;
    case TrigPan: edit.pan = v * 2. - 1.; return;
    default: break;
    }
    if (param >= TrigFxFirst && param < TrigCcFirst) {
        const int slot = (param - TrigFxFirst) / 6, parameter = (param - TrigFxFirst) % 6;
        edit.inserts[slot].parameters[parameter] = v;
    }
}
std::string trigParamName(int param, const PadEdit* edit) {
    static const char* names[] = {"PITCH", "START", "END", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "LEVEL", "PAN"};
    if (param >= 0 && param < TrigFxFirst) return names[param];
    if (param >= TrigFxFirst && param < TrigCcFirst) {
        const int slot = (param - TrigFxFirst) / 6, parameter = (param - TrigFxFirst) % 6;
        if (edit) {
            const char* name = insertParameterName(edit->inserts[slot].effect, parameter);
            if (name && *name) {
                std::string upper = name;
                for (auto& c : upper) c = char(std::toupper(static_cast<unsigned char>(c)));
                return upper;
            }
            return "";
        }
        return "FX" + std::to_string(slot + 1) + " P" + std::to_string(parameter + 1);
    }
    if (trigIsCc(param)) return "CC " + std::to_string(param - TrigCcFirst + 1);
    if (param == TrigSendReverb) return "ENVOI RÉVERBE";
    if (param == TrigSendDelay) return "ENVOI DELAY";
    return "";
}
std::string trigParamDisplay(int param, double v) {
    v = std::clamp(v, 0., 1.);
    char text[32] = {};
    switch (param) {
    case TrigPitch: {
        const int st = int(std::round(v * 48. - 24.));
        std::snprintf(text, sizeof(text), "%+d st", st);
        return text;
    }
    case TrigStart:
    case TrigEnd:
    case TrigSustain: std::snprintf(text, sizeof(text), "%.0f %%", v * 100.); return text;
    case TrigAttack: std::snprintf(text, sizeof(text), "%.0f ms", v * v * 2000.); return text;
    case TrigDecay:
        if (v <= 0) return "entier";
        std::snprintf(text, sizeof(text), "%.0f ms", v * v * 2000.);
        return text;
    case TrigRelease: std::snprintf(text, sizeof(text), "%.0f ms", v * v * 4000.); return text;
    case TrigLevel: std::snprintf(text, sizeof(text), "%.0f %%", v * 200.); return text;
    case TrigPan: {
        const int pan = int(std::round(v * 200. - 100.));
        if (pan == 0) return "C";
        std::snprintf(text, sizeof(text), "%c%d", pan < 0 ? 'L' : 'R', std::abs(pan));
        return text;
    }
    default: break;
    }
    if (trigIsCc(param)) {
        std::snprintf(text, sizeof(text), "%d", int(std::round(v * 127.)));
        return text;
    }
    if (trigIsSend(param)) {
        std::snprintf(text, sizeof(text), "%.0f %%", v * 100.);
        return text;
    }
    std::snprintf(text, sizeof(text), "%.2f", v);
    return text;
}

double trigTrackBase(const PadEdit& edit, const TrigTrack& track, int param) {
    if (trigIsCc(param)) {
        const int v = track.ccValue[size_t(param - TrigCcFirst)];
        return v < 0 ? 0. : v / 127.;
    }
    if (param == TrigSendReverb) return track.sendReverb / double(TrigLockResolution);
    if (param == TrigSendDelay) return track.sendDelay / double(TrigLockResolution);
    return trigParamBase(edit, param);
}
const char* delayDivisionName(int division) {
    static const char* names[DelayDivisions] = {"1/32", "1/16",        "1/16 pointée", "1/8 triolet", "1/16 × 3",
                                                "1/8",  "1/8 pointée", "1/4",          "1/2"};
    return names[std::clamp(division, 0, DelayDivisions - 1)];
}
double delayDivisionSteps(int division) {
    static const double steps[DelayDivisions] = {.5, 1, 1.5, 4. / 3., 3, 2, 3, 4, 8};
    return steps[std::clamp(division, 0, DelayDivisions - 1)];
}

// ---------------------------------------------------------------- shared send effects
void Engine::Impl::processSends() {
    const auto& fx = state.sendFx;
    const auto& pattern = state.sequences[state.sequence].trig;
    float* inL = sendBuffer->getOutputBuffer(0);
    float* inR = sendBuffer->getOutputBuffer(1);
    std::fill(inL, inL + Block, 0.f);
    std::fill(inR, inR + Block, 0.f);
    static thread_local std::array<float, Block> dl{}, dr{};
    std::fill(dl.begin(), dl.end(), 0.f);
    std::fill(dr.begin(), dr.end(), 0.f);
    bool anyReverb = false, anyDelay = false;
    for (int t = 0; t < TrigTracks; ++t) {
        const auto& tr = pattern.tracks[size_t(t)];
        if (tr.type == TrigTrackType::Midi) {
            sendLevel[size_t(t)] = {0, 0};
            continue;
        }
        for (int k = 0; k < 2; ++k) {
            double target = liveSend[size_t(t)][size_t(k)];
            if (std::isnan(target)) target = lockedSend[size_t(t)][size_t(k)];
            if (std::isnan(target)) target = (k == 0 ? tr.sendReverb : tr.sendDelay) / double(TrigLockResolution);
            double& level = sendLevel[size_t(t)][size_t(k)];
            const double start = level;
            level = std::clamp(target, 0., 1.);
            if (start <= 0 && level <= 0) continue;
            const auto& capture = players[size_t(t)]->capture;
            float* l = k == 0 ? inL : dl.data();
            float* r = k == 0 ? inR : dr.data();
            for (int n = 0; n < Block; ++n) {
                const float g = float(start + (level - start) * (n + 1) / double(Block));
                l[n] += capture[0][size_t(n)] * g;
                r[n] += capture[1][size_t(n)] * g;
            }
            (k == 0 ? anyReverb : anyDelay) = true;
        }
    }
    float* outL = buffer->getOutputBuffer(0);
    float* outR = buffer->getOutputBuffer(1);
    // Reverb: Airwindows Galactic, fully wet, shared by every track.
    if (!reverbSynced || fx.reverbDecay != appliedFx.reverbDecay || fx.reverbSize != appliedFx.reverbSize ||
        fx.reverbTone != appliedFx.reverbTone) {
        auto& p = reverbEdit.inserts[0].parameters;
        p[0] = std::clamp(fx.reverbDecay, 0., 1.);
        p[1] = std::clamp(fx.reverbTone, 0., 1.);
        p[2] = .35;
        p[3] = std::clamp(fx.reverbSize, 0., 1.);
        p[4] = 1.;
        reverb->sync(reverbEdit);
        reverbSynced = true;
    }
    if (anyReverb || reverbTail > 0) {
        reverbTail = anyReverb ? Rate * 12 / Block : reverbTail - 1;
        reverb->render(*sendBuffer);
        const float g = float(fx.reverbReturn);
        for (int n = 0; n < Block; ++n) {
            outL[n] += inL[n] * g;
            outR[n] += inR[n] * g;
        }
    }
    // Delay: tempo-synced stereo delay with filtered feedback and optional ping-pong.
    const double target =
        std::clamp(delayDivisionSteps(fx.delayDivision) * Rate * 60. / state.tempo / 4., 64., double(delayLeft.size() - 4));
    if (anyDelay || delayTail > 0) {
        delayTail = anyDelay ? Rate * 16 / Block : delayTail - 1;
        const double feedback = std::clamp(fx.delayFeedback, 0., .95), coefficient = .05 + .9 * std::clamp(fx.delayTone, 0., 1.);
        const float g = float(fx.delayReturn);
        const size_t size = delayLeft.size();
        for (int n = 0; n < Block; ++n) {
            delayFrames += (target - delayFrames) * .0005; // glide when the tempo changes
            const double readPos = double(delayWrite) + size - delayFrames;
            const size_t a = size_t(readPos) % size, b = (a + 1) % size;
            const double f = readPos - std::floor(readPos);
            const double yl = delayLeft[a] + (delayLeft[b] - delayLeft[a]) * f, yr = delayRight[a] + (delayRight[b] - delayRight[a]) * f;
            delayLowL += (yl - delayLowL) * coefficient;
            delayLowR += (yr - delayLowR) * coefficient;
            if (fx.delayPingPong) {
                delayLeft[delayWrite] = float(dl[size_t(n)] * .5 + dr[size_t(n)] * .5 + delayLowR * feedback);
                delayRight[delayWrite] = float(delayLowL * feedback);
            } else {
                delayLeft[delayWrite] = float(dl[size_t(n)] + delayLowL * feedback);
                delayRight[delayWrite] = float(dr[size_t(n)] + delayLowR * feedback);
            }
            delayWrite = (delayWrite + 1) % size;
            outL[n] += float(yl) * g;
            outR[n] += float(yr) * g;
        }
    } else
        delayFrames = target;
    appliedFx = fx;
}

// ---------------------------------------------------------------- transport
void Engine::Impl::trigStart() {
    auto& s = state;
    const auto& pattern = s.sequences[s.sequence].trig;
    trigPos = 0;
    trigOrigin = 0;
    clockPhase = 0;
    trigNextBoundary = pattern.masterLength > 0 ? pattern.masterLength : 16;
    trigLastCond.fill(false);
    for (auto& t : trigSentCc) t.fill(-1);
    gateSamples.fill(0);
    for (int pad = 0; pad < Pads; ++pad) resetLocks(pad);
    if (s.midiClockOut) pushMidi(frames, 0xFA);
    trigWasPlaying = true;
}
void Engine::Impl::trigStopped(uint64_t frame) {
    if (state.midiClockOut && trigWasPlaying) pushMidi(frame, 0xFC);
    for (int channel = 0; channel < 16; ++channel) {
        bool any = false;
        for (int note = 0; note < 128; ++note)
            if (activeNotes[channel][note]) {
                pushMidi(frame, uint32_t(0x80 | channel) | (uint32_t(note) << 8));
                activeNotes[channel][note] = false;
                any = true;
            }
        if (any) pushMidi(frame, uint32_t(0xB0 | channel) | (123u << 8));
    }
    for (int pad = 0; pad < Pads; ++pad) resetLocks(pad);
    for (auto& t : trigSentCc) t.fill(-1);
    gateSamples.fill(0);
    glideCount.fill(0);
    trigEditValid.fill(false);
    hitCount.fill(0);
    state.trigPlayStep.fill(-1);
    trigWasPlaying = false;
}
void Engine::Impl::trigBoundary() {
    auto& s = state;
    const double boundary = trigNextBoundary;
    bool switched = false;
    if (s.songPlaying && s.songPosition >= 0 && s.songPosition < s.songLength) {
        ++s.songRepeat;
        if (s.songRepeat >= s.song[s.songPosition].repeats) {
            s.songRepeat = 0;
            ++s.songPosition;
            if (s.songPosition >= s.songLength) {
                s.playing = false;
                s.songPlaying = false;
                s.songPosition = -1;
            } else {
                const int next = s.song[s.songPosition].sequence;
                prepareProgram(s.sequences[next].program);
                s.sequence = next;
                activatePreparedProgram();
                switched = true;
            }
        }
    } else if (s.queuedSequence >= 0) {
        s.sequence = s.queuedSequence;
        s.queuedSequence = -1;
        activatePreparedProgram();
        if (s.sequence < Groups) s.group = s.sequence;
        s.queuedGroup = -1;
        switched = true;
    }
    const auto& pattern = s.sequences[s.sequence].trig;
    if (switched || pattern.masterLength > 0) {
        trigOrigin = boundary;
        trigLastCond.fill(false);
    }
    if (switched) {
        s.viewBar = 0;
        for (int pad = 0; pad < Pads; ++pad) resetLocks(pad);
    }
    trigNextBoundary = boundary + (pattern.masterLength > 0 ? pattern.masterLength : 16);
}
void Engine::Impl::trigFire(int t, const TrigTrack& tr, const TrigStep& st, long long k, int length, uint64_t frame, double stepSamples,
                            const SequenceData& sequence, bool anySolo) {
    auto& s = state;
    const bool muted = sequence.tracks[t].mute || (anySolo && !sequence.tracks[t].solo);
    auto startSlides = [&]() {
        // Parameter slide: each locked value glides to the next lock of the same parameter.
        if (!st.slide) return;
        const int index = int(k % length);
        for (int i = 0; i < st.lockCount; ++i) {
            const int param = st.locks[i].param;
            if (tr.type == TrigTrackType::Audio && (param == TrigPitch || param == TrigStart || param == TrigEnd)) continue;
            for (int d = 1; d < length; ++d) {
                const auto& next = tr.steps[size_t((index + d) % length)];
                const int at = next.findLock(param);
                if (at < 0) continue;
                addGlide(t, param, st.locks[i].value / double(TrigLockResolution),
                         next.locks[size_t(at)].value / double(TrigLockResolution), d * stepSamples);
                break;
            }
        }
    };
    auto stopGlide = [&](int param) {
        auto& g = glides[size_t(t)];
        int& n = glideCount[size_t(t)];
        for (int i = 0; i < n; ++i)
            if (g[size_t(i)].param == param) {
                g[size_t(i)] = g[size_t(n - 1)];
                --n;
                --i;
            }
    };
    if (!st.trig) {
        // Trigless lock: changes the running sound without a new note.
        if (muted) return;
        for (int i = 0; i < st.lockCount; ++i) stopGlide(st.locks[i].param);
        startSlides();
        if (tr.type == TrigTrackType::Midi) {
            for (int c = 0; c < TrigMidiCcs; ++c) {
                const int index = st.findLock(TrigCcFirst + c);
                if (index < 0) continue;
                const int value = std::clamp(int(std::lround(st.locks[index].value * 127. / TrigLockResolution)), 0, 127);
                if (value != trigSentCc[t][c]) {
                    pushMidi(frame, uint32_t(0xB0 | tr.midiChannel) | (uint32_t(tr.ccNumber[c] & 127) << 8) | (uint32_t(value) << 16));
                    trigSentCc[t][c] = value;
                }
            }
        } else
            for (int i = 0; i < st.lockCount; ++i) {
                const int param = st.locks[i].param;
                const double v = st.locks[i].value / double(TrigLockResolution);
                if (param == TrigPan)
                    lockedPan[t] = v * 2. - 1.;
                else if (param == TrigSendReverb)
                    lockedSend[size_t(t)][0] = v;
                else if (param == TrigSendDelay)
                    lockedSend[size_t(t)][1] = v;
                else if (param == TrigLevel)
                    players[t]->edit.gain = v * 2.;
                else if (param >= TrigFxFirst && param < TrigCcFirst)
                    lockedFx[t][(param - TrigFxFirst) / 6][(param - TrigFxFirst) % 6] = st.locks[i].value;
            }
        return;
    }
    const long long loop = k / std::max(1, length);
    bool pass = true;
    bool conditional = true;
    switch (st.condition) {
    case TrigCondNone: conditional = false; break;
    case TrigCondFill: pass = s.fill; break;
    case TrigCondNotFill: pass = !s.fill; break;
    case TrigCondPre:
        pass = trigLastCond[t];
        conditional = false;
        break;
    case TrigCondNotPre:
        pass = !trigLastCond[t];
        conditional = false;
        break;
    case TrigCondNei: pass = trigLastCond[(t + TrigTracks - 1) % TrigTracks]; break;
    case TrigCondNotNei: pass = !trigLastCond[(t + TrigTracks - 1) % TrigTracks]; break;
    case TrigCondFirst: pass = loop == 0; break;
    case TrigCondNotFirst: pass = loop > 0; break;
    default: {
        int a = 1, b = 1;
        if (trigConditionCycle(st.condition, a, b)) pass = (loop % b) == a - 1;
        break;
    }
    }
    if (pass && st.probability < 100) {
        if (st.probability <= 0)
            pass = false;
        else {
            randomState = randomState * 1664525u + 1013904223u;
            pass = int((randomState >> 16) % 100u) < st.probability;
        }
        conditional = true;
    }
    if (conditional) trigLastCond[t] = pass;
    if (!pass || muted) return;
    trigFlashBlocks[t] = 8;
    glideCount[size_t(t)] = 0;
    startSlides();
    if (tr.lfo.mode != LfoFree) {
        lfoPhase[size_t(t)] = 0;
        lfoDone[size_t(t)] = false;
    }
    const int velocity = std::clamp(int(st.velocity), 1, 127);
    const int hits = std::clamp(int(st.retrig), 1, 8);
    if (tr.type == TrigTrackType::Midi) {
        const int channel = tr.midiChannel & 15;
        const int note = std::clamp(st.note >= 0 ? int(st.note) : int(tr.defaultNote), 0, 127);
        for (int c = 0; c < TrigMidiCcs; ++c) {
            const int index = st.findLock(TrigCcFirst + c);
            const int value =
                index >= 0 ? std::clamp(int(std::lround(st.locks[index].value * 127. / TrigLockResolution)), 0, 127) : tr.ccValue[c];
            if (value >= 0 && value != trigSentCc[t][c]) {
                pushMidi(frame, uint32_t(0xB0 | channel) | (uint32_t(tr.ccNumber[c] & 127) << 8) | (uint32_t(value) << 16));
                trigSentCc[t][c] = value;
            }
        }
        const double sub = stepSamples / hits;
        for (int hit = 0; hit < hits; ++hit) {
            const uint64_t on = frame + uint64_t(hit * sub);
            const double gate = hits == 1 ? std::max(1., st.length * stepSamples - 64.) : sub * .75;
            if (activeNotes[channel][note]) pushMidi(on, uint32_t(0x80 | channel) | (uint32_t(note) << 8));
            pushMidi(on, uint32_t(0x90 | channel) | (uint32_t(note) << 8) | (uint32_t(velocity) << 16));
            pushMidi(on + uint64_t(gate), uint32_t(0x80 | channel) | (uint32_t(note) << 8));
        }
        activeNotes[channel][note] = true; // cleared by the note-off sweep on stop
        return;
    }
    // Audio track: base sound + this step's locks, for this trig only.
    PadEdit edit = banks[s.program][t].edit;
    resetLocks(t);
    for (int i = 0; i < st.lockCount; ++i) {
        const int param = st.locks[i].param;
        const double v = st.locks[i].value / double(TrigLockResolution);
        if (param < TrigFxFirst) {
            trigParamApply(edit, param, v);
            if (param == TrigPan) lockedPan[t] = edit.pan;
        } else if (param < TrigCcFirst)
            lockedFx[t][(param - TrigFxFirst) / 6][(param - TrigFxFirst) % 6] = st.locks[i].value;
        else if (param == TrigSendReverb)
            lockedSend[size_t(t)][0] = v;
        else if (param == TrigSendDelay)
            lockedSend[size_t(t)][1] = v;
    }
    trigEdit[t] = edit;
    trigEditValid[t] = true;
    // Scene values win over the step locks while the crossfader holds them.
    for (int param = 0; param < TrigFxFirst; ++param) {
        const double v = sceneOverride(t, param);
        if (!std::isnan(v)) trigParamApply(edit, param, v);
    }
    players[t]->edit = edit;
    liveTouched[t] = false;
    // Sample-accurate start inside this block; retrigs wait for their own frame.
    const uint64_t offset = frame > blockFrame ? std::min<uint64_t>(frame - blockFrame, Block - 1) : 0;
    players[t]->nextStartDelay = uint32_t(offset);
    triggerPad(t, 0, velocity);
    ratchetRemaining[t] = 0;
    hitCount[size_t(t)] = 0;
    for (int hit = 1; hit < hits && hit < 9; ++hit)
        this->hits[size_t(t)][size_t(hitCount[size_t(t)]++)] = {frame + uint64_t(hit * stepSamples / hits), velocity};
    gateSamples[t] = std::max(1, int(st.length)) * stepSamples + double(offset);
}
void Engine::Impl::trigSchedule(double from, double to, bool anySolo) {
    auto& s = state;
    const auto& sequence = s.sequences[s.sequence];
    const auto& pattern = sequence.trig;
    const double swing = (std::clamp(pattern.swing, 50, 80) - 50) / 50.; // fraction of a step for odd steps
    for (int t = 0; t < TrigTracks; ++t) {
        const auto& tr = pattern.tracks[t];
        const int length = std::clamp(int(tr.length), 1, TrigMaxSteps);
        const double speed = trigSpeedFactor(tr.speed);
        const double a = (from - trigOrigin) * speed, b = (to - trigOrigin) * speed;
        if (b <= 0) continue;
        const long long first = std::max(0LL, (long long)std::floor(a) - 1), last = (long long)std::ceil(b) + 1;
        for (long long k = first; k <= last; ++k) {
            const auto& st = tr.steps[size_t(k % length)];
            if (st.empty()) continue;
            double position = k + st.micro / 24.;
            if (k % 2 == 1) position += swing;
            if (position < a || position >= b || position < 0) continue;
            const double absolute = trigOrigin + position / speed;
            const uint64_t frame = blockFrame + uint64_t(std::max(0., (absolute - blockStartSteps) * samplesPerStep));
            trigFire(t, tr, st, k, length, frame, samplesPerStep / speed, sequence, anySolo);
        }
    }
}
void Engine::Impl::trigReposition(double steps) {
    const auto& pattern = state.sequences[state.sequence].trig;
    trigPos = steps;
    if (pattern.masterLength > 0) {
        trigOrigin = std::floor(steps / pattern.masterLength) * pattern.masterLength;
        trigNextBoundary = trigOrigin + pattern.masterLength;
    } else {
        trigOrigin = 0;
        trigNextBoundary = (std::floor(steps / 16.) + 1.) * 16.;
    }
    clockPhase = std::ceil(steps * 6.) / 6.;
    glideCount.fill(0);
}
void Engine::Impl::trigRender(bool anySolo) {
    auto& s = state;
    if (s.hostSync) {
        s.tempo = std::clamp(hostBpm, 20., 400.);
        if (hostPlaying && !s.playing)
            s.playing = true;
        else if (!hostPlaying && s.playing) {
            s.playing = false;
            s.songPlaying = false;
        }
    }
    samplesPerStep = Rate * 60. / s.tempo / 4.;
    blockFrame = frames;
    for (int p = 0; p < Pads; ++p)
        if (gateSamples[p] > 0) {
            gateSamples[p] -= Block;
            if (gateSamples[p] <= 0 && banks[s.program][p].edit.samplePlay == SamplePlayType::NoteOn) players[p]->release();
        }
    for (int t = 0; t < TrigTracks; ++t) {
        if (trigFlashBlocks[t] > 0) --trigFlashBlocks[t];
        s.trigFlash[t] = trigFlashBlocks[t] > 0;
    }
    trigLive();
    if (!s.playing) {
        if (trigWasPlaying) trigStopped(frames);
        return;
    }
    if (!trigWasPlaying) {
        trigStart();
        if (s.hostSync) trigReposition(hostSteps);
    } else if (s.hostSync && std::abs(trigPos - hostSteps) > .05)
        trigReposition(hostSteps); // loop or jump in the host
    const double a = trigPos, b = trigPos + Block / samplesPerStep;
    blockStartSteps = a;
    if (s.midiClockOut)
        while (clockPhase < b) {
            pushMidi(blockFrame + uint64_t(std::max(0., (clockPhase - a) * samplesPerStep)), 0xF8);
            clockPhase += 1. / 6.;
        }
    double cursor = a;
    for (int guard = 0; guard < 8; ++guard) {
        const double end = std::min(b, trigNextBoundary);
        if (end > cursor) trigSchedule(cursor, end, anySolo);
        if (trigNextBoundary <= b && s.playing) {
            cursor = std::max(cursor, trigNextBoundary);
            trigBoundary();
            continue;
        }
        break;
    }
    trigPos = b;
    for (int t = 0; t < TrigTracks; ++t) {
        int& n = hitCount[size_t(t)];
        for (int i = 0; i < n; ++i) {
            auto& h = hits[size_t(t)][size_t(i)];
            if (h.frame >= blockFrame + Block) continue;
            players[size_t(t)]->nextStartDelay = uint32_t(h.frame > blockFrame ? h.frame - blockFrame : 0);
            triggerPad(t, 0, h.velocity);
            h = hits[size_t(t)][size_t(n - 1)];
            --n;
            --i;
        }
    }
    const auto& pattern = s.sequences[s.sequence].trig;
    for (int t = 0; t < TrigTracks; ++t) {
        const auto& tr = pattern.tracks[t];
        const int length = std::clamp(int(tr.length), 1, TrigMaxSteps);
        const double rel = (a - trigOrigin) * trigSpeedFactor(tr.speed);
        s.trigPlayStep[t] = rel < 0 ? -1 : int((long long)std::floor(rel) % length);
    }
    const long long base = (long long)std::floor(a - trigOrigin);
    const int bars = std::max(1, s.sequences[s.sequence].bars);
    s.step = int(base % Steps);
    s.playBar = int((base / Steps) % bars);
}

// ---------------------------------------------------------------- scenes and slides
void Engine::Impl::addGlide(int track, int param, double from, double to, double samples) {
    auto& g = glides[size_t(track)];
    int& n = glideCount[size_t(track)];
    for (int i = 0; i < n; ++i)
        if (g[size_t(i)].param == param) {
            g[size_t(i)] = {uint8_t(param), from, to, 0, std::max(1., samples)};
            return;
        }
    if (n < TrigLocksPerStep) g[size_t(n++)] = {uint8_t(param), from, to, 0, std::max(1., samples)};
}
void Engine::Impl::trigLive() {
    auto& s = state;
    const auto& pattern = s.sequences[s.sequence].trig;
    const double x = std::clamp(s.crossfader, 0., 1.);
    for (auto& t : sceneValue) t.fill(std::numeric_limits<double>::quiet_NaN());
    const auto& A = pattern.scenes[size_t(std::clamp(pattern.sceneA, 0, TrigScenes - 1))];
    const auto& B = pattern.scenes[size_t(std::clamp(pattern.sceneB, 0, TrigScenes - 1))];
    auto base = [&](int t, int param) -> double { return trigTrackBase(banks[s.program][t].edit, pattern.tracks[size_t(t)], param); };
    if (A.count || B.count) {
        for (int i = 0; i < A.count; ++i) {
            const auto& l = A.locks[size_t(i)];
            if (l.track >= TrigTracks || l.param >= TrigParamCount) continue;
            const int other = B.find(l.track, l.param);
            const double a = l.value / double(TrigLockResolution),
                         b = other >= 0 ? B.locks[size_t(other)].value / double(TrigLockResolution) : base(l.track, l.param);
            sceneValue[l.track][l.param] = a + (b - a) * x;
        }
        for (int i = 0; i < B.count; ++i) {
            const auto& l = B.locks[size_t(i)];
            if (l.track >= TrigTracks || l.param >= TrigParamCount || A.find(l.track, l.param) >= 0) continue;
            const double a = base(l.track, l.param), b = l.value / double(TrigLockResolution);
            if (x > 0) sceneValue[l.track][l.param] = a + (b - a) * x;
        }
    }
    for (int t = 0; t < TrigTracks; ++t) {
        // Advance slides.
        std::array<double, TrigParamCount> glideValue;
        glideValue.fill(std::numeric_limits<double>::quiet_NaN());
        auto& g = glides[size_t(t)];
        int& n = glideCount[size_t(t)];
        for (int i = 0; i < n; ++i) {
            auto& e = g[size_t(i)];
            const double r = std::min(1., e.elapsed / e.total);
            glideValue[e.param] = e.from + (e.to - e.from) * r;
            if (s.playing) e.elapsed += Block;
            if (r >= 1.) {
                e = g[size_t(n - 1)];
                --n;
                --i;
            }
        }
        auto sceneOrGlide = [&](int param) {
            const double v = sceneValue[size_t(t)][size_t(param)];
            return std::isnan(v) ? glideValue[size_t(param)] : v;
        };
        const auto& tr = pattern.tracks[size_t(t)];
        // LFO: advances with the track clock, its output is added to one parameter.
        const auto& lfo = tr.lfo;
        double lfoOut = 0;
        {
            const double cycle = trigLfoCycleSteps(lfo.speed);
            const double advance = Block / samplesPerStep * trigSpeedFactor(tr.speed) / cycle;
            double& phase = lfoPhase[size_t(t)];
            if (lfo.mode == LfoOneShot && lfoDone[size_t(t)])
                phase = 1.;
            else {
                phase += advance;
                if (phase >= 1.) {
                    if (lfo.mode == LfoOneShot) {
                        phase = 1.;
                        lfoDone[size_t(t)] = true;
                    } else {
                        phase -= std::floor(phase);
                        lfoRandom = lfoRandom * 1664525u + 1013904223u;
                        lfoHold[size_t(t)] = ((lfoRandom >> 8) & 0xffff) / 32767.5 - 1.;
                    }
                }
            }
            const double p = std::clamp(phase, 0., 1.);
            double w = 0;
            switch (lfo.wave) {
            case LfoSine: w = std::sin(p * 6.283185307179586); break;
            case LfoTriangle: w = 1. - 4. * std::abs(p - .5); break;
            case LfoSawUp: w = 2. * p - 1.; break;
            case LfoSawDown: w = 1. - 2. * p; break;
            case LfoSquare: w = p < .5 ? 1. : -1.; break;
            default: w = lfoHold[size_t(t)]; break;
            }
            lfoOut = w * std::clamp(int(lfo.depth), -100, 100) / 100.;
            s.trigLfo[t] = lfoOut;
        }
        const int lfoTarget = lfo.depth != 0 ? int(lfo.destination) : -1;
        // Value of a parameter: scene > slide, plus the LFO around it (base when nothing else).
        auto value = [&](int param, double base) -> double {
            const double v = sceneOrGlide(param);
            if (param != lfoTarget) return v;
            return std::clamp((std::isnan(v) ? base : v) + lfoOut * .5, 0., 1.);
        };
        if (tr.type == TrigTrackType::Midi) {
            for (int c = 0; c < TrigMidiCcs; ++c) {
                const int param = TrigCcFirst + c;
                if (param == lfoTarget && !s.playing && std::isnan(sceneOrGlide(param))) continue; // the LFO only sends CCs while playing
                const double v = value(param, tr.ccValue[size_t(c)] < 0 ? .5 : tr.ccValue[size_t(c)] / 127.);
                if (std::isnan(v)) continue;
                const int cc = std::clamp(int(std::lround(v * 127.)), 0, 127);
                if (cc != trigSentCc[t][c]) {
                    pushMidi(frames,
                             uint32_t(0xB0 | (tr.midiChannel & 15)) | (uint32_t(tr.ccNumber[size_t(c)] & 127) << 8) | (uint32_t(cc) << 16));
                    trigSentCc[t][c] = cc;
                }
            }
            continue;
        }
        // Audio: live parameters (envelope, level), pan and effects.
        bool any = false;
        PadEdit edit = trigEditValid[t] ? trigEdit[t] : banks[s.program][t].edit;
        const PadEdit& bank = banks[s.program][t].edit;
        for (int param : {int(TrigAttack), int(TrigDecay), int(TrigSustain), int(TrigRelease), int(TrigLevel)}) {
            const double v = value(param, trigParamBase(edit, param));
            if (!std::isnan(v)) {
                trigParamApply(edit, param, v);
                any = true;
            }
        }
        if (any) {
            edit.semitones = players[t]->edit.semitones;
            edit.start = players[t]->edit.start;
            edit.end = players[t]->edit.end;
            players[t]->edit = edit;
            liveTouched[t] = true;
        } else if (liveTouched[t]) {
            edit.semitones = players[t]->edit.semitones;
            edit.start = players[t]->edit.start;
            edit.end = players[t]->edit.end;
            players[t]->edit = edit;
            liveTouched[t] = false;
        }
        for (int k = 0; k < 2; ++k) {
            const int param = k == 0 ? TrigSendReverb : TrigSendDelay;
            const double locked = lockedSend[size_t(t)][size_t(k)];
            liveSend[size_t(t)][size_t(k)] = value(param, std::isnan(locked) ? trigTrackBase(bank, tr, param) : locked);
        }
        const double pan = value(TrigPan, std::isnan(lockedPan[t]) ? (bank.pan + 1.) / 2. : (lockedPan[t] + 1.) / 2.);
        livePan[t] = std::isnan(pan) ? std::numeric_limits<double>::quiet_NaN() : pan * 2. - 1.;
        for (int slot = 0; slot < InsertSlots; ++slot)
            for (int p = 0; p < 6; ++p) {
                const int locked = lockedFx[t][size_t(slot)][size_t(p)];
                const double v = value(trigFxParam(slot, p), locked >= 0 ? locked / double(TrigLockResolution)
                                                                         : bank.inserts[size_t(slot)].parameters[size_t(p)]);
                liveFx[t][size_t(slot)][size_t(p)] = std::isnan(v) ? -1 : int(std::lround(v * TrigLockResolution));
            }
    }
}

// ---------------------------------------------------------------- editing API
namespace {
TrigStep* trigStepAt(Snapshot& state, int track, int step) {
    if (track < 0 || track >= TrigTracks || step < 0 || step >= TrigMaxSteps) return nullptr;
    return &state.sequences[state.sequence].trig.tracks[track].steps[step];
}
} // namespace
void Engine::trigToggle(int track, int step) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto* target = trigStepAt(impl->state, track, step);
    if (!target) return;
    const auto& tr = impl->state.sequences[impl->state.sequence].trig.tracks[track];
    if (target->trig) {
        *target = TrigStep{};
    } else {
        target->trig = true;
        target->velocity = tr.defaultVelocity;
        target->length = tr.defaultLength;
    }
}
void Engine::trigSetStep(int track, int step, const TrigStep& requested) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto* target = trigStepAt(impl->state, track, step);
    if (!target) return;
    TrigStep value = requested;
    value.velocity = uint8_t(std::clamp(int(value.velocity), 1, 127));
    value.length = uint8_t(std::clamp(int(value.length), 1, TrigMaxSteps));
    value.probability = uint8_t(std::clamp(int(value.probability), 0, 100));
    value.condition = uint8_t(std::clamp(int(value.condition), 0, trigConditionCount() - 1));
    value.micro = int8_t(std::clamp(int(value.micro), -23, 23));
    value.retrig = uint8_t(std::clamp(int(value.retrig), 1, 8));
    value.note = int8_t(std::clamp(int(value.note), -1, 127));
    value.lockCount = uint8_t(std::clamp(int(value.lockCount), 0, TrigLocksPerStep));
    *target = value;
}
bool Engine::trigSetLock(int track, int step, int param, double normalised) {
    if (param < 0 || param >= TrigParamCount || !std::isfinite(normalised)) return false;
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto* target = trigStepAt(impl->state, track, step);
    if (!target) return false;
    const int16_t value = int16_t(std::lround(std::clamp(normalised, 0., 1.) * TrigLockResolution));
    const int index = target->findLock(param);
    if (index >= 0) {
        target->locks[index].value = value;
        return true;
    }
    if (target->lockCount >= TrigLocksPerStep) return false;
    target->locks[target->lockCount++] = {uint8_t(param), value};
    return true;
}
void Engine::trigClearLock(int track, int step, int param) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto* target = trigStepAt(impl->state, track, step);
    if (!target) return;
    const int index = target->findLock(param);
    if (index < 0) return;
    for (int i = index; i + 1 < target->lockCount; ++i) target->locks[i] = target->locks[i + 1];
    target->locks[--target->lockCount] = {};
}
void Engine::trigClearLocks(int track, int step) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto* target = trigStepAt(impl->state, track, step);
    if (!target) return;
    target->lockCount = 0;
    target->locks = {};
}
void Engine::trigSetTrackConfig(int track, const TrigTrack& config) {
    if (track < 0 || track >= TrigTracks) return;
    std::scoped_lock lock(impl->mutex, impl->audioMutex);
    auto& tr = impl->state.sequences[impl->state.sequence].trig.tracks[track];
    const bool typeChanged = tr.type != config.type || tr.midiChannel != config.midiChannel;
    const auto steps = tr.steps;
    tr = config;
    tr.steps = steps;
    tr.length = uint8_t(std::clamp(int(tr.length), 1, TrigMaxSteps));
    tr.speed = uint8_t(std::clamp(int(tr.speed), 0, TrigSpeeds - 1));
    tr.midiChannel = uint8_t(tr.midiChannel & 15);
    tr.defaultNote = uint8_t(std::clamp(int(tr.defaultNote), 0, 127));
    tr.defaultVelocity = uint8_t(std::clamp(int(tr.defaultVelocity), 1, 127));
    tr.defaultLength = uint8_t(std::clamp(int(tr.defaultLength), 1, TrigMaxSteps));
    tr.sendReverb = int16_t(std::clamp(int(tr.sendReverb), 0, TrigLockResolution));
    tr.sendDelay = int16_t(std::clamp(int(tr.sendDelay), 0, TrigLockResolution));
    for (auto& number : tr.ccNumber) number = uint8_t(number & 127);
    for (auto& value : tr.ccValue) value = int16_t(std::clamp(int(value), -1, 127));
    if (typeChanged) {
        impl->trigSentCc[track].fill(-1);
        if (tr.type == TrigTrackType::Midi) impl->players[track]->stop();
    }
}
void Engine::trigSetTrack(int track, const TrigTrack& complete) {
    if (track < 0 || track >= TrigTracks) return;
    trigSetTrackConfig(track, complete);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.sequences[impl->state.sequence].trig.tracks[track].steps = complete.steps;
}
void Engine::trigClearTrack(int track) {
    if (track < 0 || track >= TrigTracks) return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.sequences[impl->state.sequence].trig.tracks[track].steps = {};
}
void Engine::trigSetMasterLength(int length) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto& pattern = impl->state.sequences[impl->state.sequence].trig;
    pattern.masterLength = std::clamp(length, 0, TrigMaxSteps);
    impl->state.sequences[impl->state.sequence].bars = std::clamp((std::max(16, pattern.masterLength) + 15) / 16, 1, MaxBars);
}
void Engine::trigSetSwing(int swing) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.sequences[impl->state.sequence].trig.swing = std::clamp(swing, 50, 80);
}
void Engine::trigSetCcBase(int track, int cc, int value) {
    if (track < 0 || track >= TrigTracks || cc < 0 || cc >= TrigMidiCcs) return;
    value = std::clamp(value, -1, 127);
    uint8_t channel = 0, number = 0;
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        auto& tr = impl->state.sequences[impl->state.sequence].trig.tracks[track];
        tr.ccValue[cc] = int16_t(value);
        channel = tr.midiChannel;
        number = tr.ccNumber[cc];
    }
    if (value >= 0) {
        sendMidi(uint32_t(0xB0 | channel) | (uint32_t(number) << 8) | (uint32_t(value) << 16));
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->trigSentCc[track][cc] = value;
    }
}

void Engine::trigSetPattern(int sequence, const TrigPattern& pattern) {
    if (sequence < 0 || sequence >= Sequences) return;
    std::scoped_lock lock(impl->mutex, impl->audioMutex);
    impl->state.sequences[sequence].trig = pattern;
    auto& p = impl->state.sequences[sequence].trig;
    p.masterLength = std::clamp(p.masterLength, 0, TrigMaxSteps);
    p.swing = std::clamp(p.swing, 50, 80);
    p.sceneA = std::clamp(p.sceneA, 0, TrigScenes - 1);
    p.sceneB = std::clamp(p.sceneB, 0, TrigScenes - 1);
}
bool Engine::trigSetSceneValue(int scene, int track, int param, double normalised) {
    if (scene < 0 || scene >= TrigScenes || track < 0 || track >= TrigTracks || param < 0 || param >= TrigParamCount ||
        !std::isfinite(normalised))
        return false;
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto& sc = impl->state.sequences[impl->state.sequence].trig.scenes[size_t(scene)];
    const int16_t value = int16_t(std::lround(std::clamp(normalised, 0., 1.) * TrigLockResolution));
    const int at = sc.find(track, param);
    if (at >= 0) {
        sc.locks[size_t(at)].value = value;
        return true;
    }
    if (sc.count >= TrigSceneLocks) return false;
    sc.locks[sc.count++] = {uint8_t(track), uint8_t(param), value};
    return true;
}
void Engine::trigClearSceneValue(int scene, int track, int param) {
    if (scene < 0 || scene >= TrigScenes) return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto& sc = impl->state.sequences[impl->state.sequence].trig.scenes[size_t(scene)];
    const int at = sc.find(track, param);
    if (at < 0) return;
    for (int i = at; i + 1 < sc.count; ++i) sc.locks[size_t(i)] = sc.locks[size_t(i + 1)];
    sc.locks[--sc.count] = {};
}
void Engine::trigClearScene(int scene) {
    if (scene < 0 || scene >= TrigScenes) return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.sequences[impl->state.sequence].trig.scenes[size_t(scene)] = TrigScene{};
}
void Engine::trigSetSceneAB(int a, int b) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto& p = impl->state.sequences[impl->state.sequence].trig;
    p.sceneA = std::clamp(a, 0, TrigScenes - 1);
    p.sceneB = std::clamp(b, 0, TrigScenes - 1);
}
void Engine::setCrossfader(double position) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.crossfader = std::clamp(std::isfinite(position) ? position : 0., 0., 1.);
}
void Engine::trigSetLfo(int track, const TrigLfo& requested) {
    if (track < 0 || track >= TrigTracks) return;
    TrigLfo lfo = requested;
    lfo.wave = uint8_t(std::min<int>(lfo.wave, LfoWaves - 1));
    lfo.speed = uint8_t(std::min<int>(lfo.speed, TrigLfoSpeeds - 1));
    lfo.mode = uint8_t(std::min<int>(lfo.mode, LfoModes - 1));
    if (lfo.destination != TrigLfoNone && lfo.destination >= TrigParamCount) lfo.destination = TrigLfoNone;
    lfo.depth = int8_t(std::clamp(int(lfo.depth), -100, 100));
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.sequences[impl->state.sequence].trig.tracks[size_t(track)].lfo = lfo;
}
void Engine::setHostSync(bool enabled) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.hostSync = enabled;
    if (enabled) impl->state.midiClockOut = false;
}
void Engine::setHostTransport(bool playing, double quarterNotes, double bpm) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->hostPlaying = playing;
    if (std::isfinite(quarterNotes)) impl->hostSteps = quarterNotes * 4.;
    if (std::isfinite(bpm) && bpm > 0) impl->hostBpm = bpm;
}
void Engine::setMultiOut(bool enabled) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->multiOut = enabled;
}
void Engine::setTrackOutput(int track, int output) {
    if (track < 0 || track >= TrigTracks) return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.trackOutput[size_t(track)] = uint8_t(std::clamp(output, 0, 8));
}
std::array<uint8_t, TrigTracks> Engine::trackOutputs() {
    std::lock_guard<std::mutex> lock(impl->mutex);
    return impl->state.trackOutput;
}
void Engine::trackBlock(int pad, float* left, float* right) const {
    if (pad < 0 || pad >= Pads) return;
    const auto& capture = impl->players[size_t(pad)]->capture;
    std::copy(capture[0].begin(), capture[0].end(), left);
    std::copy(capture[1].begin(), capture[1].end(), right);
}
void Engine::setSendFx(const SendFx& requested) {
    SendFx fx = requested;
    fx.reverbDecay = std::clamp(fx.reverbDecay, 0., 1.);
    fx.reverbSize = std::clamp(fx.reverbSize, 0., 1.);
    fx.reverbTone = std::clamp(fx.reverbTone, 0., 1.);
    fx.reverbReturn = std::clamp(fx.reverbReturn, 0., 1.);
    fx.delayDivision = std::clamp(fx.delayDivision, 0, DelayDivisions - 1);
    fx.delayFeedback = std::clamp(fx.delayFeedback, 0., .95);
    fx.delayTone = std::clamp(fx.delayTone, 0., 1.);
    fx.delayReturn = std::clamp(fx.delayReturn, 0., 1.);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.sendFx = fx;
}
void Engine::setFill(bool active) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.fill = active;
}
void Engine::setMidiClockOut(bool enabled) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->state.midiClockOut = enabled;
}
void Engine::sendMidi(uint32_t message) { impl->pushMidi(0, message); }
bool Engine::popMidi(MidiEvent& event) {
    std::lock_guard<std::mutex> lock(impl->midiMutex);
    if (impl->midiCount == 0) return false;
    event = impl->midiRing[impl->midiHead];
    impl->midiHead = (impl->midiHead + 1) % Impl::MidiRing;
    --impl->midiCount;
    return true;
}
uint64_t Engine::renderedFrames() const { return impl->frames.load(); }
void Engine::trigPlayLive(int track, int note, int velocity, bool pressed, bool record) {
    if (track < 0 || track >= TrigTracks) return;
    velocity = std::clamp(velocity, 1, 127);
    TrigTrack config;
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        config = impl->state.sequences[impl->state.sequence].trig.tracks[track];
    }
    const int midiNote = std::clamp(note >= 0 ? note : int(config.defaultNote), 0, 127);
    if (config.type == TrigTrackType::Midi) {
        const uint32_t channel = config.midiChannel & 15;
        sendMidi(pressed ? ((0x90 | channel) | (uint32_t(midiNote) << 8) | (uint32_t(velocity) << 16))
                         : ((0x80 | channel) | (uint32_t(midiNote) << 8)));
    } else {
        if (pressed)
            triggerVelocity(track, note >= 0 ? note - 60 : 0, velocity);
        else
            release(track);
    }
    if (!pressed || !record) return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    auto& s = impl->state;
    if (!s.playing) return;
    auto& tr = s.sequences[s.sequence].trig.tracks[track];
    const int length = std::clamp(int(tr.length), 1, TrigMaxSteps);
    const double rel = (impl->trigPos - impl->trigOrigin) * trigSpeedFactor(tr.speed);
    if (rel < 0) return;
    auto& st = tr.steps[size_t((long long)std::llround(rel) % length)];
    st.trig = true;
    st.velocity = uint8_t(velocity);
    st.length = tr.defaultLength;
    if (tr.type == TrigTrackType::Midi)
        st.note = int8_t(midiNote);
    else if (note >= 0 && note != 60) {
        const double v = std::clamp((note - 60 + 24) / 48., 0., 1.);
        const int16_t value = int16_t(std::lround(v * TrigLockResolution));
        const int index = st.findLock(TrigPitch);
        if (index >= 0)
            st.locks[index].value = value;
        else if (st.lockCount < TrigLocksPerStep)
            st.locks[st.lockCount++] = {TrigPitch, value};
    }
}
void Engine::trigImportKickSteps() {
    std::lock_guard<std::mutex> lock(impl->mutex);
    for (auto& sequence : impl->state.sequences) {
        auto& pattern = sequence.trig;
        pattern = TrigPattern{};
        const int total = std::clamp(sequence.bars, 1, MaxBars) * Steps;
        for (int t = 0; t < TrigTracks; ++t) {
            auto& tr = pattern.tracks[t];
            tr.length = uint8_t(total);
            for (int i = 0; i < total; ++i) {
                if (!sequence.drum[t][i]) continue;
                const auto& event = sequence.drumEvents[t][i];
                auto& st = tr.steps[size_t(i)];
                st.trig = true;
                st.velocity = uint8_t(std::clamp(event.velocity, 1, 127));
                st.probability = uint8_t(std::clamp(event.probability, 0, 100));
                st.retrig = uint8_t(std::clamp(event.ratchet, 1, 8));
                st.length = uint8_t(std::clamp(event.length, 1, 16));
            }
        }
        const int source = std::clamp(sequence.sourcePad, 0, TrigTracks - 1);
        auto& melodic = pattern.tracks[source];
        for (int i = 0; i < total; ++i) {
            const int note = sequence.melody[i];
            if (note < 0) continue;
            auto& st = melodic.steps[size_t(i)];
            if (st.trig) continue;
            const auto& event = sequence.melodyEvents[i];
            st.trig = true;
            st.velocity = uint8_t(std::clamp(event.velocity, 1, 127));
            st.probability = uint8_t(std::clamp(event.probability, 0, 100));
            st.retrig = uint8_t(std::clamp(event.ratchet, 1, 8));
            st.length = uint8_t(std::clamp(event.length, 1, 16));
            const double v = std::clamp((note - sequence.rootNote + 24) / 48., 0., 1.);
            st.locks[0] = {TrigPitch, int16_t(std::lround(v * TrigLockResolution))};
            st.lockCount = 1;
        }
    }
}

// ---------------------------------------------------------------- persistence
namespace {
void writeTrigSection(std::ostream& f, const Snapshot& s) {
    f << "TRIGDATA 5 " << Sequences << '\n';
    {
        const auto& fx = s.sendFx;
        f << "FX " << fx.reverbDecay << ' ' << fx.reverbSize << ' ' << fx.reverbTone << ' ' << fx.reverbReturn << ' ' << fx.delayDivision
          << ' ' << fx.delayFeedback << ' ' << fx.delayTone << ' ' << fx.delayReturn << ' ' << fx.delayPingPong << '\n';
    }
    f << "TO";
    for (int t = 0; t < TrigTracks; ++t) f << ' ' << int(s.trackOutput[size_t(t)]);
    f << '\n';
    for (int index = 0; index < Sequences; ++index) {
        const auto& pattern = s.sequences[index].trig;
        f << "TP " << index << ' ' << pattern.masterLength << ' ' << pattern.swing << ' ' << pattern.sceneA << ' ' << pattern.sceneB
          << '\n';
        for (int t = 0; t < TrigTracks; ++t) {
            const auto& tr = pattern.tracks[t];
            f << "TT " << t << ' ' << int(tr.type) << ' ' << int(tr.length) << ' ' << int(tr.speed) << ' ' << int(tr.midiChannel) << ' '
              << int(tr.defaultNote) << ' ' << int(tr.defaultVelocity) << ' ' << int(tr.defaultLength);
            for (int c = 0; c < TrigMidiCcs; ++c) f << ' ' << int(tr.ccNumber[c]) << ' ' << tr.ccValue[c];
            f << ' ' << int(tr.lfo.wave) << ' ' << int(tr.lfo.speed) << ' ' << int(tr.lfo.mode) << ' ' << int(tr.lfo.destination) << ' '
              << int(tr.lfo.depth) << ' ' << tr.sendReverb << ' ' << tr.sendDelay << '\n';
            for (int i = 0; i < TrigMaxSteps; ++i) {
                const auto& st = tr.steps[size_t(i)];
                if (st.empty()) continue;
                f << "TS " << i << ' ' << st.trig << ' ' << int(st.note) << ' ' << int(st.velocity) << ' ' << int(st.length) << ' '
                  << int(st.probability) << ' ' << int(st.condition) << ' ' << int(st.micro) << ' ' << int(st.retrig) << ' '
                  << int(st.slide) << ' ' << int(st.lockCount);
                for (int l = 0; l < st.lockCount; ++l) f << ' ' << int(st.locks[l].param) << ' ' << st.locks[l].value;
                f << '\n';
            }
            f << "TE\n";
        }
        int used = 0;
        for (const auto& scene : pattern.scenes)
            if (scene.count) ++used;
        f << "SCN " << used << '\n';
        for (int sc = 0; sc < TrigScenes; ++sc) {
            const auto& scene = pattern.scenes[size_t(sc)];
            if (!scene.count) continue;
            f << "SC " << sc << ' ' << int(scene.count);
            for (int i = 0; i < scene.count; ++i)
                f << ' ' << int(scene.locks[size_t(i)].track) << ' ' << int(scene.locks[size_t(i)].param) << ' '
                  << scene.locks[size_t(i)].value;
            f << '\n';
        }
    }
    f << "TRIGEND\n";
}
bool readTrigSection(std::istream& f, std::array<TrigPattern, Sequences>& patterns, std::array<uint8_t, TrigTracks>& outputs,
                     SendFx& sendFx) {
    std::string tag;
    int version = 0, count = 0;
    if (!(f >> tag >> version >> count) || tag != "TRIGDATA" || version < 1 || version > 5 || count != Sequences) return false;
    sendFx = SendFx{};
    if (version >= 5) {
        int ping = 1;
        if (!(f >> tag >> sendFx.reverbDecay >> sendFx.reverbSize >> sendFx.reverbTone >> sendFx.reverbReturn >> sendFx.delayDivision >>
              sendFx.delayFeedback >> sendFx.delayTone >> sendFx.delayReturn >> ping) ||
            tag != "FX")
            return false;
        sendFx.delayPingPong = ping != 0;
        sendFx.delayDivision = std::clamp(sendFx.delayDivision, 0, DelayDivisions - 1);
    }
    outputs.fill(0);
    if (version >= 4) {
        if (!(f >> tag) || tag != "TO") return false;
        for (auto& o : outputs) {
            int v = 0;
            if (!(f >> v)) return false;
            o = uint8_t(std::clamp(v, 0, 8));
        }
    }
    for (int index = 0; index < Sequences; ++index) {
        int parsed = -1;
        auto& pattern = patterns[index];
        pattern = TrigPattern{};
        if (!(f >> tag >> parsed >> pattern.masterLength >> pattern.swing) || tag != "TP" || parsed != index) return false;
        if (version >= 2 && !(f >> pattern.sceneA >> pattern.sceneB)) return false;
        pattern.sceneA = std::clamp(pattern.sceneA, 0, TrigScenes - 1);
        pattern.sceneB = std::clamp(pattern.sceneB, 0, TrigScenes - 1);
        pattern.masterLength = std::clamp(pattern.masterLength, 0, TrigMaxSteps);
        pattern.swing = std::clamp(pattern.swing, 50, 80);
        for (int t = 0; t < TrigTracks; ++t) {
            auto& tr = pattern.tracks[t];
            int type = 0, length = 16, speed = 1, channel = 0, note = 60, velocity = 100, gate = 1;
            if (!(f >> tag >> parsed >> type >> length >> speed >> channel >> note >> velocity >> gate) || tag != "TT" || parsed != t)
                return false;
            tr.type = type == 1 ? TrigTrackType::Midi : TrigTrackType::Audio;
            tr.length = uint8_t(std::clamp(length, 1, TrigMaxSteps));
            tr.speed = uint8_t(std::clamp(speed, 0, TrigSpeeds - 1));
            tr.midiChannel = uint8_t(channel & 15);
            tr.defaultNote = uint8_t(std::clamp(note, 0, 127));
            tr.defaultVelocity = uint8_t(std::clamp(velocity, 1, 127));
            tr.defaultLength = uint8_t(std::clamp(gate, 1, TrigMaxSteps));
            for (int c = 0; c < TrigMidiCcs; ++c) {
                int number = 0, value = -1;
                if (!(f >> number >> value)) return false;
                tr.ccNumber[c] = uint8_t(number & 127);
                tr.ccValue[c] = int16_t(std::clamp(value, -1, 127));
            }
            if (version >= 3) {
                int wave = 0, speed = 5, mode = 0, destination = TrigLfoNone, depth = 0;
                if (!(f >> wave >> speed >> mode >> destination >> depth)) return false;
                tr.lfo.wave = uint8_t(std::clamp(wave, 0, LfoWaves - 1));
                tr.lfo.speed = uint8_t(std::clamp(speed, 0, TrigLfoSpeeds - 1));
                tr.lfo.mode = uint8_t(std::clamp(mode, 0, LfoModes - 1));
                tr.lfo.destination = uint8_t(destination >= 0 && destination < TrigParamCount ? destination : TrigLfoNone);
                tr.lfo.depth = int8_t(std::clamp(depth, -100, 100));
            }
            if (version >= 5) {
                int rev = 0, del = 0;
                if (!(f >> rev >> del)) return false;
                tr.sendReverb = int16_t(std::clamp(rev, 0, TrigLockResolution));
                tr.sendDelay = int16_t(std::clamp(del, 0, TrigLockResolution));
            }
            while (f >> tag && tag == "TS") {
                int step = 0, trig = 0, stepNote = -1, stepVelocity = 100, stepLength = 1, probability = 100, condition = 0, micro = 0,
                    retrig = 1, slide = 0, locks = 0;
                if (!(f >> step >> trig >> stepNote >> stepVelocity >> stepLength >> probability >> condition >> micro >> retrig) ||
                    (version >= 2 && !(f >> slide)) || !(f >> locks) || step < 0 || step >= TrigMaxSteps || locks < 0 ||
                    locks > TrigLocksPerStep)
                    return false;
                auto& st = tr.steps[size_t(step)];
                st.trig = trig != 0;
                st.note = int8_t(std::clamp(stepNote, -1, 127));
                st.velocity = uint8_t(std::clamp(stepVelocity, 1, 127));
                st.length = uint8_t(std::clamp(stepLength, 1, TrigMaxSteps));
                st.probability = uint8_t(std::clamp(probability, 0, 100));
                st.condition = uint8_t(std::clamp(condition, 0, trigConditionCount() - 1));
                st.micro = int8_t(std::clamp(micro, -23, 23));
                st.retrig = uint8_t(std::clamp(retrig, 1, 8));
                st.slide = slide != 0;
                st.lockCount = uint8_t(locks);
                for (int l = 0; l < locks; ++l) {
                    int param = 0, value = 0;
                    if (!(f >> param >> value) || param < 0 || param >= TrigParamCount) return false;
                    st.locks[size_t(l)] = {uint8_t(param), int16_t(std::clamp(value, 0, TrigLockResolution))};
                }
            }
            if (tag != "TE") return false;
        }
        if (version >= 2) {
            int used = 0;
            if (!(f >> tag >> used) || tag != "SCN" || used < 0 || used > TrigScenes) return false;
            for (int u = 0; u < used; ++u) {
                int sc = 0, n = 0;
                if (!(f >> tag >> sc >> n) || tag != "SC" || sc < 0 || sc >= TrigScenes || n < 0 || n > TrigSceneLocks) return false;
                auto& scene = pattern.scenes[size_t(sc)];
                scene.count = uint8_t(n);
                for (int i = 0; i < n; ++i) {
                    int t = 0, param = 0, value = 0;
                    if (!(f >> t >> param >> value) || t < 0 || t >= TrigTracks || param < 0 || param >= TrigParamCount) return false;
                    scene.locks[size_t(i)] = {uint8_t(t), uint8_t(param), int16_t(std::clamp(value, 0, TrigLockResolution))};
                }
            }
        }
    }
    return f >> tag && tag == "TRIGEND";
}
} // namespace
