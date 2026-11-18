#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Log.h"

using namespace steinex;

//==============================================================================
static juce::uint64 packOrder (const std::array<int, kNumModules>& o)
{
    juce::uint64 v = 0;
    for (int i = 0; i < kNumModules; ++i)
        v |= ((juce::uint64) (o[(size_t) i] & 0xF)) << (4 * i);
    return v;
}

static std::array<int, kNumModules> unpackOrder (juce::uint64 v)
{
    std::array<int, kNumModules> o {};
    for (int i = 0; i < kNumModules; ++i)
        o[(size_t) i] = (int) ((v >> (4 * i)) & 0xF);
    return o;
}

static bool isValidOrder (const std::array<int, kNumModules>& o)
{
    std::array<bool, kNumModules> seen {};
    for (int v : o)
    {
        if (v < 0 || v >= kNumModules || seen[(size_t) v]) return false;
        seen[(size_t) v] = true;
    }
    return true;
}

static void stxCrashHandler (void* p)
{
    juce::String s = "CRASH";
   #if JUCE_WINDOWS
    if (p != nullptr)
    {
        auto** ep = (void**) p;
        auto* rec = (const unsigned char*) ep[0];
        if (rec != nullptr)
        {
            const unsigned code = *(const unsigned*) rec;
            void* addr = *(void* const*) (rec + 16);
            s << " code=0x" << juce::String::toHexString ((juce::int64) code)
              << " addr=0x" << juce::String::toHexString ((juce::int64) (juce::pointer_sized_int) addr)
              << " ref=0x" << juce::String::toHexString ((juce::int64) (juce::pointer_sized_int) (void*) &stxCrashHandler);
        }
    }
   #else
    juce::ignoreUnused (p);
   #endif
    steinex::logLine (s);
    steinex::logLine (juce::SystemStats::getStackBacktrace());
}

//==============================================================================
TransitFxProcessor::TransitFxProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STEINEX_TRANSITFX", createLayout())
{
    for (int m = 0; m < kNumModules; ++m)
    {
        modules[(size_t) m] = fx::createModule (m);
        std::vector<std::atomic<float>*> ps;
        for (auto& p : moduleSpecs()[(size_t) m].params)
            ps.push_back (apvts.getRawParameterValue (paramId (m, p.id)));
        modules[(size_t) m]->bind (apvts.getRawParameterValue (onId (m)), std::move (ps));
    }

    pMix    = apvts.getRawParameterValue ("g_mix");
    pOut    = apvts.getRawParameterValue ("g_out");
    pBypass = apvts.getRawParameterValue ("g_bypass");
    pTail   = apvts.getRawParameterValue ("rec_tail");
    pTrim   = apvts.getRawParameterValue ("rec_trim");
    pRecSR  = apvts.getRawParameterValue ("rec_sr");

    std::array<int, kNumModules> o {};
    for (int i = 0; i < kNumModules; ++i) o[(size_t) i] = i;
    orderPacked.store (packOrder (o));
    steinex::installCrashLogger();
    steinex::logLine ("processor: created (v1.0.2)");
}

TransitFxProcessor::~TransitFxProcessor()
{
    steinex::logLine ("processor: destroyed");
    steinex::removeCrashLogger();
}

bool TransitFxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TransitFxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    steinex::logLine ("processor: prepare sr=" + juce::String (sampleRate) + " block=" + juce::String (samplesPerBlock));
    const int maxBlock = juce::jmax (samplesPerBlock, 512);
    for (auto& m : modules) m->prepare (sampleRate, maxBlock);
    dryBuf.setSize (2, maxBlock);
    recorder.prepare (sampleRate);
    mixSm.reset (sampleRate, 0.02);  mixSm.setCurrentAndTargetValue (*pMix * 0.01f);
    outSm.reset (sampleRate, 0.02);  outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((float) *pOut));
}

//==============================================================================
void TransitFxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    static std::atomic<bool> firstBlock { true };
    if (firstBlock.exchange (false))
        steinex::logLine ("processor: first processBlock n=" + juce::String (n) + " ch=" + juce::String (buffer.getNumChannels()));
    if (! loggedFirstBlock.exchange (true))
        steinex::logLine ("processor: first processBlock, n=" + juce::String (n) + " ch=" + juce::String (buffer.getNumChannels()));

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2 || n == 0) return;

    fx::Ctx ctx;
    ctx.sr = currentSR;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) ctx.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
            {
                ctx.ppq = *ppq;
                ctx.ppqValid = pos->getIsPlaying();
            }
        }

    if (dryBuf.getNumSamples() < n) dryBuf.setSize (2, n, false, false, true);

    const bool bypass = pBypass->load() > 0.5f;
    if (! bypass)
    {
        dryBuf.copyFrom (0, 0, buffer, 0, 0, n);
        dryBuf.copyFrom (1, 0, buffer, 1, 0, n);

        const auto order = unpackOrder (orderPacked.load());
        for (int slot = 0; slot < kNumModules; ++slot)
            modules[(size_t) order[(size_t) slot]]->run (buffer, ctx);

        mixSm.setTargetValue (pMix->load() * 0.01f);
        float* wl = buffer.getWritePointer (0);
        float* wr = buffer.getWritePointer (1);
        const float* dl = dryBuf.getReadPointer (0);
        const float* dr = dryBuf.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.getNextValue();
            wl[i] = dl[i] + (wl[i] - dl[i]) * m;
            wr[i] = dr[i] + (wr[i] - dr[i]) * m;
        }

        outSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
        for (int i = 0; i < n; ++i)
        {
            const float g = outSm.getNextValue();
            wl[i] *= g; wr[i] *= g;
        }
    }

    recorder.capture (buffer);
    recorder.mixPreview (buffer);

    const float pl = buffer.getMagnitude (0, 0, n);
    const float pr = buffer.getMagnitude (1, 0, n);
    meterL.store (juce::jmax (pl, meterL.load() * 0.9f));
    meterR.store (juce::jmax (pr, meterR.load() * 0.9f));
}

//==============================================================================
std::array<int, kNumModules> TransitFxProcessor::getOrder() const { return unpackOrder (orderPacked.load()); }

void TransitFxProcessor::setOrder (const std::array<int, kNumModules>& o)
{
    if (isValidOrder (o)) orderPacked.store (packOrder (o));
}

//==============================================================================
void TransitFxProcessor::setParamReal (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void TransitFxProcessor::turnAllModulesOff()
{
    for (int m = 0; m < kNumModules; ++m) setParamReal (onId (m), 0.f);
}

void TransitFxProcessor::resetModuleParams()
{
    for (int m = 0; m < kNumModules; ++m)
    {
        setParamReal (onId (m), 0.f);
        for (auto& p : moduleSpecs()[(size_t) m].params)
            if (auto* ap = apvts.getParameter (paramId (m, p.id)))
                ap->setValueNotifyingHost (ap->getDefaultValue());
    }
}

void TransitFxProcessor::applyScene (const Scene& s)
{
    resetModuleParams();
    for (auto& v : s.vals) setParamReal (v.id, v.v);
}

juce::File TransitFxProcessor::presetsDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("STEINEX").getChildFile ("TRANSIT FX").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::Array<juce::File> TransitFxProcessor::listUserPresets()
{
    return presetsDir().findChildFiles (juce::File::findFiles, false, "*.steinexpreset");
}

bool TransitFxProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;

    juce::XmlElement root ("STEINEXPRESET");
    root.setAttribute ("name", name.trim());
    for (int m = 0; m < kNumModules; ++m)
    {
        auto addP = [&] (const juce::String& id)
        {
            if (auto* p = apvts.getParameter (id))
            {
                auto* e = root.createNewChildElement ("P");
                e->setAttribute ("id", id);
                e->setAttribute ("v", (double) p->convertFrom0to1 (p->getValue()));
            }
        };
        addP (onId (m));
        for (auto& p : moduleSpecs()[(size_t) m].params) addP (paramId (m, p.id));
    }
    return root.writeTo (presetsDir().getChildFile (clean + ".steinexpreset"));
}

bool TransitFxProcessor::loadUserPreset (const juce::File& f)
{
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
    if (xml == nullptr || ! xml->hasTagName ("STEINEXPRESET")) return false;
    resetModuleParams();
    for (auto* e : xml->getChildWithTagNameIterator ("P"))
        setParamReal (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("v"));
    return true;
}

//==============================================================================
void TransitFxProcessor::recordStart()
{
    recorder.prepare (currentSR);
    recorder.startRecording (180.f);
}

void TransitFxProcessor::recordStop()
{
    recorder.stopRecording (pTail->load(), pTrim->load() > 0.5f);
}

int TransitFxProcessor::getExportSampleRate() const
{
    static const int sr[] { 0, 44100, 48000, 88200, 96000 };
    return sr[juce::jlimit (0, 4, (int) std::lround (pRecSR->load()))];
}

//==============================================================================
void TransitFxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    static int cg = 0; if (++cg <= 20) steinex::logLine ("processor: getStateInformation");
    steinex::logLine ("processor: getStateInformation");
    auto state = apvts.copyState();
    juce::String o;
    for (int v : getOrder()) o << v << ",";
    state.setProperty ("order", o, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TransitFxProcessor::setStateInformation (const void* data, int size)
{
    static int cs = 0; if (++cs <= 20) steinex::logLine ("processor: setStateInformation size=" + juce::String (size));
    steinex::logLine ("processor: setStateInformation size=" + juce::String (size));
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto vt = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (vt);

    auto parts = juce::StringArray::fromTokens (vt.getProperty ("order").toString(), ",", "");
    parts.removeEmptyStrings();
    if (parts.size() == kNumModules)
    {
        std::array<int, kNumModules> o {};
        for (int i = 0; i < kNumModules; ++i) o[(size_t) i] = parts[i].getIntValue();
        setOrder (o);
    }
}

juce::AudioProcessorEditor* TransitFxProcessor::createEditor()
{
    steinex::logLine ("processor: createEditor");
    return new TransitFxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TransitFxProcessor(); }
