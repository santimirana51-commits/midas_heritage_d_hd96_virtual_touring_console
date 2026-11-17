/*
  ==============================================================================
   Phase 89: Offline Mixdown & Multi-Track Stems Bounce Engine (Chapter 36)

   Renders a source audio file through the live mixer state faster than
   realtime and writes, in a single render pass:
     * one stereo Master L/R file, and
     * one mono file per selected bus stem (AUX / MATRIX).

   The engine-facing surface is OfflineRenderTarget, so this header stays free
   of mixer internals. MixerEngine drives the exact production callback path
   (renderOfflineBlocks) through the adapter defined in Main.cpp; that seam is
   what the unit tests exercise.

   Design notes:
     * The source file is streamed block by block (never fully loaded), so a
       multi-hour multitrack costs only a few hundred kB of working memory.
     * Optional sample-rate conversion runs per input channel through a 4-point
       Lagrange interpolator before the block reaches the engine.
     * Cancellation deletes every partially written file.
  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace nova::graviton
{
/** Engine interface used by the offline bouncer.

    One renderBlock() call consumes numSamples input frames and produces the
    Master L/R buffers plus every bus stem buffer for the same frames. The
    bounce writer then decides which of them are persisted, so master and stems
    always stay sample-accurate to each other.
*/
class OfflineRenderTarget
{
public:
    virtual ~OfflineRenderTarget() = default;

    /** Number of mono bus stems this engine can capture in a single pass. */
    virtual int getNumStems() const = 0;

    /** Human readable stem name, e.g. "AUX 1" or "MATRIX 2". */
    virtual juce::String getStemName (int stemIndex) const = 0;

    /** Render numSamples frames through the mixer.

        @param inputs        planar input channel pointers (numInputs entries)
        @param numInputs     number of valid input pointers
        @param numSamples    frames to render
        @param sampleRate    render sample rate in Hz
        @param masterOutL    receives numSamples frames of Master L
        @param masterOutR    receives numSamples frames of Master R
        @param stems         array of getNumStems() planar mono buffers
    */
    virtual void renderBlock (const float* const* inputs, int numInputs, int numSamples,
                              double sampleRate, float* masterOutL, float* masterOutR,
                              float* const* stems) = 0;
};

/** Complete description of one offline bounce job. */
struct OfflineBounceRequest
{
    juce::File sourceFile;        ///< multitrack or stereo source audio file
    juce::File outputDirectory;   ///< folder that receives the bounced files
    juce::String fileNamePrefix;  ///< defaults to the source file name
    bool exportMaster = true;     ///< write the stereo Master L/R file
    std::vector<int> stemSelection;  ///< stem indices to write (empty = none)
    juce::String container = "wav";  ///< "wav" or "flac"
    int bitDepth = 24;            ///< 16 or 24 bits per sample
    double sampleRate = 0.0;      ///< 0 = follow the source file sample rate
    int chunkFrames = 4096;       ///< render block size (64..16384)
};

/** Progress snapshot published while the bounce runs. */
struct OfflineBounceProgress
{
    juce::int64 framesDone = 0;
    juce::int64 totalFrames = 0;
    juce::String jobName;
    double elapsedSeconds = 0.0;
    double speedFactor = 0.0;  ///< rendered seconds per wall-clock second
};

/** Final outcome of a bounce job. */
struct OfflineBounceResult
{
    bool ok = false;
    bool cancelled = false;
    juce::String error;
    juce::Array<juce::File> files;
    juce::int64 framesRendered = 0;
    double elapsedSeconds = 0.0;
    double speedFactor = 0.0;  ///< faster-than-realtime factor
};

/** Streams a source file through an OfflineRenderTarget into WAV/FLAC files. */
class OfflineBouncer
{
public:
    static constexpr int kMaxBounceInputs = 64;   // matches engine input map
    static constexpr int kMinChunkFrames = 64;
    static constexpr int kMaxChunkFrames = 16384;

    using ProgressCallback = std::function<void (const OfflineBounceProgress&)>;

    /** Runs the whole bounce synchronously; call it from a worker thread.

        @param target      engine adapter that renders blocks
        @param request     job description (validated before any file is written)
        @param cancelFlag  polled once per block; a set flag aborts the job and
                           removes every file created so far
        @param progress    optional progress callback (same thread as run())
    */
    OfflineBounceResult run (OfflineRenderTarget& target,
                             const OfflineBounceRequest& request,
                             std::atomic<bool>* cancelFlag = nullptr,
                             const ProgressCallback& progress = nullptr)
    {
        OfflineBounceResult result;
        const double startedMs = juce::Time::getMillisecondCounterHiRes();

        const auto elapsedNow = [&startedMs] {
            return (juce::Time::getMillisecondCounterHiRes() - startedMs) / 1000.0;
        };

        // ---------------------------------------------------------- validate
        if (request.sourceFile.getFullPathName().isEmpty()
            || ! request.sourceFile.existsAsFile())
            return fail (result, "Source audio file not found", elapsedNow());

        if (request.outputDirectory.getFullPathName().isEmpty())
            return fail (result, "Output folder is not set", elapsedNow());

        if (request.chunkFrames < kMinChunkFrames || request.chunkFrames > kMaxChunkFrames)
            return fail (result, "Invalid render block size", elapsedNow());

        if (request.sampleRate > 0.0
            && (request.sampleRate < 8000.0 || request.sampleRate > 384000.0))
            return fail (result, "Unsupported output sample rate", elapsedNow());

        const bool useFlac = request.container.trim().equalsIgnoreCase ("flac");
        const int bitDepth = (request.bitDepth == 16) ? 16 : 24;

        // Sanitise the stem selection: range check plus de-duplication.
        std::vector<int> stems;
        stems.reserve (request.stemSelection.size());
        for (int index : request.stemSelection)
        {
            if (index < 0 || index >= target.getNumStems())
                continue;
            if (std::find (stems.begin(), stems.end(), index) == stems.end())
                stems.push_back (index);
        }
        std::sort (stems.begin(), stems.end());

        if (! request.exportMaster && stems.empty())
            return fail (result, "No bounce target selected (master or stems)", elapsedNow());

        // ------------------------------------------------------- open source
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (request.sourceFile));
        if (reader == nullptr)
            return fail (result, "Unsupported or corrupt source audio file", elapsedNow());

        if (reader->numChannels == 0 || reader->lengthInSamples <= 0)
            return fail (result, "Source audio file is empty", elapsedNow());

        const double sourceRate = reader->sampleRate;
        const double outRate = (request.sampleRate > 0.0) ? request.sampleRate : sourceRate;
        const double sourcePerOutput = sourceRate / outRate;
        const bool resample = std::abs (outRate - sourceRate) > 0.01;
        const int sourceChannels = static_cast<int> (reader->numChannels);
        const int inputChannels = juce::jlimit (1, kMaxBounceInputs, sourceChannels);
        const auto totalFrames = static_cast<juce::int64> (
            std::ceil (static_cast<double> (reader->lengthInSamples) * outRate / sourceRate));

        if (totalFrames <= 0)
            return fail (result, "Source audio file is empty", elapsedNow());

        if (! request.outputDirectory.createDirectory())
            return fail (result, "Cannot create output folder: "
                                 + request.outputDirectory.getFullPathName(), elapsedNow());

        const juce::String prefix = request.fileNamePrefix.isNotEmpty()
                                        ? request.fileNamePrefix
                                        : request.sourceFile.getFileNameWithoutExtension();
        const juce::String extension = useFlac ? ".flac" : ".wav";

        // ------------------------------------------------------ open outputs
        juce::WavAudioFormat wavFormat;
        juce::FlacAudioFormat flacFormat;
        juce::Array<juce::File> created;  // every output file of this job

        const auto openWriter = [&](const juce::String& suffix, int channels)
            -> std::unique_ptr<juce::AudioFormatWriter>
        {
            const juce::File file = availableFile (request.outputDirectory,
                                                   prefix + suffix, extension);
            std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
            if (stream == nullptr)
                return nullptr;

            // Both formats take ownership of the stream only when they build a
            // writer; on a null return the raw stream is still ours to delete.
            juce::OutputStream* rawStream = stream.release();
            juce::AudioFormatWriter* writer =
                useFlac ? flacFormat.createWriterFor (rawStream, outRate,
                                                      static_cast<unsigned int> (channels),
                                                      bitDepth, {}, 0)
                        : wavFormat.createWriterFor (rawStream, outRate,
                                                     static_cast<unsigned int> (channels),
                                                     bitDepth, {}, 0);
            if (writer == nullptr)
            {
                delete rawStream;
                return nullptr;
            }
            created.add (file);
            return std::unique_ptr<juce::AudioFormatWriter> (writer);
        };

        const auto cleanupOutputs = [&created]
        {
            for (const auto& file : created)
                file.deleteFile();
            created.clear();
        };

        std::unique_ptr<juce::AudioFormatWriter> masterWriter;
        if (request.exportMaster)
        {
            masterWriter = openWriter ("_Master_LR", 2);
            if (masterWriter == nullptr)
            {
                cleanupOutputs();
                return fail (result, "Cannot open Master output file for writing", elapsedNow());
            }
        }

        std::vector<std::unique_ptr<juce::AudioFormatWriter>> stemWriters;
        stemWriters.reserve (stems.size());
        for (int stemIndex : stems)
        {
            auto writer = openWriter ("_" + fileSafeName (target.getStemName (stemIndex)), 1);
            if (writer == nullptr)
            {
                cleanupOutputs();
                return fail (result, "Cannot open stem output file: "
                                     + target.getStemName (stemIndex), elapsedNow());
            }
            stemWriters.push_back (std::move (writer));
        }

        // ----------------------------------------------------- render buffers
        const int chunk = request.chunkFrames;
        const int numStems = target.getNumStems();

        juce::AudioBuffer<float> inputBlock (inputChannels, chunk);
        juce::AudioBuffer<float> masterBlock (2, chunk);
        std::vector<juce::AudioBuffer<float>> stemBlocks;
        stemBlocks.reserve (static_cast<size_t> (numStems));
        for (int s = 0; s < numStems; ++s)
            stemBlocks.emplace_back (1, chunk);

        std::vector<float*> inputWritePtrs (static_cast<size_t> (inputChannels), nullptr);
        std::vector<float*> sourceWritePtrs (static_cast<size_t> (inputChannels), nullptr);
        std::vector<const float*> inputReadPtrs (static_cast<size_t> (inputChannels), nullptr);
        std::vector<float*> stemWritePtrs (static_cast<size_t> (numStems), nullptr);
        for (int c = 0; c < inputChannels; ++c)
            inputWritePtrs[static_cast<size_t>(c)] = inputBlock.getWritePointer (c);
        for (int s = 0; s < numStems; ++s)
            stemWritePtrs[static_cast<size_t>(s)] = stemBlocks[static_cast<size_t>(s)].getWritePointer (0);

        juce::AudioBuffer<float> sourceBlock;
        std::vector<std::unique_ptr<juce::LagrangeInterpolator>> interpolators;
        if (resample)
        {
            const int sourceChunk = juce::jlimit (
                chunk + 8, 1 << 20,
                static_cast<int> (std::ceil (static_cast<double> (chunk) * sourcePerOutput)) + 32);
            sourceBlock.setSize (inputChannels, sourceChunk);
            sourceWritePtrs.clear();
            for (int c = 0; c < inputChannels; ++c)
            {
                sourceWritePtrs.push_back (sourceBlock.getWritePointer (c));
                interpolators.push_back (std::make_unique<juce::LagrangeInterpolator>());
            }
        }

        // ------------------------------------------------------ render loop
        juce::int64 sourcePos = 0;
        juce::int64 framesDone = 0;
        double lastProgressMs = 0.0;
        juce::String ioError;

        const auto reportProgress = [&](double nowMs, bool force)
        {
            if (progress == nullptr)
                return;
            if (! force && (nowMs - lastProgressMs) < 80.0)
                return;
            lastProgressMs = nowMs;
            OfflineBounceProgress update;
            update.framesDone = framesDone;
            update.totalFrames = totalFrames;
            update.jobName = prefix;
            update.elapsedSeconds = (nowMs - startedMs) / 1000.0;
            update.speedFactor = update.elapsedSeconds > 0.0
                                     ? (static_cast<double> (framesDone) / outRate)
                                           / update.elapsedSeconds
                                     : 0.0;
            progress (update);
        };

        while (framesDone < totalFrames)
        {
            if (cancelFlag != nullptr && cancelFlag->load (std::memory_order_relaxed))
                break;

            const int outFrames = static_cast<int> (
                std::min<juce::int64> (chunk, totalFrames - framesDone));

            // ------------------------------------------- feed the engine
            if (resample)
            {
                const int sourceFrames = juce::jlimit (
                    1, sourceBlock.getNumSamples(),
                    static_cast<int> (std::ceil (outFrames * sourcePerOutput)) + 16);

                if (! reader->read (sourceWritePtrs.data(), inputChannels,
                                    sourcePos, sourceFrames))
                {
                    ioError = "Read error on source audio file";
                    break;
                }

                int consumed = 0;
                for (int c = 0; c < inputChannels; ++c)
                {
                    const int used = interpolators[static_cast<size_t>(c)]->process (
                        sourcePerOutput, sourceBlock.getReadPointer (c),
                        inputBlock.getWritePointer (c), outFrames, sourceFrames, 0);
                    consumed = juce::jmax (consumed, used);
                }
                sourcePos += consumed;
            }
            else
            {
                if (! reader->read (inputWritePtrs.data(), inputChannels,
                                    sourcePos, outFrames))
                {
                    ioError = "Read error on source audio file";
                    break;
                }
                sourcePos += outFrames;
            }

            for (int c = 0; c < inputChannels; ++c)
                inputReadPtrs[static_cast<size_t>(c)] = inputBlock.getReadPointer (c);

            target.renderBlock (inputReadPtrs.data(), inputChannels, outFrames, outRate,
                                masterBlock.getWritePointer (0),
                                masterBlock.getWritePointer (1),
                                stemWritePtrs.data());

            // --------------------------------------------------- write files
            if (masterWriter != nullptr
                && ! masterWriter->writeFromAudioSampleBuffer (masterBlock, 0, outFrames))
            {
                ioError = "Write error on Master output file";
                break;
            }

            bool stemWriteFailed = false;
            for (size_t i = 0; i < stemWriters.size(); ++i)
            {
                const float* stemChannel[1] = {
                    stemBlocks[static_cast<size_t> (stems[i])].getReadPointer (0)
                };
                if (! stemWriters[i]->writeFromFloatArrays (stemChannel, 1, outFrames))
                {
                    ioError = "Write error on stem output file: "
                              + target.getStemName (stems[i]);
                    stemWriteFailed = true;
                    break;
                }
            }
            if (stemWriteFailed)
                break;

            framesDone += outFrames;
            reportProgress (juce::Time::getMillisecondCounterHiRes(), false);
        }

        // ------------------------------------------------------ finish job
        const bool cancelled = framesDone < totalFrames
            && cancelFlag != nullptr
            && cancelFlag->load (std::memory_order_relaxed);

        masterWriter.reset();
        for (auto& writer : stemWriters)
            writer.reset();

        reportProgress (juce::Time::getMillisecondCounterHiRes(), true);

        if (ioError.isNotEmpty())
        {
            cleanupOutputs();
            return fail (result, ioError, elapsedNow());
        }

        if (cancelled)
        {
            cleanupOutputs();
            auto failed = fail (result, "Bounce cancelled", elapsedNow());
            failed.cancelled = true;
            return failed;
        }

        for (const auto& file : created)
        {
            if (! file.existsAsFile() || file.getSize() <= 0)
            {
                cleanupOutputs();
                return fail (result, "Output file is empty: " + file.getFileName(), elapsedNow());
            }
        }

        result.ok = true;
        result.files = created;
        result.framesRendered = framesDone;
        result.elapsedSeconds = elapsedNow();
        result.speedFactor = result.elapsedSeconds > 0.0
                                 ? (static_cast<double> (framesDone) / outRate)
                                       / result.elapsedSeconds
                                 : 0.0;
        return result;
    }

private:
    static OfflineBounceResult fail (OfflineBounceResult& result,
                                     const juce::String& error,
                                     double elapsedSeconds)
    {
        result.ok = false;
        result.error = error;
        result.elapsedSeconds = elapsedSeconds;
        return result;
    }

    /** First non-existing "{base}{ext}" inside dir: bounce_1.wav, bounce_2.wav ... */
    static juce::File availableFile (const juce::File& dir,
                                     const juce::String& base,
                                     const juce::String& extension)
    {
        juce::File file = dir.getChildFile (base + extension);
        int suffix = 2;
        while (file.exists())
            file = dir.getChildFile (base + "_" + juce::String (suffix++) + extension);
        return file;
    }

    /** Makes a stem name safe for use inside a file name. */
    static juce::String fileSafeName (juce::String name)
    {
        return name.replaceCharacters (" /\\:*?\"<>|", "__________");
    }
};
}  // namespace nova::graviton
