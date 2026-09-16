# Detail texture properties

Select background faces and use the **Detail texture** section in Properties.

- **Off** removes the extra detail tile and uses ordinary mipmapping.
- **On - reuse base image** enables a detail tile that samples the base image.
- **On - separate image** uses a second image. Enter its hexadecimal image ID
  in **Detail image** and press Enter; entering an ID also enables this mode.
  The thumbnail and Find button identify the image in the content browser.
- **Detail U/V scale** chooses the native power-of-two sampling scales.
- **Minimum LOD** accepts an integer from 0 to 255, in units of 1/256.
  Press Enter to apply. This is an LOD clamp, not an opacity percentage.
- **Tile offset** selects no offset or the native half-texel offset. The latter
  affects generated mipmaps; explicit-LOD images ignore it. This native setting
  is shared by the base mip chain and detail tile.

Changes affect every selected face, across rooms and background layers. Mixed
values remain independent: changing U scale does not unify image IDs, V scales,
or other properties. Number fields apply on Enter; Escape restores the displayed
value. Each change supports undo/redo and is saved in the native background.

Enabling detail on an ordinary face starts at 2x U/V sampling and minimum LOD 0.
Use a textured, two-cycle background material with explicit texture state.
Specialized combiners that cannot be converted safely are left unchanged, with
an error explaining the unsupported selection. Existing detail materials can
still have their sampling properties edited without replacing their combiner.

New image assignments require available texture metadata. Separate image pairs
must fit the base mip chain plus the detail's full-size image in 4 KB of texture
memory. Newly created separate pairs use non-paletted images of 16 bits or less;
use the base-image mode for paletted or 32-bit images. Existing native pairs are
preserved when editing their sampling values. Changing an image's format or size
later can change its memory requirements.

The viewport continues to preview the base image. Check the actual detail blend
in game. Enabled detail remains a two-cycle effect; switching it off allows the
existing AA-off one-cycle optimization when the remaining material is eligible.

## Native data and validation

The editor writes the C0 texture marker, updates supported shaded combiners,
and splits/restores native draw-state groups around the selection. It preserves
room/layer membership, geometry, UVs, color, wrapping, render-mode/fog state and
unselected faces. The primitive color changed by the native detail loader is
restored after newly inserted detail spans.

Run `python3 tools/geditor/tests/face_detail/run.py` for marker/input decoding,
image-pair validation, multi-room/layer edits, native save/reload, history,
state restoration, no-op behavior, and allocation-failure checks. Native detail
disabling is exercised on Depot, Train, Surface and Bunker assets.
