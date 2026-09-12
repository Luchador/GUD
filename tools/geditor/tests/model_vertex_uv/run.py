#!/usr/bin/env python3
"""Test native vertex/UV/color edits and saved glTF/GLB/ROM replacements.

Uses real repository models plus a fixture with vertices loaded under two
different matrices into the same RSP cache. No ROM or Windows SDK is needed.
PNG encoding and ROM directory/texture lookup are stubs, not tested here.
"""
import argparse
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
    struct.pack_into(">H", data, 0xd0, 4)
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


def load_state_fixture(path, inherited=False):
    mixed_fixture(path)
    data = bytearray(path.read_bytes()[:0x120])
    commands = [(0xc0000002, 1)]
    if not inherited:
        commands += [(0x01020040, 0x03000000)]
    commands += [(0xbb002801, 0x80004000), (0x04100020, 0x04000000),
                 (0x01020040, 0x03000040), (0xbb003001, 0x40002000),
                 (0x04120020, 0x05000100), (0xbf000000, 0x00000a14),
                 (0xbb003801, 0x20001000), (0xbf000000, 0x000a1e14),
                 (0xb8000000, 0)]
    for command in commands:
        data += struct.pack(">II", *command)
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


def color_fixture(path, kind):
    mixed_fixture(path)
    data = bytearray(path.read_bytes()[:0x120])
    for i in range(4):
        data[0xe0 + i * 16 + 15] = 40 + i * 30
    commands = [(0xc0000002, 1), (0x01020040, 0x03000000)]
    lighting = kind in ("normals", "reflection", "shared-normals")
    flags = 0x60000 if kind == "reflection" else 0x20000
    if lighting:
        commands += [(0xb7000000, flags)]
    commands += [(0x04300040, 0x04000000)]
    if lighting and kind != "shared-normals":
        commands += [(0xb6000000, flags)]  # load-time interpretation must survive
    environment = [(0xfb000000, 128), (0xfcffffff, 0xfffe793d)]
    if kind == "constant-alpha":
        commands += environment
    commands += [(0xbf000000, 0x00000a14)]
    if kind == "shared-alpha":
        commands += environment
    if kind == "shared-normals":
        commands += [(0xb6000000, flags), (0x04300040, 0x04000000)]
    commands += [(0xbf000000, 0x000a1e14), (0xb8000000, 0)]
    for command in commands:
        data += struct.pack(">II", *command)
    path.write_bytes(data)


def painted(document, encoding):
    doc = copy.deepcopy(document)
    binary = bytearray(base64.b64decode(doc["buffers"][0]["uri"].split(",")[1]))
    components = 3 if encoding == "rgb" else 4
    values = (17, 99, 201, 173)[:components]
    component_type = {"byte": 5121, "short": 5123}.get(encoding, 5126)
    fmt = "<" + {5121: "B", 5123: "H", 5126: "f"}[component_type] * components
    if component_type == 5123:
        values = tuple(v * 257 for v in values)
    if component_type == 5126:
        values = tuple((v + 0.2) / 255 for v in values)  # test rounding to bytes
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            accessor = doc["accessors"][primitive["attributes"]["COLOR_0"]]
            binary += b"\0" * (-len(binary) % 4)
            offset = len(binary)
            binary += struct.pack(fmt, *values) * accessor["count"]
            accessor.update(bufferView=len(doc["bufferViews"]), byteOffset=0,
                            componentType=component_type, type=f"VEC{components}")
            accessor.pop("normalized", None)
            if component_type != 5126:
                accessor["normalized"] = True
            doc["bufferViews"].append({"buffer": 0, "byteOffset": offset,
                                       "byteLength": len(binary) - offset})
    doc["buffers"][0]["byteLength"] = len(binary)
    doc["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(binary).decode()
    return doc, binary


def seamed(document):
    doc = copy.deepcopy(document)
    binary = bytearray(base64.b64decode(doc["buffers"][0]["uri"].split(",")[1]))
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            attributes = primitive["attributes"]
            def at(name, index):
                accessor = doc["accessors"][attributes[name]]
                view = doc["bufferViews"][accessor["bufferView"]]
                return view.get("byteOffset", 0) + accessor.get("byteOffset", 0) + index * view["byteStride"]
            for i in range(doc["accessors"][attributes["COLOR_0"]]["count"]):
                identity = int(struct.unpack_from("<f", binary, at("_GUD_VERTEX", i))[0])
                struct.pack_into("4B", binary, at("COLOR_0", i), identity & 255, identity >> 8, 77, 99 + identity % 156)
                u, v = struct.unpack_from("<ff", binary, at("TEXCOORD_0", i))
                struct.pack_into("<ff", binary, at("TEXCOORD_0", i), u + (identity % 7) * 0.25, v)
    doc["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(binary).decode()
    # Blender may reorder objects/primitives; _GUD_VERTEX determines identity.
    doc["nodes"].reverse()
    return doc, binary


def write_model(path, doc, binary):
    if path.suffix == ".gltf":
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blender-glb", type=Path, help="also test the original Pjungle3_treeZ Blender export")
    args = parser.parse_args()
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
        run("property-guards", fixture, "unused")
        scales = work / "mixed-scales.bin"
        load_state_fixture(scales)
        inherited = work / "inherited.bin"
        load_state_fixture(inherited, inherited=True)
        run("inherited", inherited, "unused")
        dynamic = work / "dynamic.bin"
        mixed_fixture(dynamic, dynamic=True)
        run("dynamic", dynamic, "unused")
        for kind in ("normals", "reflection", "constant-alpha", "shared-alpha", "shared-normals"):
            special = work / f"{kind}.bin"
            color_fixture(special, kind)
            run("special", special, kind)
        assets = [root / "assets/obseg/prop" / name for name in
                  ("Pjungle3_treeZ.bin", "Pjungle5_treeZ.bin", "Pbook1Z.bin")] + [fixture, scales]
        surface_fixtures = []
        for kind in ("one-cycle", "two-cycle", "cycle-override", "no-depth"):
            path = work / f"surface-{kind}.bin"
            mixed_fixture(path)
            data = bytearray(path.read_bytes()[:0x120])
            if kind == "two-cycle":
                data[0xd2] = 4
            commands = [(0xb6000000, 0x3000), (0xc0000002, 1),
                        (0x01020040, 0x03000000), (0x04300040, 0x04000000)]
            if kind == "cycle-override":
                commands += [(0xba001402, 0x00100000), (0xb900031d, 0xc8112078)]
            if kind == "no-depth":
                commands += [(0xb900031d, 0x00552048)]
            commands += [(0xbf000000, 0x00000a14), (0xbf000000, 0x000a1e14),
                         (0xbf000000, 0x00001e14), (0xb8000000, 0)]
            for command in commands:
                data += struct.pack(">II", *command)
            path.write_bytes(data)
            surface_fixtures.append(path)
        for index, asset in enumerate(assets[:2] + surface_fixtures):
            project = work / f"properties{index}"
            (project / "models/objects").mkdir(parents=True)
            run("properties", asset, project)
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
                write_model(path, doc, binary)
                run("edited", asset, path, project)
            doc, binary = seamed(document)
            for format_name in ("gltf", "glb"):
                path = work / f"seams{index}.{format_name}"
                project = work / f"seams{index}-{format_name}"
                (project / "models/objects").mkdir(parents=True)
                if format_name == "gltf":
                    external = copy.deepcopy(doc)
                    external["buffers"][0]["uri"] = path.with_suffix(".bin").name
                    path.with_suffix(".bin").write_bytes(binary)
                    path.write_text(json.dumps(external))
                else:
                    write_model(path, doc, binary)
                run("seams", asset, path, project)
            for encoding in ("byte", "short", "float", "rgb"):
                doc, binary = painted(document, encoding)
                path = work / f"paint{index}-{encoding}.glb"
                project = work / f"paint{index}-{encoding}"
                (project / "models/objects").mkdir(parents=True)
                write_model(path, doc, binary)
                run("paint-rgb" if encoding == "rgb" else "paint", asset, path, project)
            invalid = copy.deepcopy(document)
            for mesh in invalid["meshes"]:
                for primitive in mesh["primitives"]:
                    del primitive["attributes"]["TEXCOORD_0"]
            bad = work / "missing-uv.gltf"
            bad.write_text(json.dumps(invalid))
            run("reject", asset, bad, "no UVs")
            invalid = copy.deepcopy(document)
            for mesh in invalid["meshes"]:
                for primitive in mesh["primitives"]:
                    del primitive["attributes"]["COLOR_0"]
            bad.write_text(json.dumps(invalid))
            run("reject", asset, bad, "no vertex colors")
            for value in (float("nan"), float("inf"), -0.1, 1.1):
                invalid, binary = painted(document, "float")
                accessor = invalid["accessors"][invalid["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]]
                offset = invalid["bufferViews"][accessor["bufferView"]]["byteOffset"]
                struct.pack_into("<f", binary, offset, value)
                bad = work / "bad-color.glb"
                write_model(bad, invalid, binary)
                run("reject", asset, bad, "vertex color is invalid")
        if args.blender_glb:
            project = work / "blender-original"
            (project / "models/objects").mkdir(parents=True)
            run("blender", assets[0], args.blender_glb.resolve(), project)
        print("All model vertex/UV/color/seam regressions passed (ASan + UBSan).")


if __name__ == "__main__":
    main()
