# Native Bink movie decoder

The Windows x64 build uses FFmpeg libraries to decode the original Bink 1 movies inside the game process. Video is converted to BGRA, audio to interleaved floating point PCM. Packets and frames are decoded progressively. SDL handles presentation and audio output in TIG.

`ARCANUM_NATIVE_MOVIES` defaults to ON for Windows 64-bit configurations and OFF elsewhere. The existing 32-bit backend remains available with the option OFF.

The default Windows SDK is built from the pinned FFmpeg source revision by `tools/build_ffmpeg_windows.py`. Only Bink video, Bink RDFT/DCT audio, local file input and the required conversion libraries are enabled. CPU SIMD optimizations and runtime detection remain enabled. The libraries use LGPL 2.1 or later.

The first build needs Visual C++ x64 tools, Git for Windows and Python 3.8 or newer. The builder downloads the verified FFmpeg sources, GNU Make and NASM into the build directory; these tools are not installed globally or distributed with the game. Later configurations reuse the SDK after checking its recipe and runtime file hashes.

Set `ARCANUM_FFMPEG_ROOT` to use an alternative ABI-compatible SDK with `include`, `lib` and `bin` directories. The architecture must match the executable. The automatic builder targets x86-64, not ARM64.

Builds stage five shared libraries next to the executables: avformat, avcodec, avutil, swscale and swresample. Keep these files with the x64 executable when installing it. License, source/build links and `BUILD-CONFIG.txt` are staged in `licenses/ffmpeg`. No ffmpeg command-line program is required at runtime. The libraries remain replaceable shared components.

The selected sound track is matched to its Bink track ID. An absent selected track produces silent video. Movies larger than the display are reduced while preserving their aspect ratio; smaller movies retain their native dimensions and are centered.

`movie_decoder_test` always checks invalid arguments and malformed input. Set `ARCANUM_TEST_MOVIE` to the original 800x400 SierraLogo.bik to enable complete decoding and timing checks. `ARCANUM_TEST_MOVIE_ORACLES` optionally supplies first-frame.bgra and audio.f32 references from the same FFmpeg build. Do not distribute the game assets as test fixtures.

For a general installed-movie audit, run `movie_decoder_test --asset path.bik`. It verifies the announced frame count, increasing presentation timestamps, bounded resized dimensions, finite PCM samples and compatible audio/video durations.
