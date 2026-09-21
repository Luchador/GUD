# Create ROM defaults

Create ROM remembers the ROM name and output directory after a successful
export. The next dialog uses those values, including after restarting GEditor
or opening another project. Before the first successful export, it defaults to
the current project's name and directory. Cancelling, declining an overwrite,
or a failed save/export leaves the remembered values unchanged.

Output paths collapse redundant separators while preserving drive roots and
the leading pair of backslashes required by UNC network paths. New Project and
Rebase also avoid adding a second separator when the parent directory already
ends in one. Relative output directories are remembered as absolute paths.

Preferences are stored per Windows user under
`HKEY_CURRENT_USER\Software\GUD\GEditor\Create ROM`, in the `Name` and `Directory`
string values. If the remembered directory no longer exists, the dialog shows
its usual validation message so the user can choose a new destination.

Run the host regression checks with:

```sh
python3 tools/geditor/tests/rom_export_settings/run.py
python3 tools/geditor/tests/project_rebase/run.py
```
