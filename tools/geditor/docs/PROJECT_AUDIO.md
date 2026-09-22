# Project audio

Settings -> Project Settings has SFX and Music tabs. SFX lists the sound effects
from the open project's `base.z64`. Filter by name or by hexadecimal/decimal ID.
The list shows the ID, a readable name, native sample format and sample byte size.
Music is reserved for future controls; this change adds no playback or editing.

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
game-engine changes are needed.

Build GEditor normally. `sfxnames.h` is regenerated when the SFX enum changes.
Run the catalog tests with:

```sh
python3 tools/geditor/tests/sfx_catalog/run.py
python3 tools/geditor/tests/sfx_catalog/run.py path/to/GUD.z64
```
