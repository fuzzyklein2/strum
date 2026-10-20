#include <csignal>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h> // for isatty() and fileno()
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio.h"
#include <rtmidi/RtMidi.h>

using namespace std;

inline const std::string WORLD = "\U0001F30E"; // 🌎 Earth Globe Americas
inline const std::string SONG_DIR = "/home/fuzzy/SOLO/2026/songs/";
inline const std::vector<std::string> SONG_LIST = {
    "Jack the Ripper"
};

atomic<bool> startRequested = false;
atomic<bool> breakRequested = false;
atomic<bool> running = true;

struct Clip
{
    vector<float> samples;
    ma_uint64 frames = 0;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
};

bool loadClip(string filename, Clip &clip)
{
    ma_decoder decoder;

    if (ma_decoder_init_file(filename.c_str(), nullptr, &decoder) != MA_SUCCESS)
        return false;

    clip.channels = decoder.outputChannels;
    clip.sampleRate = decoder.outputSampleRate;

    ma_uint64 totalFrames;

    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

    clip.frames = totalFrames;

    clip.samples.resize(
        totalFrames * clip.channels
    );

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(
        &decoder,
        clip.samples.data(),
        totalFrames,
        &framesRead
    );

    ma_decoder_uninit(&decoder);

    return framesRead == totalFrames;
}

/// @brief Audio player class
class Player
{
public:
    Clip intro;
    Clip breakClip;

    // std::atomic<bool> breakRequested{false};

    bool playing = false;

    size_t position = 0;
    bool playingBreak = false;

    // Need constructor to load the clips.
    Player();
    // Destructor?

    void fill(float* output, ma_uint32 frames);
    // void run(const Song& s);
};

Player::Player()
{
    if (!loadClip(SONG_DIR + SONG_LIST[0] + "/patterns/Intro.flac", intro))
    {
        throw runtime_error("Could not load Intro.flac");
    }

    cout << "Loaded Intro.flac; "
         << intro.frames
         << " frames\n";

    if (!loadClip(SONG_DIR + SONG_LIST[0] + "/patterns/Break.flac", breakClip))
    {
        throw runtime_error("Could not load Break.flac");
    }

    cout << "Loaded Break.flac; "
         << breakClip.frames
         << " frames\n";

    if (intro.channels != 2 || breakClip.channels != 2)
    {
        throw runtime_error("Expected stereo files");
    }
}

void Player::fill(float* output, ma_uint32 frames)
{
    if (!playing)
    {
        std::fill(output, output + frames * 2, 0.0f);
        return;
    }
    
    for (ma_uint32 i = 0; i < frames; i++)
    {
        Clip& clip = playingBreak ? breakClip : intro;

        if (position == 0)
        cout << "Starting "
         << (playingBreak ? "Break" : "Intro")
         << "\n";

        if (clip.samples.empty())
        {
            output[i * 2] = 0;
            output[i * 2 + 1] = 0;
            continue;
        }
        

        // Assuming stereo float samples:
        output[i * 2]     = clip.samples[position * 2];     // left
        output[i * 2 + 1] = clip.samples[position * 2 + 1]; // right

        position++;

        if (position >= clip.frames)
        {
            position = 0;

            if (!playingBreak && breakRequested)
            {
                breakRequested = false;
                playingBreak = true;
            }
            else if (playingBreak)
            {
                playingBreak = false;
            }
        }
    }
}



void midi_listener()
{   // Look for the keyboard MIDI port.
    RtMidiIn midi;
    unsigned int ports = midi.getPortCount();
    cout << "MIDI inputs: " << ports << "\n";
    for (unsigned int i = 0; i < ports; i++)
        {
            cout << i << ": " << midi.getPortName(i) << "\n";
        }

    // Open the Alesis port, not Midi Through
    midi.openPort(1);

    // Listen for MIDI messages...
    
    cout << "Listening..." << endl;

    vector<unsigned char> message;

    while (running)
    {
        midi.getMessage(&message);

        if (!message.empty())
        {
            cout << "Message:";
            for (auto byte : message)
                cout << " " << (int)byte;
            cout << "\n";

            if (message.size() == 3 &&
                message[0] == 0xB0 &&
                message[1] == 64)
            {
                if (message[2] > 63)
                {
                    cout << "PEDAL DOWN\n";
                    if (!startRequested)
                        startRequested = true;
                    else
                        breakRequested = true;                }
            }
        }

        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

void data_callback(
    ma_device* device,
    void* output,
    const void* input,
    ma_uint32 frameCount)
{
    auto* player =
        static_cast<Player*>(device->pUserData);

    player->fill(
        static_cast<float*>(output),
        frameCount
    );
}

void play_audio()
{
    try
    {
        Player player;
    
        // TODO: load intro and breakClip here
    
        ma_device_config config =
            ma_device_config_init(ma_device_type_playback);
    
        config.playback.format = ma_format_f32;
        config.playback.channels = 2;
        config.sampleRate = player.intro.sampleRate;
    
        config.dataCallback = data_callback;
        config.pUserData = &player;
    
        ma_device device;
    
        if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
        {
            cerr << "Couldn't open playback device.\n";
            return;
        }
    
        if (ma_device_start(&device) != MA_SUCCESS)
        {
            cerr << "Couldn't start playback.\n";
            ma_device_uninit(&device);
            return;
        }
    
        cout << "Press sustain pedal to start...\n";
        
        while (running && !startRequested)
        {
            this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        cout << "Starting playback.\n";
        player.playing = true;
        cout << "Playing forever... Ctrl-C to quit.\n";
    
        while (running)
        {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    
        ma_device_uninit(&device);
    }
    catch (const exception& e)
    {
        cerr << e.what() << "\n";
        running = false;
    }
}

void signal_handler(int)
{
    running = false;
}


int main(
    // Coroutine passed here
) {
    // Check if stdin is a terminal (not piped)
    if (isatty(fileno(stdin))) {
        // No piped input
        cout << "Hello, " << WORLD << '!' << endl;
    } else {
        // There is piped input
        ostringstream oss;
        oss << cin.rdbuf();
        string input = oss.str();
        // Remove trailing newline (optional, improves output for typical use)
        if (!input.empty() && input.back() == '\n') input.pop_back();

        cout << "Hello, " << input << '!' << endl;
    }

    signal(SIGINT, signal_handler);
    thread midiThread(midi_listener);
    thread audioThread(play_audio);
    midiThread.join();
    audioThread.join();

}
