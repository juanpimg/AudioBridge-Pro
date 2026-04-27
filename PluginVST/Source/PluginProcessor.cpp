/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioBridgeAudioProcessor::AudioBridgeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    // Iniciamos el socket para enviar datos (el puerto 0 significa que Windows asigne uno libre localmente)
    udpSocket.bindToPort(0);
}

AudioBridgeAudioProcessor::~AudioBridgeAudioProcessor()
{

}

//==============================================================================
const juce::String AudioBridgeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioBridgeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioBridgeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioBridgeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioBridgeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioBridgeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioBridgeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioBridgeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AudioBridgeAudioProcessor::getProgramName (int index)
{
    return {};
}

void AudioBridgeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AudioBridgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void AudioBridgeAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioBridgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AudioBridgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // 1. Limpieza de seguridad estándar de JUCE
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), NUM_CHANNELS);

    // Si por algún motivo el DAW no envía samples en este bloque, salimos
    if (numSamples == 0 || numChannels == 0) return;

    // 2. Obtener los punteros de lectura de los canales del DAW
    const float* leftChannel = buffer.getReadPointer(0);
    // Si la pista es mono, copiamos el izquierdo al derecho para evitar errores
    const float* rightChannel = (numChannels > 1) ? buffer.getReadPointer(1) : leftChannel;

    // 3. Empaquetar y enviar por Red (UDP)
    for (int i = 0; i < numSamples; ++i) {
        // Entrelazamos el audio en nuestro paquete (L, R, L, R...)
        currentPacket.samples[packetIndex * 2 + 0] = leftChannel[i];
        currentPacket.samples[packetIndex * 2 + 1] = rightChannel[i];
        packetIndex++;

        // Cuando el paquete está lleno (alcanza los 256 samples), lo disparamos
        if (packetIndex >= PACKET_FRAMES) {
            currentPacket.sequenceNumber = sequenceCounter++;
            currentPacket.numFrames = PACKET_FRAMES;

            // Enviar a localhost (127.0.0.1) al puerto 9090
            udpSocket.write("127.0.0.1", UDP_PORT, &currentPacket, sizeof(AudioPacket));

            // Reiniciar el contador para el siguiente paquete
            packetIndex = 0;
        }
    }
}

//==============================================================================
bool AudioBridgeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioBridgeAudioProcessor::createEditor()
{
    return new AudioBridgeAudioProcessorEditor (*this);
}

//==============================================================================
void AudioBridgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AudioBridgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioBridgeAudioProcessor();
}
