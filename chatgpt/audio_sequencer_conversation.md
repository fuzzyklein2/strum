# Audio Sequencer and Playback Architecture — Conversation

> This Markdown transcript collects the audio-architecture discussion available in the current conversation context. Earlier portions are reconstructed from the conversation summary; the latest exchanges are reproduced verbatim.

## Background and design context

The project is a mixed-language music/audio application. The goal is to keep musical sequencing logic separate from the real-time audio playback path. The system uses audio clips and patterns; patterns can repeat indefinitely, and a sustain pedal can request a transition at an appropriate musical boundary rather than cutting audio off abruptly.

The design distinguishes:
- **Pattern stack:** what musical structure should happen next.
- **Clip queue:** which audio clips are scheduled to play next.
- **Audio data:** the actual sample values that are played.

A pattern may refer to clips and nested patterns, allowing song structures to be composed from reusable parts. Infinite patterns require preparing for both continued repetition and a possible transition out.

## Earlier discussion (reconstructed from available context)

### User

Audio timing is the challenge. The producer/sequencer must know what sample/time indices the audio thread needs. The queue producer has to produce float samples faster than the audio executable can consume and play them. A separate producer thread is likely.

### User

The audio callback should pop an `Array<f32>` with `n` floats, where `n` is twice the requested number of frames for stereo.

### Discussion

Buffers should match the audio callback's requested size. With miniaudio, `periodSizeInFrames` can request a callback period, while `noFixedSizedCallback = false` leaves fixed-size callbacks enabled; the backend may not honor the requested size exactly, so the actual callback frame count should be checked.

For example, 1,024 stereo frames means 2,048 interleaved `f32` values. At 48 kHz, 1,024 frames represent about 21.33 ms.

### User

For an indefinitely repeating pattern, we may not know whether to move on until about the last second before audio runs out. One idea is to prepare two possible future audio queues: one for repeating the current pattern and another for the transition. They would be ready in advance, and the producer would select based on a command.

### Discussion

Prepare the two alternatives ahead of time and switch at a musical boundary. Avoid overwriting samples that the audio callback may currently be reading.

### User

Should the audio app expose a global `unique_ptr` to Rust/Python, or should other apps pipe floats to the audio app's stdin?

### Discussion

The audio app should own its player internally; a C++ `unique_ptr` should not be exposed directly across process boundaries. A control protocol between processes is usually a better boundary. Piping raw floats can work, but it is not automatically the best default.

### User

The audio app should load all the clips so it has easy access to their data. The sequencer never needs to touch it. It just needs to know their names (key to the map). The callback receives input somehow telling it which key to use next. If that clip's data runs out in the middle of the call, there had better be a name ready in the audio clip's input so it can immediately transition to the next clip.

### Assistant

- The audio app loads all clips into a map keyed by clip name and owns the sample data.
- The sequencer handles pattern logic and clip names only.
- The producer keeps upcoming clip names ready.
- The callback consumes the current clip and switches immediately to the next clip when necessary.
- The callback must not wait for the next clip name. The producer should maintain a thread-safe queue of upcoming names populated ahead of time; an empty queue is a sequencing pipeline underrun even if all clip samples are already loaded in memory.

A real callback must calculate how many frames remain in the current clip and copy only that many before switching to the next clip.

## Latest exchange (verbatim)

### User

OR the sequencer could load the audio clips and keep a queue<array<f32>> fully populated instead. And produce 2 queues of such type objects in the case of infinite repetition. Then the sequencer does all of the thinking and audio just eats numbers.

### Assistant

Honestly, I like that architecture better for your goal. The **sequencer does all the musical thinking; the audio engine just consumes samples.**

- **Sequencer:** Loads clips, interprets patterns, handles pedal events, and prepares queues of `Array<f32>` buffers.
- **Audio engine:** Pops a buffer and copies its numbers to the audio output. No pattern logic, clip lookup, or musical decisions.
- **Infinite repetition:** Maintain two ready-to-go queues: one for continuing the current pattern and one for transitioning out. The sequencer selects which queue becomes active at the appropriate musical boundary.

One important detail: each buffer must be sized for the audio callback's requested frames, and the callback may need to consume parts of multiple buffers during a single call.

The queues need a **nonblocking, thread-safe handoff**. The sequencer must stay ahead of playback; if the active queue runs dry, the audio engine can't wait for it to catch up.

### User

The sequencer must splice clip edges into a single buffer if it's necessary, and I can 99% guarantee it will be necessary.

### Assistant

Exactly. The sequencer fills each output buffer to the required size, even when that means joining the tail of one clip to the beginning of the next.

For stereo audio, if a callback needs \(N\) frames, the sequencer supplies exactly \(2N\) floats:

```text
Clip A:  ... [last 300 frames]
Clip B:  [first 724 frames] ...
              ↓ splice
Buffer: [300 frames from A | 724 frames from B]
```

That example fills a 1,024-frame buffer with 2,048 floats.

**The key design decision:** clip boundaries belong to the sequencer, not the audio engine. The audio engine doesn't even need to know a boundary occurred; it just plays the next complete buffer.

And your two-queue idea handles infinite repetition neatly: prepare buffers for continuing the pattern and buffers for transitioning out, then select the appropriate stream at the musical boundary.

### User

Give me this entire conversation as a Markdown file.
