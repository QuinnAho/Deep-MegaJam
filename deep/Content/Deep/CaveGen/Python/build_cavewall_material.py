# Builds M_CaveWall, the rock on the generated cave chunks (D-070, lane note "cave-rock", R1). Run headless:
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering
# or in an editor: exec(open(<this file>).read()).
#
# The recipe is a copy of the jam's M_CaveRock (deep/Content/Deep/Cave/build_cave_materials.py, which stays untouched):
#   1. Object-space triplanar sampling of Dragon Cave's T_CaveRocks01 and 02 (read-only, never saved into), weights
#      pow(abs(vertex normal), 8) (M_CaveRock: 4; sharper here, because 4 smeared stretched texture across
#      the chunks' many 45-degree faces in the first frames). Sources: Epic's WorldAlignedTexture / WorldAlignedNormal material functions (Material
#      Functions reference, "Texturing"), Ben Golus, "Normal Mapping for a Triplanar Shader". Object space, because the
#      cave moves past the sub; world space would swim.
#   2. Texture set 01 and 02 blended by a slow mask (M_CaveRock also blends two tile sizes of each; here each set has
#      one tile size, 3 m and 5 m, and reads only inside clean windows of the atlas: see "The texture source" below).
#   3. UDN normal blend in object space, turned to world for the output (tangent_space_normal off).
#   4. Silt on faces that point up (vertex normal z plus a wobble), albedo 0.095..0.165, roughness 0.9. Here the
#      silt keeps a little of the rock's grain (albedo x 0.8..1.2 by the rock's value, normal flattened 55%, not
#      80%), because flat silt read as a pale, textureless sheet under the lamp in the first frames.
#   5. Style-guide grade: saturation at most 0.35, value at most 0.25 linear; roughness from ORM green.
#      COST, for Quinn (Nov 25): DefaultEngine.ini traces Lumen reflections up to roughness 0.6 (for wet steel). With the
#      whole wall at the guide's wet 0.35..0.55, Lumen reflections cost +2.0 to +2.4 ms at the cave views against the
#      grey (budget +0.5). So the defaults are damp rock at 0.60..0.70 (not traced) with the wet shine on the flowstone
#      patches (0.30, traced, about +0.5 ms); the lamp's own highlight is direct light and needs no trace. RoughMin 0.35
#      and RoughMax 0.55 give the guide's numbers back, if the trace limit drops or the cost is accepted.
# Added here (research note "Cave Rock Material - Approaches", section 7):
#   6. Cavity from vertex colour R: cavegen.bake_vertex_data() bakes Geometry Script ambient occlusion (BakeVertex,
#      16 rays, 3 m) into R. Cracks darken the albedo and lose specular, so they read under a moving lamp at range,
#      where the normal map is gone. Vertex colour G, B and A are not read (B stays free for a later guideline).
#   7. Macro value shift: plus or minus 15% over about 8 to 15 m (object space), so a 30 m lamp cone shows no repeat.
#   8. Flowstone: a smoother (roughness 0.28), slightly lighter calcite coat on walls, in patches stretched downward
#      like drapes, the normal half flattened. Parameter FlowAmount; 0 turns it off.
# Usage flags: Nanite on (the chunks run with Nanite off, but a cooked game needs the flag if that ever changes, lesson
# of Nov 15), instanced static meshes as in M_CaveRock.
import unreal

PATH = "/Game/Deep/CaveGen/Materials"
NAME = "M_CaveWall"
TEX = "/Game/DragonCave/Textures/"
mel = unreal.MaterialEditingLibrary
errors_seen = []


# ---- HLSL (straight-line code: a Custom node body cannot define functions) -------------------------------------------
# Cost: M_CaveRock's recipe reads every texture 12 times (3 planes x 2 tile sizes x 2 sets), 36 reads a pixel, and
# measured +1.5 to +2.9 ms against the grey in the first A/B. Here a plane is read only where its weight counts (after
# the pow-8 blend, under 3% is dropped and the rest renormalised) and a set only where the set mask uses it, each read
# behind a [branch]. Reads in a branch take their gradients from ddx/ddy of the position, taken outside the branches
# (Texture2DSampleGrad), so mips stay right. Most pixels then read 1 plane of 1 set: 2 reads a texture, not 12.

# The texture source (found Nov 25): Dragon Cave's T_CaveRocks01 and 02 are UV atlases baked for the pack's own rock
# meshes, not tiling textures. About a third of each is island padding (stretched, starburst streaks), and M_CaveRock
# tiles the whole texture, so those streaks show on the walls. Here every read stays inside a square window that lies
# wholly within one island (checked on the exported textures), and the window's own tile seams are hidden by blending
# three reads offset by a third of a tile, each weighted by a tent that is 0 at its tile's edge. The blend keeps the
# texture's contrast: mean + (blend - mean) * sum(w) / sqrt(sum(w^2)), the mean read from the last mip
# (variance-preserving blending, Heitz and Neyret, "High-Performance By-Example Noise using a Histogram-Preserving
# Blending Operator", 2018, section 1.3). Set 01 tiles at TileSmallM, set 02 at TileLargeM; the x, y and z planes use
# different windows, so a wall and a floor do not show the same patch.
PLANES = (("x", "yz"), ("y", "xz"), ("z", "xy"))
# (u0, v0, size) of the windows, per texture set and plane
WINDOWS = {
    "1": {"x": (0.03, 0.40, 0.22), "y": (0.765, 0.625, 0.17), "z": (0.03, 0.40, 0.22)},
    "2": {"x": (0.38, 0.47, 0.265), "y": (0.40, 0.04, 0.26), "z": (0.38, 0.47, 0.265)},
}


def tile3(name, uv, scale, win, sw):
    """HLSL statements that leave the seamless read of one plane in `v` (float3 for rgb, float2 offset for normals)."""
    u0, v0, sz = win
    lines = ["float2 q = WP.%s / %s; float2 gx = dWx.%s / %s * %.4f; float2 gy = dWy.%s / %s * %.4f;" % (uv, scale, uv, scale, sz, uv, scale, sz),
             "float3 acc = 0; float ws = 0; float w2 = 0;"]
    for k in range(3):
        lines.append("{ float2 f = frac(q + %.6f); float2 tt = 1.0 - abs(f * 2.0 - 1.0); float wk = tt.x * tt.y + 0.0001; "
                     "acc += wk * Texture2DSampleGrad(%s, %sSampler, float2(%.4f, %.4f) + f * %.4f, gx, gy).rgb; ws += wk; w2 += wk * wk; }"
                     % (k / 3.0, name, name, u0, v0, sz))
    lines.append("float3 mean = Texture2DSampleLevel(%s, %sSampler, float2(0.5, 0.5), 12.0).rgb;" % (name, name))
    lines.append("float3 v = saturate(mean + (acc / ws - mean) * ws * rsqrt(w2));")
    return " ".join(lines)


def set_block(out, name, weight, cond, kind):
    """Accumulate one texture set into `out`, only where cond holds and only the planes that count."""
    sid = name[-1]
    scale = "sS" if sid == "1" else "sL"
    lines = ["[branch] if (%s) {" % cond, "  float3 t = 0;"]
    for c, uv in PLANES:
        body = tile3(name, uv, scale, WINDOWS[sid][c], kind)
        if kind == "rgb":
            lines.append("  [branch] if (w.%s > 0.0) { %s t += w.%s * v; }" % (c, body, c))
        else:
            off = {"x": "float3(0.0, n.x, n.y)", "y": "float3(n.x, 0.0, n.y)", "z": "float3(n.x, n.y, 0.0)"}[c]
            lines.append("  [branch] if (w.%s > 0.0) { %s float2 n = v.xy * 2.0 - 1.0; t += w.%s * %s; }" % (c, body, c, off))
    lines.append("  %s += (%s) * t;" % (out, weight))
    lines.append("}")
    return "\n".join(lines) + "\n"


def sets(out, n1, n2, kind):
    return ("float3 %s = 0;\n" % out + set_block(out, n1, "1.0 - mSet", "mSet < 0.999", kind)
            + set_block(out, n2, "mSet", "mSet > 0.001", kind))


PRELUDE = r"""
float3 Nv = normalize(VN);
float3 dWx = ddx(WP);
float3 dWy = ddy(WP);
float3 Ng = cross(dWx, dWy);
Ng = (dot(Ng, Ng) > 1e-12) ? normalize(Ng) : Nv;
float3 Nw = normalize(lerp(Nv, Ng * sign(dot(Ng, Nv) + 1e-6), GeoBlend));
float3 w = pow(abs(Nw), 8.0);
w = w / (w.x + w.y + w.z + 0.00001);
w = w * step(0.03, w);
w = w / (w.x + w.y + w.z + 0.00001);
float sS = max(TileSmallM, 0.1) * 100.0;
float sL = max(TileLargeM, 0.1) * 100.0;
float a2 = sin(WP.y * 0.0013 + 1.1 * sin(WP.z * 0.0011 + 0.4)) + sin(WP.z * 0.0016 + WP.x * 0.0012 + 2.1);
float mSet = smoothstep(0.3, 0.7, saturate(0.5 + 0.35 * a2));
"""

# x: silt (faces up), y: flowstone (walls, drapes), z: cavity (1 open, 0 deep in a crack), w: macro value shift
MASKS_HLSL = r"""
float3 nt = normalize(VN);
float2 p = WP.xy * 0.01;
float wob = 0.5 * sin(1.7 * p.x + 1.3 * sin(0.9 * p.y)) + 0.5 * sin(2.3 * p.y - 1.1 * p.x + 0.7);
float silt = saturate((nt.z - (1.0 - Amount) + 0.12 * wob) * Sharpness);
float f1 = sin(WP.x * 0.0031 + 1.9 * sin(WP.y * 0.0023 + WP.z * 0.0006));
float f2 = sin(WP.y * 0.0029 - 1.5 * sin(WP.x * 0.0019 + 0.8) + WP.z * 0.0005);
float f3 = sin(WP.z * 0.0009 + 0.6 * f1);
float fn = (f1 + f2) * 0.35 + 0.3 * f3;
float wall = 1.0 - smoothstep(0.35, 0.7, abs(nt.z));
float flow = smoothstep(FlowCover, FlowCover + 0.25, fn) * wall * (1.0 - silt) * FlowAmount;
float cav = saturate((VC.r - CavLo) / max(CavHi - CavLo, 0.001));
cav = cav * cav * (3.0 - 2.0 * cav);
float m1 = sin(WP.x * 0.00062 + 1.4 * sin(WP.y * 0.00049 + 0.3)) * sin(WP.y * 0.00057 - 1.2 * sin(WP.z * 0.00071 + 1.1) + 0.9);
float m2 = sin(WP.z * 0.00083 + WP.x * 0.00041 + 2.3 + 0.8 * sin(WP.y * 0.00038));
float macro = clamp(0.75 * m1 + 0.4 * m2, -1.0, 1.0);
return float4(silt, flow, cav, macro);
"""

COLOR_HLSL = PRELUDE + sets("c", "B1", "B2", "rgb") + """
float l = dot(c, float3(0.2126, 0.7152, 0.0722));
float3 rock = lerp(l.xxx, c, Saturation) * Value;
rock *= 1.0 + MacroAmount * M.w;
float3 flowc = lerp(dot(rock, float3(0.2126, 0.7152, 0.0722)).xxx, rock, 0.6) * FlowLift + 0.006;
rock = lerp(rock, flowc, M.y);
float mx = max(rock.r, max(rock.g, rock.b));
float mn = min(rock.r, min(rock.g, rock.b));
float s = (mx - mn) / max(mx, 0.0001);
float k = (s > 0.35) ? (0.35 / s) : 1.0;
rock = mx - (mx - rock) * k;
rock = min(rock, 0.25);
float2 p = WP.xy * 0.01;
float m = saturate(0.5 + 0.5 * sin(0.37 * p.x + 1.7 * sin(0.21 * p.y + 0.4)));
float3 silt = lerp(SiltDark.rgb, SiltLight.rgb, m) * (0.8 + 0.4 * saturate(l * 2.5));
float3 col = lerp(rock, silt, M.x);
return col * lerp(CavDark, 1.0, M.z);
"""


NORMAL_HLSL = PRELUDE + sets("off", "N1", "N2", "n") + r"""
off *= Strength * (1.0 - 0.55 * M.y);
float3 nrock = normalize(Nv + off);
return normalize(lerp(nrock, Nv, M.x * 0.55));
"""

ORM_HLSL = PRELUDE + sets("o", "O1", "O2", "rgb") + """
float rock = lerp(RoughMin, RoughMax, saturate(o.g));
rock = lerp(rock, FlowRough, M.y);
float ao = o.r * lerp(CavDark, 1.0, M.z);
return float3(ao, lerp(rock, 0.9, M.x), 0.5 * lerp(CavSpec, 1.0, M.z));
"""


# ---- helpers (as in build_cave_materials.py) -------------------------------------------------------------------------

def fresh(name):
    full = PATH + "/" + name
    mat = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else None
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def scalar(mat, name, value, x, y, group):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    e.set_editor_property("group", group)
    return e


def vector(mat, name, rgb, x, y, group="Silt"):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    e.set_editor_property("group", group)
    return e


def custom(mat, code, out_type, inputs, desc, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("code", code)
    e.set_editor_property("output_type", out_type)
    e.set_editor_property("description", desc)
    ins = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    e.set_editor_property("inputs", ins)
    return e


def link(src, out, dst, inp):
    if not mel.connect_material_expressions(src, out, dst, inp):
        msg = "CAVEWALL could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp)
        errors_seen.append(msg)
        unreal.log_error(msg)


def sampler_for(path):
    t = unreal.load_asset(path)
    if not t:
        msg = "CAVEWALL no texture %s" % path
        errors_seen.append(msg)
        unreal.log_error(msg)
        return None, None
    cs = t.get_editor_property("compression_settings")
    if t.get_editor_property("virtual_texture_streaming"):
        errors_seen.append("CAVEWALL %s streams virtually; a Custom-node Texture2DSample cannot read it" % path)
    S = unreal.MaterialSamplerType
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return t, S.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS:
        return t, S.SAMPLERTYPE_MASKS
    if not t.get_editor_property("srgb"):
        return t, S.SAMPLERTYPE_LINEAR_COLOR
    return t, S.SAMPLERTYPE_COLOR


def tex_object(mat, path, x, y):
    t, sampler = sampler_for(path)
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObject, x, y)
    if t:
        e.set_editor_property("texture", t)
        e.set_editor_property("sampler_type", sampler)
    return e


def pick(mat, src, comp, desc, x, y):
    e = custom(mat, "return V.%s;" % comp, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["V"], desc, x, y)
    link(src, "", e, "V")
    return e


def build():
    mat = fresh(NAME)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("tangent_space_normal", False)
    for flag in ("used_with_instanced_static_meshes", "used_with_nanite"):
        mat.set_editor_property(flag, True)

    wp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1800, 800)
    vnw = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -2100, 950)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, -1800, 950)
    vn.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    vn.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(vnw, "", vn, "")
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1800, 1300)
    tile_s = scalar(mat, "TileSmallM", 3.0, -1800, 1100, "Tiling")
    tile_l = scalar(mat, "TileLargeM", 5.0, -1800, 1180, "Tiling")
    geo = scalar(mat, "GeoBlend", 1.0, -1800, 1240, "Tiling")

    masks = custom(mat, MASKS_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                   ["VN", "WP", "VC", "Amount", "Sharpness", "FlowAmount", "FlowCover", "CavLo", "CavHi"], "CaveMasks", -900, 900)
    link(vn, "", masks, "VN")
    link(wp, "", masks, "WP")
    link(vc, "", masks, "VC")
    link(scalar(mat, "SiltAmount", 0.45, -1300, 900, "Silt"), "", masks, "Amount")
    link(scalar(mat, "SiltSharpness", 6.0, -1300, 980, "Silt"), "", masks, "Sharpness")
    link(scalar(mat, "FlowAmount", 1.0, -1300, 1060, "Flowstone"), "", masks, "FlowAmount")
    link(scalar(mat, "FlowCover", 0.35, -1300, 1140, "Flowstone"), "", masks, "FlowCover")
    link(scalar(mat, "CavLo", 0.35, -1300, 1220, "Cavity"), "", masks, "CavLo")
    link(scalar(mat, "CavHi", 0.9, -1300, 1300, "Cavity"), "", masks, "CavHi")
    cav_dark = scalar(mat, "CavDark", 0.3, -1300, 1380, "Cavity")

    def common(node):
        link(wp, "", node, "WP")
        link(vn, "", node, "VN")
        link(tile_s, "", node, "TileSmallM")
        link(tile_l, "", node, "TileLargeM")
        link(geo, "", node, "GeoBlend")
        link(masks, "", node, "M")

    color = custom(mat, COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ["B1", "B2", "WP", "VN", "TileSmallM", "TileLargeM", "GeoBlend", "M", "Saturation", "Value", "SiltDark", "SiltLight",
                    "MacroAmount", "FlowLift", "CavDark"], "CaveColor", -300, -400)
    link(tex_object(mat, TEX + "T_CaveRocks01_BC", -1300, -600), "", color, "B1")
    link(tex_object(mat, TEX + "T_CaveRocks02_BC", -1300, -500), "", color, "B2")
    common(color)
    link(scalar(mat, "Saturation", 0.5, -1300, -300, "Grade"), "", color, "Saturation")
    link(scalar(mat, "Value", 0.35, -1300, -220, "Grade"), "", color, "Value")
    link(vector(mat, "SiltDark", (0.095, 0.088, 0.076), -1300, -140), "", color, "SiltDark")
    link(vector(mat, "SiltLight", (0.165, 0.152, 0.13), -1300, -60), "", color, "SiltLight")
    link(scalar(mat, "MacroAmount", 0.15, -1300, 20, "Grade"), "", color, "MacroAmount")
    link(scalar(mat, "FlowLift", 1.3, -1300, 1460, "Flowstone"), "", color, "FlowLift")
    link(cav_dark, "", color, "CavDark")

    normal = custom(mat, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    ["N1", "N2", "WP", "VN", "TileSmallM", "TileLargeM", "GeoBlend", "M", "Strength"], "CaveNormal", -300, 0)
    link(tex_object(mat, TEX + "T_CaveRocks01_N", -1300, 100), "", normal, "N1")
    link(tex_object(mat, TEX + "T_CaveRocks02_N", -1300, 200), "", normal, "N2")
    common(normal)
    link(scalar(mat, "NormalStrength", 1.0, -1300, 300, "Grade"), "", normal, "Strength")

    orm = custom(mat, ORM_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                 ["O1", "O2", "WP", "VN", "TileSmallM", "TileLargeM", "GeoBlend", "M", "RoughMin", "RoughMax", "FlowRough", "CavDark",
                  "CavSpec"], "CaveORM", -300, 400)
    link(tex_object(mat, TEX + "T_CaveRocks01_ORM", -1300, 500), "", orm, "O1")
    link(tex_object(mat, TEX + "T_CaveRocks02_ORM", -1300, 600), "", orm, "O2")
    common(orm)
    link(scalar(mat, "RoughMin", 0.60, -1300, 700, "Wet"), "", orm, "RoughMin")
    link(scalar(mat, "RoughMax", 0.70, -1300, 780, "Wet"), "", orm, "RoughMax")
    link(scalar(mat, "FlowRough", 0.30, -1300, 1540, "Flowstone"), "", orm, "FlowRough")
    link(cav_dark, "", orm, "CavDark")
    link(scalar(mat, "CavSpec", 0.15, -1300, 1620, "Cavity"), "", orm, "CavSpec")

    ao = pick(mat, orm, "x", "CaveAO", 100, 350)
    rough = pick(mat, orm, "y", "CaveRoughness", 100, 450)
    spec = pick(mat, orm, "z", "CaveSpecular", 100, 550)

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nworld = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, 100, 0)
    nworld.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    nworld.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(normal, "", nworld, "")
    mel.connect_material_property(nworld, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("CAVEWALL not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        errors_seen.append(str(e))
        unreal.log_error("CAVEWALL compile: %s" % str(e))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    saved = unreal.EditorAssetLibrary.load_asset(PATH + "/" + NAME)
    flags = ["tangent_space_normal", "two_sided", "used_with_instanced_static_meshes", "used_with_nanite"]
    unreal.log("CAVEWALL readback " + " ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in flags))
    if ok and not errors_seen:
        unreal.log("CAVEWALL ok %s expressions=%d" % (NAME, mel.get_num_material_expressions(mat)))
    else:
        unreal.log_error("CAVEWALL FAILED saved=%s problems=%d: %s" % (ok, len(errors_seen), "; ".join(errors_seen[:5])))
    return ok and not errors_seen


build()
