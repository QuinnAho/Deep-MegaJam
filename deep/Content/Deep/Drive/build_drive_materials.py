# Builds the drive lane's materials from text (D2, BT-01's cabin). Run headless (no editor window), in the machine slot:
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering
# Recipe and sources: deep-claude-brain/05 - Log/Lane Notes/drive.md (D2, "The bow ports: the recipe").
#
#   M_PortCut  : SM_BT01's Glass slot with the bow ports cut out. SM_BT01 is Nanite and its port glass is opaque (every
#                slot is an M_SubPBR instance); a translucent material on a Nanite mesh is not supported
#                (Nanite::IsSupportedBlendMode, r.Nanite.AllowTranslucency off) and drops the whole hull to its fallback
#                mesh. Masked is supported (programmable raster), so this is M_SubPBR's look for the Glass texture set
#                (graded the same: Saturation 0.7, Tint) with Opacity Mask 0 inside a box in the mesh's own space
#                (CutMin/CutMax, set per hull from C++). Used with Nanite (the flag a cooked game needs).
#   M_PortGlass: the pane that fills each port: thick pressure glass, translucent, lit per pixel (Surface ForwardShading,
#                so Lumen's front-layer reflections, on for the project, put a faint copy of the lit cabin on it), nearly
#                clear face on and a mirror at a slant (Fresnel opacity), with a film of grime that thickens toward the
#                rim, broken up by low world-free noise from the pane's own local position (no texture). Drawn on the
#                engine cylinder (radius 50 units in its local XY), scaled to the port.
import unreal

PATH = "/Game/Deep/Drive"
TEX = "/Game/Deep/Subs/Textures"
mel = unreal.MaterialEditingLibrary

CUT_HLSL = r"""
float3 p = LP;
float3 lo = min(CutMin.xyz, CutMax.xyz);
float3 hi = max(CutMin.xyz, CutMax.xyz);
bool inside = all(p > lo) && all(p < hi);
return inside ? 0.0 : 1.0;
"""

GRIME_HLSL = r"""
float r = length(LP.xy) / max(Radius, 1.0);
// Low, lumpy noise from the pane's own position: dried film, not a smooth vignette.
float n = 0.5 + 0.25 * sin(LP.x * 0.31 + 1.7 * sin(LP.y * 0.23)) + 0.25 * sin(LP.y * 0.37 + 1.3 * sin(LP.x * 0.19 + 0.7));
float ring = smoothstep(GrimeStart, 1.0, r);
// A few drips running down from the rim (the pane's local -y is down once placed upright).
float drip = smoothstep(0.75, 1.0, frac(LP.x * 0.055 + 0.37)) * smoothstep(0.35, 1.0, r) * step(LP.y, 0.0);
return saturate(Film + ring * lerp(0.45, 1.0, n) + 0.35 * drip * n);
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


def vector(mat, name, rgba, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgba[0], rgba[1], rgba[2], rgba[3] if len(rgba) > 3 else 1.0))
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
        unreal.log_error("DEEPDRIVEMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def lerp(mat, a, b, alpha, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, "", e, "A")
    link(b, "", e, "B")
    link(alpha, "", e, "Alpha")
    return e


def sampler(mat, name, short, kind, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", unreal.load_asset("%s/T_BT01_Glass_%s" % (TEX, short)))
    e.set_editor_property("sampler_type", kind)
    return e


def finish(mat):
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DEEPDRIVEMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DEEPDRIVEMAT compile %s: %s" % (mat.get_name(), str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DEEPDRIVEMAT %s saved=%s expressions=%d errors=%d" % (
        mat.get_name(), ok, mel.get_num_material_expressions(mat), len(errors or [])))


def build_cut():
    mat = fresh("M_PortCut")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_nanite", True)
    S = unreal.MaterialSamplerType
    bc = sampler(mat, "BaseColor", "BC", S.SAMPLERTYPE_COLOR, -900, -400)
    rough = sampler(mat, "Roughness", "R", S.SAMPLERTYPE_MASKS, -900, -100)
    metal = sampler(mat, "Metallic", "M", S.SAMPLERTYPE_MASKS, -900, 150)
    ao = sampler(mat, "AO", "AO", S.SAMPLERTYPE_MASKS, -900, 400)
    # The grade M_SubPBR gives every BT-01 set (build_sub_materials.py): desaturate to 0.7, then a darker, cool tint.
    sat = scalar(mat, "Saturation", 0.7, -900, -600)
    one_minus = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -650, -600)
    link(sat, "", one_minus, "")
    desat = mel.create_material_expression(mat, unreal.MaterialExpressionDesaturation, -500, -400)
    link(bc, "RGB", desat, "")
    link(one_minus, "", desat, "Fraction")
    tint = vector(mat, "Tint", (0.62, 0.66, 0.66, 1.0), -500, -650)
    graded = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -450)
    link(desat, "", graded, "A")
    link(tint, "RGB", graded, "B")
    mel.connect_material_property(graded, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -900, 650)
    cut = custom(mat, CUT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["LP", "CutMin", "CutMax"], "PortCut", -400, 650)
    link(lp, "", cut, "LP")
    # Nothing is cut until the hull sets the box (min above max on every axis cuts nothing).
    link(vector(mat, "CutMin", (1.0, 1.0, 1.0, 0.0), -700, 750), "", cut, "CutMin")
    link(vector(mat, "CutMax", (0.0, 0.0, 0.0, 0.0), -700, 850), "", cut, "CutMax")
    mel.connect_material_property(cut, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat)
    saved = unreal.load_asset(PATH + "/M_PortCut")
    flags = ["blend_mode", "used_with_nanite", "opacity_mask_clip_value"]
    unreal.log("DEEPDRIVEMAT M_PortCut readback " + " ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in flags))


def build_glass():
    mat = fresh("M_PortGlass")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    mat.set_editor_property("two_sided", False)
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1400, 0)
    grime = custom(mat, GRIME_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["LP", "Radius", "GrimeStart", "Film"], "PortGrime", -1000, 0)
    link(lp, "", grime, "LP")
    link(scalar(mat, "Radius", 50.0, -1400, 100), "", grime, "Radius")
    link(scalar(mat, "GrimeStart", 0.62, -1400, 180), "", grime, "GrimeStart")
    link(scalar(mat, "Film", 0.04, -1400, 260), "", grime, "Film")
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -1000, 300)
    clear = lerp(mat, scalar(mat, "OpacityFacing", 0.05, -1000, 400), scalar(mat, "OpacityGrazing", 0.5, -1000, 480), fres, -700, 350)
    opacity = lerp(mat, clear, scalar(mat, "GrimeOpacity", 0.8, -700, 500), grime, -400, 350)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    base = lerp(mat, vector(mat, "Tint", (0.012, 0.016, 0.016, 1.0), -700, -300), vector(mat, "GrimeColor", (0.045, 0.04, 0.032, 1.0), -700, -200), grime, -400, -250)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lerp(mat, scalar(mat, "Roughness", 0.04, -700, 100), scalar(mat, "GrimeRoughness", 0.6, -700, 180), grime, -400, 120)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Specular", 0.5, -400, 250), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(mat)
    saved = unreal.load_asset(PATH + "/M_PortGlass")
    flags = ["blend_mode", "translucency_lighting_mode", "two_sided"]
    unreal.log("DEEPDRIVEMAT M_PortGlass readback " + " ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in flags))


build_cut()
build_glass()
