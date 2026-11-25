# Rebuilds the immersion lane's material from text (D-072). Run headless (no editor window), in the machine slot:
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering
# Recipe and sources: deep-claude-brain/05 - Log/Lane Notes/immersion.md (Silt: the recipe).
#
#   M_SiltPuff : one puff of lifted silt (ADeepSilt draws each puff as an instance of the engine sphere). Translucent,
#                lit through the translucency lighting volume (Volumetric NonDirectional: Epic's mode for smoke and dust,
#                no normal, shadowed light from every movable light, so the lamp and the floods light it and nothing
#                else does: style guide 2, light from sources). A soft edge from the sphere's facing (an inverted
#                Fresnel raised to a power), a slow world-space lumpiness so no two puffs read as balls, the instance's
#                own opacity (PerInstanceCustomData 0, set each frame from the puff's age), Depth Fade where it meets
#                rock (no seam on the cave walls), and a fade near the camera so the chase view never pops through
#                one. Albedo is a parameter (FDeepSiltTuning::Albedo): grey-brown, saturation under 35% (style guide 4).
import unreal

PATH = "/Game/Deep/Immersion"
mel = unreal.MaterialEditingLibrary

PUFF_HLSL = r"""
float facing = saturate(dot(normalize(N), normalize(V)));
// The sphere's rim is the puff's thin edge, its middle the thick core.
float soft = pow(facing, EdgePower);
// Lumpy, slowly turning density: three crossed sine ridges in world space (no texture), never the same twice.
float3 p = WP * NoiseScale + float3(0.0, 0.0, Time * 0.04);
float n = (sin(p.x * 1.7 + sin(p.y * 1.3)) + sin(p.y * 2.1 + sin(p.z * 1.1)) + sin(p.z * 1.9 + sin(p.x * 0.7))) / 6.0 + 0.5;
float lumpy = lerp(1.0 - Lumps, 1.0, n);
// Never a hard pop as the chase camera passes through a puff: gone within NearCm of the eye.
float nearFade = saturate((Depth - NearCm * 0.3) / max(NearCm, 1.0));
return saturate(soft * lumpy * Alpha * Density * nearFade);
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
        unreal.log_error("DEEPIMMMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def finish(mat):
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DEEPIMMMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DEEPIMMMAT compile %s: %s" % (mat.get_name(), str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DEEPIMMMAT %s saved=%s expressions=%d errors=%d" % (
        mat.get_name(), ok, mel.get_num_material_expressions(mat), len(errors or [])))


def build_puff():
    mat = fresh("M_SiltPuff")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    mat.set_editor_property("two_sided", False)
    # Drawn on instanced spheres only: the one usage flag it needs (a cooked game has no fallback for a missing flag).
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    n = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1400, -200)
    v = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1400, -100)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1400, 100)
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1400, 200)
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1400, 300)
    alpha.set_editor_property("data_index", 0)
    alpha.set_editor_property("const_default_value", 0.5)

    puff = custom(mat, PUFF_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                  ["N", "V", "WP", "Time", "Depth", "Alpha", "EdgePower", "NoiseScale", "Lumps", "Density", "NearCm"], "SiltPuff", -600, 0)
    link(n, "", puff, "N")
    link(v, "", puff, "V")
    link(wp, "", puff, "WP")
    link(time, "", puff, "Time")
    link(depth, "", puff, "Depth")
    link(alpha, "", puff, "Alpha")
    # 1.6: a thick middle that thins well before the rim (1 read as a ball, 3 as a dot).
    link(scalar(mat, "EdgePower", 1.6, -1000, 200), "", puff, "EdgePower")
    # One lump every metre or so.
    link(scalar(mat, "NoiseScale", 0.012, -1000, 280), "", puff, "NoiseScale")
    link(scalar(mat, "Lumps", 0.45, -1000, 360), "", puff, "Lumps")
    link(scalar(mat, "Density", 1.0, -1000, 440), "", puff, "Density")
    link(scalar(mat, "NearCm", 150.0, -1000, 520), "", puff, "NearCm")

    # Soft where it meets rock or the hull: no seam (Epic: Depth Fade hides where translucency meets opaque).
    fade = mel.create_material_expression(mat, unreal.MaterialExpressionDepthFade, -250, 0)
    link(puff, "", fade, "")
    link(scalar(mat, "RockFadeCm", 90.0, -600, 250), "", fade, "FadeDistance")

    mel.connect_material_property(vector(mat, "Albedo", (0.30, 0.28, 0.24), -600, -300), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    finish(mat)
    saved = unreal.load_asset(PATH + "/M_SiltPuff")
    flags = ["blend_mode", "translucency_lighting_mode", "used_with_instanced_static_meshes", "two_sided"]
    unreal.log("DEEPIMMMAT M_SiltPuff readback " + " ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in flags))


build_puff()
