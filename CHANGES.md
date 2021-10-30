# Change Log

### Version 0.2.3

- Game Gear panning is now controlled with `N01` to the left, `N02` to center, `N03` to the right.
  - This has the side effect of breaking stereo functionality with modules made in previous SnevenTracker versions.
- Added a new noise reset enable effect, `NE2`, allowing the noise to be reset every frame.

- When a C-# (Copy) note is placed in the noise channel, inputting a note on Channel 3 will allow you to hear what the noise channel will sound like.
- VGM export has been updated and reworked to function similarly to WAV export. The old method of exporting VGMs are still there, however.
- Special thanks to Shiru for the changes made for this update.

### Version 0.2.2

- Overloaded `NE0` / `NE1` for noise reset enable effect
- Added SN76489 stereo separation to mixer menu
- Re-added file association
- SN76489 VGM logger now eliminates extra register writes that have no side effects
- Extended VGM header size so that it will not be misinterpreted by certain players
- Fixed text export and import (for SN7T only)
- Renamed `NCx` to "Channel swap"
- Samples are now properly downmixed to mono for visualizers

### Version 0.2.1

- Overloaded `N00` - `N1F` for Game Gear stereo control
- Channels no longer reduce to zero volume when the mixed volume is less than 0 (to match 0CC-FT's 5B behaviour)
- Fixed arpeggio on noise channel now maps 0 to `L-#` and 2 to `H-#` **(backward-incompatible change, modify your modules accordingly)**
- Fixed VGM logs putting 0 in the sample count fields

### Version 0.2.0

- Added VGM logger (with `vgm_cmp` postprocessing and proper GD3 tag support); use `vgmlpfnd` manually for looped songs
- Added `NCx` noise pitch rebind effect
- Channels now use subtractive mixing instead of multiplicative mixing for channel volume and instrument volume

### Version 0.1.0

- Initial release
