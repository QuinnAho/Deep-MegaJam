# Rebuilds the serpent's two materials from text. Run headless (no editor window):
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash
#     -AllowCommandletRendering -ModelContextProtocolPort=8011
# Values come from the Style Guide, section 6, and the second pass (devlog, Nov 15 "serpent 2").
#   M_SerpentSkin  : near-black skin under a wet clear coat. Round overlapping scales (offset rows, the front one
#                    on top) plus slow folds in the normal, so the lamp's highlight breaks into glints. The
#                    grooves between scales are rougher (silt), the scale crowns wetter. Lit only by the lamp.
#   M_SerpentMarks : the markings as an unlit additive layer drawn just over the skin: two sparse rows of small
#                    photophores low on the flanks, about half the cells empty, a travelling pulse. They fade at
#                    grazing angles so they never trace the outline. The fresnel rim term is kept but its gain
#                    defaults to 0 (the rim outline made it a cartoon eel).
# ADeepSerpent drives the parameters each frame (pulse phase from the brain's threat, spot gain, reveal).
# Mesh data the tube writes: UV1 = (metres from the neck toward the tail, 0..1 around the body with 0.25 on the
# dorsal ridge); UV0 = UV1 times the tiling settings (for a third-party body material); vertex colour R = the ring
# radius in metres; tangent X runs along the body toward the tail (so tangent-space normals line up with UV1.x).
import unreal

PATH = "/Game/Deep/Serpent"
mel = unreal.MaterialEditingLibrary

MARKS_HLSL = r"""
float rad = max(Rad, 0.04);
float cell = UV.x / max(Spacing, 0.05);
float id = floor(cell);
float du = (frac(cell) - 0.5) * Spacing;
float circ = 6.2831853 * rad;
float d1 = abs(UV.y - RowV);
float d2 = abs(UV.y - (1.5 - RowV));
float side = d1 < d2 ? 0.0 : 1.0;
float dv = min(d1, d2) * circ;
float rnd = frac(sin(id * 12.9898 + side * 78.233) * 43758.5453);
float keep = step(Sparse, rnd);
float size = SpotSize * lerp(0.45, 1.0, saturate(rad / 0.45)) * lerp(0.7, 1.25, frac(rnd * 7.13));
float spot = (1.0 - smoothstep(size * 0.25, size, sqrt(du * du + dv * dv))) * keep;
float wave = 0.5 + 0.5 * sin(Phase - id * Ripple);
float pulse = 0.1 + 0.9 * wave * wave * wave;
float face = saturate(1.0 - Fres * 3.0);
float rim = saturate(Fres) * RimGain;
return MarkColor.rgb * (spot * pulse * face * SpotGain + rim);
"""

# Scales: round overlapping scales in metres on UV1 (x along the body toward the tail, y around), offset every other
# row, like a fish or a snake: of the scales whose disc covers a point, the one nearer the head lies on top (its free
# edge points to the tail). Each is a low dome with a random height, so the lamp catches a scatter of small glints
# and dark crescent edges instead of one smooth highlight. The finite-difference step is the pixel footprint, and the
# scales fade out when they get smaller than a few pixels (no shimmer at range). Slow folds on top. Tangent Y runs
# against UV1.y (tangent X along the body, normal out), hence +hy.
SCALE_STRUCT = r"""
struct FScales {
  float H(float2 p, float sz) {
    float2 g = p / float2(sz * 0.7, sz);
    float2 c0 = floor(g);
    float best = -1e5;
    float h = 0.0;
    for (int j = -1; j <= 1; ++j) {
      for (int i = -1; i <= 1; ++i) {
        float2 c = c0 + float2(i, j);
        float off = 0.5 * (c.y - 2.0 * floor(c.y * 0.5));
        float2 ctr = float2(c.x + off + 0.5, c.y + 0.5);
        float2 d = (g - ctr) * float2(0.7, 1.0);
        float rnd = frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453);
        float r = length(d) / (0.74 + 0.08 * rnd);
        if (r < 1.0 && -ctr.x > best) {
          best = -ctr.x;
          h = sqrt(1.0 - r * r) * (0.6 + 0.4 * rnd);
        }
      }
    }
    return h;
  }
};
FScales S;
float2 p = float2(UV.x, UV.y * 6.2831853 * max(Rad, 0.04));
float sz = Size * lerp(0.45, 1.0, saturate(Rad / 0.5));
float fw = max(length(fwidth(p)), 1e-5);
float e = clamp(fw, 0.0015, sz * 0.15);
float lod = saturate(sz / (fw * 8.0) - 0.25);
float h = S.H(p, sz);
"""
NORMAL_HLSL = SCALE_STRUCT + r"""
float hx = (S.H(p + float2(e, 0.0), sz) - h) / e;
float hy = (S.H(p + float2(0.0, e), sz) - h) / e;
float k = Depth * sz * lod;
float gx = -hx * k;
float gy = hy * k;
float a;
a = 2.3 * p.x + 0.9 * p.y;  gx += 0.05 * 2.3 * cos(a); gy -= 0.05 * 0.9 * cos(a);
a = -1.1 * p.x + 3.1 * p.y; gx += 0.04 * -1.1 * cos(a); gy -= 0.04 * 3.1 * cos(a);
return normalize(float3(gx * Strength, gy * Strength, 1.0));
"""

ROUGH_HLSL = SCALE_STRUCT + r"""
float n = 0.5 * sin(4.1 * p.x + 2.0 * sin(3.3 * p.y)) + 0.5 * sin(1.7 * p.x - 2.9 * p.y + 1.3);
float crown = lerp(0.5, h, lod);
return clamp(lerp(Groove, Base, crown) + Var * n, 0.05, 0.9);
"""


def fresh(name):
    full = PATH + "/" + name
    mat = unreal.load_asset(full)
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def scalar(mat, name, value, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector(mat, name, rgb, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
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
        unreal.log_error("SERPENTSKIN could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def body_inputs(mat):
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    uv.set_editor_property("coordinate_index", 1)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -1200, 200)
    return uv, vc


def finish(mat, usages):
    for usage in usages:
        mel.set_material_usage(mat, usage)
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:  # no renderer: the material still saves and compiles on first load
        unreal.log_warning("SERPENTSKIN not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("SERPENTSKIN compile %s: %s" % (mat.get_name(), str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("SERPENTSKIN %s saved=%s expressions=%d errors=%d" % (
        mat.get_name(), ok, mel.get_num_material_expressions(mat), len(errors or [])))


def build_skin():
    mat = fresh("M_SerpentSkin")
    # Clear coat: a thin wet film (sharp, low roughness) over a duller scaled skin, the engine's two-layer model.
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    uv, vc = body_inputs(mat)
    # Near-black, faintly blue-green (saturation and value far under the section 4 caps).
    skin = vector(mat, "SkinColor", (0.008, 0.010, 0.010), -400, -300)
    size = scalar(mat, "ScaleSize", 0.085, -900, 20)
    depth = scalar(mat, "ScaleDepth", 0.16, -900, 60)

    normal = custom(mat, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    ["UV", "Rad", "Strength", "Size", "Depth"], "SerpentScaleNormal", -400, 0)
    link(uv, "", normal, "UV")
    link(vc, "R", normal, "Rad")
    link(scalar(mat, "NormalStrength", 1.0, -900, 100), "", normal, "Strength")
    link(size, "", normal, "Size")
    link(depth, "", normal, "Depth")

    rough = custom(mat, ROUGH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                   ["UV", "Rad", "Base", "Groove", "Var", "Size"], "SerpentScaleRoughness", -400, 200)
    link(uv, "", rough, "UV")
    link(vc, "R", rough, "Rad")
    link(scalar(mat, "Roughness", 0.36, -900, 250), "", rough, "Base")
    link(scalar(mat, "GrooveRoughness", 0.55, -900, 290), "", rough, "Groove")
    link(scalar(mat, "RoughnessVariation", 0.06, -900, 330), "", rough, "Var")
    link(size, "", rough, "Size")

    mel.connect_material_property(skin, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Specular", 0.35, -400, 150), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    # Clear coat amount (Custom Data 0) and its roughness (Custom Data 1): the wet film.
    mel.connect_material_property(scalar(mat, "WetCoat", 0.8, -400, 400), "", unreal.MaterialProperty.MP_CUSTOM_DATA0)
    mel.connect_material_property(scalar(mat, "WetCoatRoughness", 0.18, -400, 450), "", unreal.MaterialProperty.MP_CUSTOM_DATA1)
    # The sphere fallback draws through an instanced static mesh.
    finish(mat, [unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES])


def build_marks():
    mat = fresh("M_SerpentMarks")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    uv, vc = body_inputs(mat)
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -1200, 400)
    fres.set_editor_property("exponent", 4.0)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    # Cyan-green (#3FE0B0, chosen here) for the spots.
    mark = vector(mat, "MarkColor", (0.05, 0.75, 0.43), -900, 600)

    marks = custom(mat, MARKS_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ["UV", "Rad", "Fres", "Phase", "SpotGain", "RimGain", "MarkColor", "RowV", "Spacing", "SpotSize",
                    "Ripple", "Sparse"],
                   "SerpentMarks", -400, 400)
    link(uv, "", marks, "UV")
    link(vc, "R", marks, "Rad")
    link(fres, "", marks, "Fres")
    link(mark, "", marks, "MarkColor")
    # RowV 0.6: two rows just below the flanks (0.6 and 0.9 around; 0.75 is the belly), photophore-style.
    for i, (name, value) in enumerate([("PulsePhase", 0.0), ("SpotGain", 2.5), ("RimGain", 0.0), ("RowV", 0.6),
                                       ("SpotSpacing", 0.9), ("SpotSize", 0.035), ("Ripple", 0.9), ("Sparse", 0.45)]):
        p = scalar(mat, name, value, -900, 800 + i * 90)
        pin = {"PulsePhase": "Phase", "SpotSpacing": "Spacing"}.get(name, name)
        link(p, "", marks, pin)
    mel.connect_material_property(marks, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat, [])


build_skin()
build_marks()
