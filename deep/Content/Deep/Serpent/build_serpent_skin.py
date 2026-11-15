# Rebuilds the serpent's two materials from text. Run headless (no editor window):
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash
#     -AllowCommandletRendering -ModelContextProtocolPort=8011
# Values come from the Style Guide, section 6.
#   M_SerpentSkin  : near-black wet skin, roughness 0.25 broken up, a fine wet-fold normal. Lit only by the lamp.
#   M_SerpentMarks : the markings as an unlit additive layer drawn just over the skin: a cyan-green fresnel rim and
#                    two rows of small spots with a travelling pulse. A layer, so it still works when the body or
#                    head wears another material (a pack dragon's). No other colour.
# ADeepSerpent drives the parameters each frame (pulse phase from the brain's threat, rim and spot gains, reveal).
# Mesh data the tube writes: UV1 = (metres from the neck toward the tail, 0..1 around the body with 0.25 on the
# dorsal ridge); UV0 = UV1 times the tiling settings (for a third-party body material); vertex colour R = the ring
# radius in metres.
import unreal

PATH = "/Game/Deep/Serpent"
mel = unreal.MaterialEditingLibrary

MARKS_HLSL = r"""
float rad = max(Rad, 0.04);
float cell = UV.x / max(Spacing, 0.05);
float du = (frac(cell) - 0.5) * Spacing;
float circ = 6.2831853 * rad;
float dv = min(abs(UV.y - RowV), abs(UV.y - (0.5 - RowV))) * circ;
float size = SpotSize * lerp(0.45, 1.0, saturate(rad / 0.45));
float spot = 1.0 - smoothstep(size * 0.35, size, sqrt(du * du + dv * dv));
float wave = 0.5 + 0.5 * sin(Phase - floor(cell) * Ripple);
float pulse = 0.2 + 0.8 * wave * wave;
float rim = saturate(Fres) * RimGain;
return MarkColor.rgb * (spot * pulse * SpotGain + rim);
"""

# Wet folds: a few crossing sines in metres with an analytic slope, tangent space (X along the body).
NORMAL_HLSL = r"""
float2 p = float2(UV.x, UV.y * 6.2831853 * max(Rad, 0.04));
float gx = 0.0; float gy = 0.0; float a;
a = 11.0 * p.x + 3.0 * p.y;  gx += 0.012 * 11.0 * cos(a); gy += 0.012 * 3.0 * cos(a);
a = -5.0 * p.x + 13.0 * p.y; gx += 0.010 * -5.0 * cos(a); gy += 0.010 * 13.0 * cos(a);
a = 23.0 * p.x - 17.0 * p.y; gx += 0.004 * 23.0 * cos(a); gy += 0.004 * -17.0 * cos(a);
a = 3.0 * p.x + 29.0 * p.y;  gx += 0.003 * 3.0 * cos(a);  gy += 0.003 * 29.0 * cos(a);
return normalize(float3(-gx * Strength, -gy * Strength, 1.0));
"""

# Roughness broken up so the beam's highlight is patchy, as wet skin is.
ROUGH_HLSL = r"""
float2 p = float2(UV.x, UV.y * 6.2831853 * max(Rad, 0.04));
float n = 0.5 * sin(4.1 * p.x + 2.0 * sin(3.3 * p.y)) + 0.5 * sin(1.7 * p.x - 2.9 * p.y + 1.3);
return clamp(Base + Var * n, 0.05, 0.6);
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
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    uv, vc = body_inputs(mat)
    # Near-black, faintly blue-green (saturation and value far under the section 4 caps).
    skin = vector(mat, "SkinColor", (0.012, 0.016, 0.016), -400, -300)

    normal = custom(mat, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["UV", "Rad", "Strength"],
                    "SerpentWetNormal", -400, 0)
    link(uv, "", normal, "UV")
    link(vc, "R", normal, "Rad")
    link(scalar(mat, "NormalStrength", 1.0, -900, 100), "", normal, "Strength")

    rough = custom(mat, ROUGH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["UV", "Rad", "Base", "Var"],
                   "SerpentWetRoughness", -400, 200)
    link(uv, "", rough, "UV")
    link(vc, "R", rough, "Rad")
    link(scalar(mat, "Roughness", 0.25, -900, 250), "", rough, "Base")
    link(scalar(mat, "RoughnessVariation", 0.07, -900, 330), "", rough, "Var")

    mel.connect_material_property(skin, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Specular", 0.6, -400, 150), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
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
    # Cyan-green (#3FE0B0, chosen here) for the rim and spots.
    mark = vector(mat, "MarkColor", (0.05, 0.75, 0.43), -900, 600)

    marks = custom(mat, MARKS_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ["UV", "Rad", "Fres", "Phase", "SpotGain", "RimGain", "MarkColor", "RowV", "Spacing", "SpotSize", "Ripple"],
                   "SerpentMarks", -400, 400)
    link(uv, "", marks, "UV")
    link(vc, "R", marks, "Rad")
    link(fres, "", marks, "Fres")
    link(mark, "", marks, "MarkColor")
    for i, (name, value) in enumerate([("PulsePhase", 0.0), ("SpotGain", 6.0), ("RimGain", 1.5), ("RowV", 0.14),
                                       ("SpotSpacing", 0.5), ("SpotSize", 0.07), ("Ripple", 0.9)]):
        p = scalar(mat, name, value, -900, 800 + i * 90)
        pin = {"PulsePhase": "Phase", "SpotSpacing": "Spacing"}.get(name, name)
        link(p, "", marks, pin)
    mel.connect_material_property(marks, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat, [])


build_skin()
build_marks()
