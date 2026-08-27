#!/usr/bin/env python3
"""Convert all playable-character hand OBJ models and the shared swatch texture
into C++ headers
for the Quest VR runtime.

The engine (Pure3D/PDDI) has no OBJ loader, so we pre-bake the geometry into
static float arrays. Each triangle vertex is expanded so the runtime just
transforms and draws, no index bookkeeping. Normals and UVs are kept so the
runtime can use the same lit/phong material the characters use and sample the
swatch texture.

The PNG is decoded here (zlib only, no external deps) and embedded as raw RGBA
so the hand texture needs no asset packaging on the device.
"""
import os
import struct
import zlib

SRC_DIR = r"D:\games\The Simpsons Hit & Run\VR_models"
OUT_MESH = r"D:\Projects\The-Simpsons-Hit-and-Run-Android\code\vr\vr_hand_mesh.h"
OUT_TEX = r"D:\Projects\The-Simpsons-Hit-and-Run-Android\code\vr\vr_hand_texture.h"

CHARACTERS = ("homer", "bart", "lisa", "marge", "apu")
HANDS = {
    "%s_%s" % (character, side): "%s_hands_%s.obj" % (character, side)
    for character in CHARACTERS
    for side in ("l", "r")
}

TEX_NAME = "char_swatches_lit.bmp.png"


def parse_obj(path):
    verts = []
    norms = []
    uvs = []
    tris = []  # list of (pos_idx, uv_idx, norm_idx)
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if line.startswith("v "):
                verts.append([float(x) for x in line.split()[1:4]])
            elif line.startswith("vt "):
                uvs.append([float(x) for x in line.split()[1:3]])
            elif line.startswith("vn "):
                norms.append([float(x) for x in line.split()[1:4]])
            elif line.startswith("f "):
                toks = line.split()[1:]
                idx = []
                for t in toks:
                    parts = t.split("/")
                    vi = int(parts[0]) - 1
                    ti = -1
                    ni = -1
                    if len(parts) >= 2 and parts[1]:
                        ti = int(parts[1]) - 1
                    if len(parts) >= 3 and parts[2]:
                        ni = int(parts[2]) - 1
                    elif ti >= 0 and not uvs:
                        ni = ti
                    idx.append((vi, ti, ni))
                for i in range(1, len(idx) - 1):
                    tris.append(idx[0])
                    tris.append(idx[i])
                    tris.append(idx[i + 1])
    return verts, norms, uvs, tris


def emit_mesh(name, verts, norms, uvs, tris):
    pos = []
    nrm = []
    tex = []
    for (vi, ti, ni) in tris:
        p = verts[vi]
        pos.extend(p)
        if ni is not None and ni >= 0 and ni < len(norms):
            nrm.extend(norms[ni])
        else:
            nrm.extend([0.0, 1.0, 0.0])
        if ti is not None and ti >= 0 and ti < len(uvs):
            tex.extend(uvs[ti])
        else:
            tex.extend([0.0, 0.0])
    n = len(tris)
    out = []
    out.append("static const float vr_hand_%s_positions[%d] = {" % (name, n * 3))
    for i in range(0, len(pos), 3):
        if (i // 3) % 6 == 0:
            out.append("")
        out.append("    %.6f, %.6f, %.6f," % (pos[i], pos[i + 1], pos[i + 2]))
    out.append("};")
    out.append("static const float vr_hand_%s_normals[%d] = {" % (name, n * 3))
    for i in range(0, len(nrm), 3):
        if (i // 3) % 6 == 0:
            out.append("")
        out.append("    %.6f, %.6f, %.6f," % (nrm[i], nrm[i + 1], nrm[i + 2]))
    out.append("};")
    out.append("static const float vr_hand_%s_uvs[%d] = {" % (name, n * 2))
    for i in range(0, len(tex), 2):
        if (i // 2) % 6 == 0:
            out.append("")
        out.append("    %.6f, %.6f," % (tex[i], tex[i + 1]))
    out.append("};")
    # BeginPrims expects a vertex count.  `tris` is already the expanded list
    # of triangle vertices; multiplying it by three makes the renderer walk
    # beyond every generated array and interpret normals/UVs/adjacent data as
    # more geometry.
    out.append("static const int vr_hand_%s_count = %d;" % (name, n))
    return "\n".join(out)


def decode_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos = 8
    width = height = bitdepth = colortype = 0
    palette = None
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bitdepth, colortype, _, _, _ = struct.unpack(">IIBBBBB", chunk)
        elif ctype == b"PLTE":
            palette = chunk
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
        pos += 12 + length
    raw = zlib.decompress(idat)

    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colortype]
    bpp = channels * (bitdepth // 8)
    stride = width * bpp
    out = bytearray(width * height * 4)

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        if pb <= pc:
            return b
        return c

    prev = bytearray(stride)
    cur = bytearray(stride)
    rp = 0
    for y in range(height):
        ftype = raw[rp]
        rp += 1
        line = bytearray(raw[rp:rp + stride])
        rp += stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xff
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xff
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xff
            elif ftype == 4:
                line[i] = (line[i] + paeth(a, b, c)) & 0xff
        prev = line
        for x in range(width):
            o = y * width * 4 + x * 4
            s = x * bpp
            if colortype == 0:
                v = line[s]
                out[o:o + 4] = bytes((v, v, v, 255))
            elif colortype == 2:
                out[o:o + 4] = bytes((line[s], line[s + 1], line[s + 2], 255))
            elif colortype == 3:
                idx = line[s]
                po = idx * 3
                out[o:o + 4] = bytes((palette[po], palette[po + 1], palette[po + 2], 255))
            elif colortype == 4:
                out[o:o + 4] = bytes((line[s], line[s], line[s], line[s + 1]))
            elif colortype == 6:
                out[o:o + 4] = bytes((line[s], line[s + 1], line[s + 2], line[s + 3]))
    return width, height, bytes(out)


def emit_texture(width, height, rgba):
    out = []
    out.append("// Auto-generated by tools/generate_vr_hands.py - do not edit.")
    out.append("#ifndef SHAR_VR_HAND_TEXTURE_H")
    out.append("#define SHAR_VR_HAND_TEXTURE_H")
    out.append("")
    out.append("static const int vr_hand_tex_width = %d;" % width)
    out.append("static const int vr_hand_tex_height = %d;" % height)
    out.append("static const unsigned char vr_hand_tex_rgba[%d] = {" % (width * height * 4))
    for i in range(0, len(rgba), 4):
        if (i // 4) % 8 == 0:
            out.append("")
        out.append("    %d, %d, %d, %d," % (rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]))
    out.append("};")
    out.append("#endif")
    return "\n".join(out)


def main():
    mesh_out = []
    mesh_out.append("// Auto-generated by tools/generate_vr_hands.py - do not edit.")
    mesh_out.append("#ifndef SHAR_VR_HAND_MESH_H")
    mesh_out.append("#define SHAR_VR_HAND_MESH_H")
    mesh_out.append("")
    for key, fn in HANDS.items():
        verts, norms, uvs, tris = parse_obj(os.path.join(SRC_DIR, fn))
        mesh_out.append("// %s : %d triangles" % (fn, len(tris) // 3))
        mesh_out.append(emit_mesh(key, verts, norms, uvs, tris))
        mesh_out.append("")
    mesh_out.append("#endif")
    with open(OUT_MESH, "w", encoding="utf-8") as f:
        f.write("\n".join(mesh_out) + "\n")

    tex_path = os.path.join(SRC_DIR, TEX_NAME)
    if os.path.exists(tex_path):
        w, h, rgba = decode_png(tex_path)
        with open(OUT_TEX, "w", encoding="utf-8") as f:
            f.write(emit_texture(w, h, rgba) + "\n")
        print("Wrote", OUT_MESH, "and", OUT_TEX, "(texture %dx%d)" % (w, h))
    else:
        print("Wrote", OUT_MESH, "(texture %s NOT found, skipping)" % TEX_NAME)


if __name__ == "__main__":
    main()
