#!/usr/bin/env python3
"""Test native vertex/UV edits and glTF/GLB save/reopen/ROM replacement reads.

Uses real repository models plus a fixture with vertices loaded under two
different matrices into the same RSP cache. No ROM or Windows SDK is needed.
PNG encoding and ROM directory/texture lookup are stubs, not tested here.
"""
import base64
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def mixed_fixture(path, dynamic=False):
    data = bytearray(0x1d8 if dynamic else 0x160)
    display_list = 0x180 if dynamic else 0x120

    def word(at, value):
        struct.pack_into(">I", data, at, value)

    def node(at, opcode, rodata, parent=0, child=0):
        struct.pack_into(">H", data, at, opcode)
        for delta, value in ((4, rodata), (8, parent), (20, child)):
            word(at + delta, 0x05000000 | value if value else 0)

    node(0x20, 3, 0x80, child=0x38)
    node(0x38, 3, 0xa0, parent=0x20, child=0x50)
    node(0x50, 4, 0xc0, parent=0x38)
    struct.pack_into(">fff", data, 0xa0, 100, 200, 300)
    struct.pack_into(">h", data, 0xae, 1)
    word(0xc0, 0x05000000 | display_list)
    word(0xcc, 0x050000e0)
    data[0xd2] = 1
    for i in range(4):
        struct.pack_into(">hhhHhh4B", data, 0xe0 + i * 16,
                         10 + 10 * i, 10 * (i % 2), 0, 0x1234,
                         i * 32, i * -32, 127, 128, 129, 255)
    commands = [(0xc0000002, 1), (0x01020040, 0x03000000),
                (0x04100020, 0x04000000), (0x01020040, 0x03000040),
                (0x04120020, 0x05000100), (0xbf000000, 0x00000a14),
                (0xbf000000, 0x000a1e14), (0xb8000000, 0)]
    for i, command in enumerate(commands):
        struct.pack_into(">II", data, display_list + i * 8, *command)
    if dynamic:
        node(0x68, 0x16, 0x140, parent=0x38)
        word(0x5c, 0x05000068)  # next sibling after ordinary mesh
        word(0x144, 0x050000e0)
        word(0x148, 0x050001c0)
        struct.pack_into(">IIIIII", data, 0x1c0, 0x04200030, 0x04000000,
                         0xbf000000, 0x00000a14, 0xb8000000, 0)
    path.write_bytes(data)


def edited(document):
    doc = copy.deepcopy(document)
    binary = bytearray(base64.b64decode(doc["buffers"][0]["uri"].split(",")[1]))
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            for name, fmt in (("POSITION", "<fff"), ("TEXCOORD_0", "<ff")):
                accessor = doc["accessors"][primitive["attributes"][name]]
                view = doc["bufferViews"][accessor["bufferView"]]
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                stride = view.get("byteStride", struct.calcsize(fmt))
                for index in range(accessor["count"]):
                    at = offset + index * stride
                    value = struct.unpack_from(fmt, binary, at)
                    value = (value[0] * 0.75, value[1], value[2]) if name == "POSITION" else (value[0] + 2, value[1] - 1)
                    struct.pack_into(fmt, binary, at, *value)
    # Transform translation is deliberately left on the glTF nodes, as DCC
    # exporters do when object transforms have not been applied to the mesh.
    for node in doc["nodes"]:
        if "mesh" in node:
            node["translation"] = [0, 2.25, -3.75]
    doc["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(binary).decode()
    return doc, binary


def main():
    tests = Path(__file__).resolve().parent
    src = tests.parent.parent / "src"
    root = tests.parents[3]
    shim = tests.parent / "image_import"
    with tempfile.TemporaryDirectory(prefix="geditor-model-test-") as temp:
        work = Path(temp)
        executable = work / "check"
        command = [os.environ.get("CC", "cc"), "-O1", "-g", "-std=c99",
                   "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                   "-ffunction-sections", "-fdata-sections", "-fsanitize=address,undefined",
                   "-Dfopen=TestFopen", f"-I{shim}", f"-I{src}", f"-I{root}",
                   str(tests / "check.c"), str(shim / "platform.c")]
        command += [str(src / name) for name in ("modelload.c", "modelcompile.c", "gltf.c", "modeledits.c", "bgmaterial.c", "bgrender.c")]
        command += ["-lm", "-Wl,--gc-sections", "-o", str(executable)]
        subprocess.run(command, check=True)
        # LeakSanitizer's /proc thread scan is unavailable in some containers;
        # address/undefined-behavior checks still cover every production path.
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")

        def run(*args):
            subprocess.run([str(executable), *map(str, args)], check=True, env=env)

        fixture = work / "mixed.bin"
        mixed_fixture(fixture)
        dynamic = work / "dynamic.bin"
        mixed_fixture(dynamic, dynamic=True)
        run("dynamic", dynamic, "unused")
        assets = [root / "assets/obseg/prop" / name for name in
                  ("Pjungle3_treeZ.bin", "Pjungle5_treeZ.bin", "Pbook1Z.bin")] + [fixture]
        for index, asset in enumerate(assets):
            exported = work / f"export{index}.gltf"
            run("export", asset, exported)
            document = json.loads(exported.read_text())
            project = work / f"noop{index}"
            project.mkdir()
            run("noop", asset, exported, project)
            doc, binary = edited(document)
            for format_name in ("gltf", "glb"):
                path = work / f"edited{index}.{format_name}"
                project = work / f"project{index}-{format_name}"
                (project / "models/objects").mkdir(parents=True)
                if format_name == "gltf":
                    path.write_text(json.dumps(doc))
                else:
                    glb = copy.deepcopy(doc)
                    del glb["buffers"][0]["uri"]
                    json_chunk = json.dumps(glb).encode()
                    json_chunk += b" " * (-len(json_chunk) % 4)
                    bin_chunk = binary + b"\0" * (-len(binary) % 4)
                    path.write_bytes(struct.pack("<III", 0x46546c67, 2, 28 + len(json_chunk) + len(bin_chunk))
                                     + struct.pack("<II", len(json_chunk), 0x4e4f534a) + json_chunk
                                     + struct.pack("<II", len(bin_chunk), 0x004e4942) + bin_chunk)
                run("edited", asset, path, project)
            invalid = copy.deepcopy(document)
            for mesh in invalid["meshes"]:
                for primitive in mesh["primitives"]:
                    del primitive["attributes"]["TEXCOORD_0"]
            bad = work / "missing-uv.gltf"
            bad.write_text(json.dumps(invalid))
            run("reject", asset, bad, "no UVs")
        print("All model vertex/UV regressions passed (ASan + UBSan).")


if __name__ == "__main__":
    main()
