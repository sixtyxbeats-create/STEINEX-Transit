#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// STEINEX TRANSIT FX - recorder.
// Captures the final processed output of the plugin (all effects, in chain order),
// keeps recording after STOP to catch reverb / delay tails, and writes 24-bit WAV files.
// Rendered files live in Documents/STEINEX/TRANSIT FX/Recordings and are never deleted
// automatically, so FL Studio can keep referencing them.

namespace steinex
{
class TransitRecorder
{
public:
    enum class State { Empty = 0, Recording, Finishing, Ready };

    void prepare (double sr) { sampleRate = sr; }
    double getSampleRate() const { return sampleRate; }

    //============================== message thread
    bool startRecording (float maxSeconds = 180.f)
    {
        stopPreview();
        const juce::SpinLock::ScopedLockType sl (lock);
        capacity = (int) (sampleRate * maxSeconds);
        if (buf.getNumSamples() != capacity)
            buf.setSize (2, capacity, false, true, false);
        length.store (0);
        silentRun = 0; stopLen = 0; tailMax = 0;
        fileSR = 0; renderedFile = juce::File();
        recordedSR = sampleRate;
        state.store ((int) State::Recording);
        return true;
    }

    void stopRecording (float tailSeconds, bool trim)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        trimEnabled = trim;
        if (state.load() != (int) State::Recording) return;
        if (tailSeconds > 0.01f)
        {
            tailMax = (int) (tailSeconds * sampleRate);
            stopLen = length.load();
            silentRun = 0;
            state.store ((int) State::Finishing);
        }
        else
            finalise();
    }

    void clear()
    {
        stopPreview();
        const juce::SpinLock::ScopedLockType sl (lock);
        state.store ((int) State::Empty);
        length.store (0);
        buf.setSize (0, 0);
        capacity = 0;
        fileSR = 0; renderedFile = juce::File();
    }

    void startPreview()
    {
        if (getState() == State::Ready && length.load() > 0)
        {
            playPos.store (0);
            playing.store (true);
        }
    }
    void stopPreview() { playing.store (false); }

    State getState() const        { return (State) state.load(); }
    int   getLength() const       { return length.load(); }
    bool  isPlaying() const       { return playing.load(); }
    int   getPlayPos() const      { return playPos.load(); }
    double getRecordedSR() const  { return recordedSR; }
    float peek (int ch, int i) const { return (i >= 0 && i < length.load() && i < buf.getNumSamples()) ? buf.getSample (ch, i) : 0.f; }

    //============================== audio thread
    void capture (const juce::AudioBuffer<float>& block)
    {
        const int st = state.load();
        if (st != (int) State::Recording && st != (int) State::Finishing) return;
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked()) return;

        const int n = block.getNumSamples();
        const int len = length.load();
        const int room = capacity - len;
        const int ncopy = juce::jmin (n, room);
        if (ncopy > 0)
        {
            buf.copyFrom (0, len, block, 0, 0, ncopy);
            buf.copyFrom (1, len, block, juce::jmin (1, block.getNumChannels() - 1), 0, ncopy);
            length.store (len + ncopy);
        }

        if (st == (int) State::Finishing)
        {
            const float pk = block.getMagnitude (0, n);
            silentRun = pk < 0.0003f ? silentRun + n : 0;
            if (silentRun >= (int) (0.3 * sampleRate) || (length.load() - stopLen) >= tailMax || room <= n)
                finalise();
        }
        else if (room <= n)
            finalise();
    }

    void mixPreview (juce::AudioBuffer<float>& out)
    {
        if (! playing.load()) return;
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked()) return;
        if (state.load() != (int) State::Ready) { playing.store (false); return; }

        const int len = length.load();
        int p = playPos.load();
        const int n = out.getNumSamples();
        const int ncopy = juce::jmin (n, len - p);
        if (ncopy > 0)
        {
            out.addFrom (0, 0, buf, 0, p, ncopy);
            out.addFrom (1, 0, buf, 1, p, ncopy);
            p += ncopy;
        }
        playPos.store (p);
        if (p >= len) playing.store (false);
    }

    //============================== files
    static juce::File recordingsDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("STEINEX").getChildFile ("TRANSIT FX").getChildFile ("Recordings");
        d.createDirectory();
        return d;
    }

    static juce::File nextUniqueFile()
    {
        auto dir = recordingsDir();
        int maxIdx = 0;
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "STEINEX_TRANSITFX_*.wav"))
            maxIdx = juce::jmax (maxIdx, f.getFileNameWithoutExtension().fromLastOccurrenceOf ("_", false, false).getIntValue());
        return dir.getChildFile ("STEINEX_TRANSITFX_" + juce::String (maxIdx + 1).paddedLeft ('0', 3) + ".wav");
    }

    // Writes the recording as 24-bit stereo WAV. targetSR <= 0 means "same as host".
    bool renderToFile (const juce::File& file, int targetSR) const
    {
        const int len = length.load();
        if (getState() != State::Ready || len <= 0) return false;

        const int srcSR = (int) std::lround (recordedSR);
        const int dstSR = targetSR > 0 ? targetSR : srcSR;

        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());
        if (fos == nullptr || fos->failedToOpen()) return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (fos.get(), (double) dstSR, 2, 24, {}, 0));
        if (writer == nullptr) return false;
        fos.release();   // the writer owns the stream now

        bool ok;
        if (dstSR == srcSR)
        {
            ok = writer->writeFromAudioSampleBuffer (buf, 0, len);
        }
        else
        {
            const int outLen = (int) std::ceil ((double) len * (double) dstSR / (double) srcSR);
            juce::AudioBuffer<float> out (2, outLen);
            const double ratio = (double) srcSR / (double) dstSR;
            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.process (ratio, buf.getReadPointer (ch), out.getWritePointer (ch), outLen);
            }
            ok = writer->writeFromAudioSampleBuffer (out, 0, outLen);
        }
        writer->flush();
        return ok;
    }

    // Returns a ready-to-drag file for the current recording (re-renders if the sample rate changed).
    juce::File getOrCreateFile (int targetSR)
    {
        if (getState() != State::Ready) return {};
        const int want = targetSR > 0 ? targetSR : (int) std::lround (recordedSR);
        if (renderedFile.existsAsFile() && fileSR == want) return renderedFile;

        auto f = nextUniqueFile();
        if (! renderToFile (f, want)) return {};
        renderedFile = f;
        fileSR = want;
        return f;
    }

    juce::File getRenderedFile() const { return renderedFile; }

private:
    // audio thread (lock held) or message thread (lock held)
    void finalise()
    {
        int len = length.load();
        if (trimEnabled && len > 0)
        {
            const float thr = 0.00025f;
            int first = 0;
            while (first < len && std::abs (buf.getSample (0, first)) < thr && std::abs (buf.getSample (1, first)) < thr) ++first;
            int last = len - 1;
            while (last > first && std::abs (buf.getSample (0, last)) < thr && std::abs (buf.getSample (1, last)) < thr) --last;

            const int pre = (int) (0.005 * sampleRate), post = (int) (0.030 * sampleRate);
            first = juce::jmax (0, first - pre);
            last = juce::jmin (len - 1, last + post);
            const int newLen = last - first + 1;
            if (newLen > 0 && (first > 0 || newLen < len))
            {
                if (first > 0)
                    for (int ch = 0; ch < 2; ++ch)
                        std::memmove (buf.getWritePointer (ch), buf.getReadPointer (ch) + first, sizeof (float) * (size_t) newLen);
                len = newLen;
            }
            else if (newLen <= 0)
                len = 0;
        }

        // short fade-out to avoid a click at the very end
        const int fo = juce::jmin (len, (int) (0.01 * sampleRate));
        for (int i = 0; i < fo; ++i)
        {
            const float g = (float) (fo - 1 - i) / (float) fo;
            const int idx = len - fo + i;
            buf.setSample (0, idx, buf.getSample (0, idx) * g);
            buf.setSample (1, idx, buf.getSample (1, idx) * g);
        }

        length.store (len);
        state.store (len > 0 ? (int) State::Ready : (int) State::Empty);
    }

    juce::AudioBuffer<float> buf;
    juce::SpinLock lock;
    std::atomic<int> state { (int) State::Empty }, length { 0 }, playPos { 0 };
    std::atomic<bool> playing { false };
    double sampleRate = 44100.0, recordedSR = 44100.0;
    int capacity = 0, silentRun = 0, stopLen = 0, tailMax = 0, fileSR = 0;
    bool trimEnabled = true;
    juce::File renderedFile;
};
} // namespace steinex
