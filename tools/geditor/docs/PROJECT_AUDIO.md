# Project audio

Settings -> Project Settings has SFX and Music tabs. SFX lists the sound effects
from the open project's `base.z64`. Filter by name or by hexadecimal/decimal ID.
The list shows the ID, a readable name, native sample format and sample byte size.
Select an effect and click Play, double-click its row, or press Enter while the
list has focus. Stop ends the preview; playing another effect replaces it.
The main editor remains usable during playback. Leaving the SFX tab, changing
the filter or project, and closing the window also stop playback. Music is
reserved for future controls; this change does not add audio editing.

The catalog reads `SFXC` and `SFXT` from the ROM manifest and follows the native
`ALBankFile` metadata for bank 0, instrument 0, matching the game's SFX loader.
GUD's `ALInstrumentAlt` view starts its sound-pointer array four bytes before
`ALInstrument`, so the first actual sound has ID 1. ID 0 means no sound and has
no row. Each sound slot keeps its own ID even when sounds share sample bytes.
The count comes from the ROM; it is not limited to the known enum names.

Names are generated from `SFX_ID` in `src/bondconstants.h`, since the ROM bank
has no name strings. They describe the stock ID meanings; replacing samples
does not rename them. Additional IDs display as "Unnamed sound effect". Sample
bytes are the native stored size, not a decoded WAV size or duration.

The window refreshes when a project is opened, created or rebased, clears on
project close, and releases its metadata when closed. Missing audio manifest
entries or invalid bank offsets produce an explanation in the SFX tab instead
of retaining the previous project's sounds. Older ROMs without SFXC/SFXT need
a rebase onto a current GUD ROM to use this list. No project format changes or
game-engine changes are needed. The window retains the project ROM while open
for preview decoding, then releases it on project change or close.

## Preview rendering

The editor decodes native N64 ADPCM (order-two predictor books) and big-endian
16-bit PCM without external programs. It follows the effect's pitch, sample
volume, pan, envelope, linked layers and their delays, pitch slides, and sample
loop points. Quiet variants retain their relative volume. Samples are rendered
to stereo PCM and played asynchronously through Windows `waveOut` (`winmm`).
The output buffer remains alive until the driver finishes or Stop resets it.

Sustained effects preview for five seconds; other previews have a ten-second
limit. Retriggered sequences are auditioned once. A limited preview is marked
in the playback status. This is a bank preview, using linear resampling and an
approximated envelope; it does not reproduce the game's reverb, spatialization,
or gameplay-driven volume/pitch changes. Source sample decoding is exact; the
complete sound mix is not intended to be a bit-identical emulation of the RSP.

Build GEditor normally. `sfxnames.h` is regenerated when the SFX enum changes.
Run the catalog tests with:

```sh
python3 tools/geditor/tests/sfx_catalog/run.py
python3 tools/geditor/tests/sfx_catalog/run.py path/to/GUD.z64
python3 tools/geditor/tests/sfx_playback/run.py
python3 tools/geditor/tests/sfx_playback/run.py path/to/GUD.z64
```

The playback tests use the production decoder/renderer and a mocked Windows
audio driver to check buffer ownership, replacement, completion and error
cleanup. Windows audio-device playback still needs a listening check. The
decoder was additionally compared sample-for-sample across all 261 stock bank
entries against Mupen64Plus RSP-HLE's `adpcm_compute_residuals` at commit
`8a7a472a7172eb2c8725b305eae26818ed7b51a2`; that external reference is not a
build or runtime dependency.
