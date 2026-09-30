# Editor themes

Settings > Editor Settings > Theme offers **Light** and **Dark**. Dark is the
initial default, including when an old preference store has no Theme value.
Changes apply immediately to open windows and are saved per Windows user under
`Software\GUD\GEditor\Editor Settings`, DWORD `Theme` (0 = Light, 1 = Dark).
This is an editor preference, not project or ROM data.

## Changing the colors

Edit **`tools/geditor/src/theme.c`**, the **`g_DarkPalette`** array at the top.
Each entry is `RGB(red, green, blue)`, with components from 0 to 255. Rebuild
GEditor after editing. The roles are declared in `theme.h`.

| Role | Default RGB | Used for |
| --- | --- | --- |
| `THEME_BACKGROUND` | 36, 36, 36 | Window and dialog backgrounds |
| `THEME_PANEL` | 45, 45, 45 | Headers and panel chrome |
| `THEME_INPUT` | 29, 29, 29 | Text fields and standard lists |
| `THEME_BUTTON` | 58, 58, 58 | Buttons |
| `THEME_HOVER` | 73, 73, 73 | Hovered controls |
| `THEME_PRESSED` | 43, 62, 85 | Pressed controls |
| `THEME_BORDER` | 82, 82, 82 | Borders and separators |
| `THEME_TEXT` | 222, 222, 222 | Normal text |
| `THEME_MUTED` | 153, 153, 153 | Help and disabled text |
| `THEME_SELECTION` | 58, 91, 138 | Selected menus and control accents |
| `THEME_SELECTION_TEXT` | 255, 255, 255 | Selected menu/browser text |
| `THEME_MENU` | 40, 40, 40 | Menu backgrounds |
| `THEME_TITLE` | 30, 30, 30 | Windows 11 title bars |
| `THEME_TITLE_TEXT` | 222, 222, 222 | Windows 11 title text |
| `THEME_ERROR_BACKGROUND` | 93, 42, 42 | Invalid color-picker input |

Light uses the Windows system colors. `ThemeSystemColor` and `ThemeSystemBrush`
map existing custom panel painting onto these roles in Dark mode.

## Integration

`ThemeInitialize` runs before the first editor window is created. A
`WH_CALLWNDPROCRET` hook confined to the editor's UI thread attaches subclasses
after window/control creation. This also covers later Model Editor, UV Editor,
Render Studio and dialog windows without a separate theme implementation in
each tool. `ThemeSet` refreshes existing windows immediately.

Native controls retain their input handling. The theme supplies painting for
ordinary buttons, checkboxes/radio buttons, combo fields, tabs, list headers
and sliders; uses control-color messages for labels, edits and list boxes;
and sets list/tree colors through their public APIs. Existing owner-drawn
icons, image thumbnails and material color swatches keep their own drawing.

Menus use documented owner drawing. Item IDs, states, original item data and
Unicode labels are retained; popup owner-draw records are restored and freed
when the popup closes. Menu mnemonics, duplicate mnemonic cycling, separators,
checks and shortcut text are handled by the shared module. Window frames use
DWM attributes 20, 35 and 36; unsupported attributes fail harmlessly on older
Windows versions. The build links `uxtheme` and `dwmapi`.

`src/geditor.manifest` requests Common Controls v6, required by
`GetWindowSubclass`. It is embedded as `RT_MANIFEST` resource 1 so Windows
selects the correct DLL before resolving imports. No external manifest or DLL
needs to be copied beside the executable. The resource build depends on the
manifest so an edit to it rebuilds the executable.

This is the initial theme framework. Native scrollbar chrome, some shell/common
dialog elements, and backgrounds baked into supplied toolbar PNGs can still use
their original appearance. This change does not recolor those images, the 3D
viewport, textures, vertex paint, or material colors.

## Verification

- `python3 tools/geditor/tests/editor_units/run.py`: preferences, defaults,
  malformed/missing values, immediate theme application and registry failure.
- `python3 tools/geditor/tests/theme/run.py`: real menu logic with mocked Win32
  menu storage, including state/data preservation, mnemonics, dynamic rebuilds
  and repeated cleanup under ASan/UBSan.
- Build the complete Win64 editor and its resources with the normal Makefile.
- `python3 tools/geditor/tests/theme/check_manifest.py tools/geditor/GEditor.exe`:
  check that the built executable embeds the v6 dependency at the startup
  manifest resource ID. This catches a loader requirement that a successful
  compile/link alone cannot verify.

The host tests do not render Windows controls. On Windows, check opening menus
with mouse and Alt shortcuts, checking/toggling controls, typing in fields,
opening combo lists, and switching themes while auxiliary windows are open.
