#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "Engine/Voice.h"
#include "UI/Scope.h"
#include "Fx/Effects.h"
#include "Fx/ConvolveFx.h"

class XumAudioProcessor : public juce::AudioProcessor,
                          private juce::AsyncUpdater,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    XumAudioProcessor();
    ~XumAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;     // piano de la GUI -> MIDI
    xum::ScopeBuffer scope;                    // audio -> osciloscopio
    xum::ModBus modBus;
    xum::FxChain fx;                           // cadena de efectos (12 modulos)

    // Convolve: IR personalizada (archivo del usuario)
    void loadCustomIR (const juce::File& file);
    juce::String getCustomIRName() const { return customIR.getFileName(); }                        // modulación en vivo -> anillos de la GUI

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { applyConvolveIR(); }
    void applyConvolveIR();
    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void renderVoices (juce::AudioBuffer<float>&, int start, int num, double bpm);

    std::unique_ptr<xum::ParamRefs> refs;
    xum::ConvolveFx convolve;
    juce::File customIR;
    xum::Voice voices[xum::kMaxVoices];
    uint64_t voiceCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XumAudioProcessor)
};
