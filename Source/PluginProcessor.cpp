#include "PluginProcessor.h"
#include "PluginEditor.h"

XumAudioProcessor::XumAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XUM_STATE", xum::createLayout())
{
    xum::WavetableBank::instance();      // genera las tablas aquí, nunca en el hilo de audio
    refs = std::make_unique<xum::ParamRefs> (apvts);

    // efectos: cada modulo busca sus parametros por id (puede devolver nullptr, p. ej. el EQ no tiene "mix")
    auto getter = [this] (const char* id) -> xum::AP { return apvts.getRawParameterValue (id); };
    convolve.bind (getter);
    fx.bind (getter, &convolve);
    apvts.addParameterListener ("con_ir", this);
}

XumAudioProcessor::~XumAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener ("con_ir", this);
}

void XumAudioProcessor::applyConvolveIR()
{
    const int idx = (int) apvts.getRawParameterValue ("con_ir")->load();
    if (idx == 6 && customIR.existsAsFile()) convolve.loadFile (customIR);
    else                                      convolve.loadInternal (idx == 6 ? 1 : idx);
}

void XumAudioProcessor::loadCustomIR (const juce::File& file)
{
    customIR = file;
    apvts.state.setProperty ("convIR", file.getFullPathName(), nullptr);
    if (auto* p = apvts.getParameter ("con_ir"))
    {
        const bool already = (int) apvts.getRawParameterValue ("con_ir")->load() == 6;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (6.f));
        p->endChangeGesture();
        if (already) triggerAsyncUpdate();
    }
}

void XumAudioProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& v : voices) v.prepare (sampleRate);
    fx.prepare (sampleRate);
    applyConvolveIR();
}

bool XumAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void XumAudioProcessor::noteOn (int note, float velocity)
{
    const int limit = refs->ecoMode->load() > 0.5f ? 8 : xum::kMaxVoices;   // modo eco: 8 voces
    int active = 0;
    xum::Voice* freeVoice = nullptr;
    xum::Voice* oldest = nullptr;
    for (auto& v : voices)
    {
        if (v.isActive())
        {
            ++active;
            if (oldest == nullptr || v.age < oldest->age) oldest = &v;
        }
        else if (freeVoice == nullptr) freeVoice = &v;
    }
    xum::Voice* target = (freeVoice != nullptr && active < limit) ? freeVoice : oldest;   // si no hay hueco, roba la mas antigua
    if (target == nullptr) target = &voices[0];
    target->noteOn (note, velocity, *refs);
    target->age = ++voiceCounter;
}

void XumAudioProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.isActive() && v.isHeld() && v.getNote() == note) v.noteOff();
}

void XumAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                              noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                        noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) v.noteOff();
}

void XumAudioProcessor::renderVoices (juce::AudioBuffer<float>& buffer, int start, int num, double bpm)
{
    if (num <= 0) return;
    float* l = buffer.getWritePointer (0) + start;
    float* r = buffer.getWritePointer (1) + start;
    for (auto& v : voices)
        if (v.isActive()) v.render (l, r, num, *refs, bpm);
}

void XumAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    const int numSamples = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);   // inyecta el piano de la GUI

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    // MIDI con precisión de muestra: se renderiza hasta cada evento y luego se procesa
    int cursor = 0;
    for (const auto meta : midi)
    {
        const int at = std::clamp (meta.samplePosition, 0, numSamples);
        renderVoices (buffer, cursor, at - cursor, bpm);
        cursor = std::max (cursor, at);
        handleMidi (meta.getMessage());
    }
    renderVoices (buffer, cursor, numSamples - cursor, bpm);

    if (fx.anyOn())                                    // los efectos apagados no consumen CPU
        fx.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, bpm);

    const float g = refs->masterGain->load();
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        l[i] = juce::jlimit (-1.f, 1.f, l[i] * g);
        r[i] = juce::jlimit (-1.f, 1.f, r[i] * g);
    }
    scope.push (l, r, numSamples);

    // Publica la modulación de la voz más reciente para animar anillos y pestañas
    const xum::Voice* newest = nullptr;
    for (auto& v : voices)
        if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;
    for (int i = 0; i < xum::Dst::count; ++i)
        modBus.dst[i].store (newest != nullptr ? newest->getLastMod()[i] : 0.f, std::memory_order_relaxed);
    for (int i = 0; i < xum::Src::count; ++i)
        modBus.src[i].store (newest != nullptr ? newest->getLastSrc()[i] : 0.f, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* XumAudioProcessor::createEditor() { return new XumEditor (*this); }

void XumAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void XumAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    customIR = juce::File (apvts.state.getProperty ("convIR").toString());
    triggerAsyncUpdate();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XumAudioProcessor(); }
