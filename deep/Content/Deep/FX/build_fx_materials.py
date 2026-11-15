# Builds the two particle materials of the FX lane: M_Bubble (breath, hatch stream, winch churn) and M_SiltPuff.
# Run headless like the other lanes' scripts:
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -ModelContextProtocolPort=8011
# Recipe (devlog, Nov 15 "bubbles with Niagara"; Epic's Niagara `fundamentals` and `renderers` skills):
#   Bubble : a camera-facing sprite drawn as a sphere seen through. Translucent, Default Lit, Translucency Lighting
#            Mode "Surface ForwardShading" (fundamentals: translucent needs a directional/surface mode before
#            roughness, specular and normal do anything). A fake sphere normal from the sprite UV goes to Normal, so
#            the lamp puts a highlight on the side it comes from. Opacity is a fresnel term on that fake sphere: a thin
#            bright rim and a nearly clear middle. Emissive only on the rim, times MPC_FxLight.Spill (the diver's own
#            light, set by C++; try 4): in black water with the lamp off the rim
#            lit by nothing and the bubble is all but invisible (style guide: "light must come from a source").
#   Silt   : translucent, Volumetric NonDirectional (soft, cheap), a soft radial falloff with a little break-up,
#            silt colour from M_Silt's albedo range (0.09 to 0.16), lit per pixel (Surface ForwardShading: the first try with
#            Volumetric NonDirectional did not pick up the lamp's narrow cone at all), depth fade so it does not cut a line in the bed.
#   Both   : particle colour alpha fades them; usage flag "Used with Niagara Sprites" set HERE and read back from
#            the saved asset (a cooked game cannot set a flag on the fly: it shows the default grey material).
import unreal

PATH = "/Game/Deep/FX"
mel = unreal.MaterialEditingLibrary
TAG = "DEEPFXMAT"

RIM_HLSL = r"""
float2 c = UV * 2.0 - 1.0;
float r2 = dot(c, c);
if (r2 >= 1.0) return 0.0;
float nz = sqrt(1.0 - r2);
float fres = pow(1.0 - nz, RimPower);
float edge = 1.0 - smoothstep(0.92, 1.0, sqrt(r2));
return saturate((Clear + (1.0 - Clear) * fres) * edge) * Alpha;
"""

# Half a sphere: the side toward a light is brighter, but the far rim is never fully black, so the ring reads as
# a ring wherever light reaches it, and a highlight sits on the lit side.
NORMAL_HLSL = r"""
float2 c = UV * 2.0 - 1.0;
float r2 = min(dot(c, c), 0.999);
float3 n = float3(c.x, -c.y, sqrt(1.0 - r2));
return normalize(lerp(float3(0.0, 0.0, 1.0), n, Bend));
"""

SILT_HLSL = r"""
float2 c = UV * 2.0 - 1.0;
float r = length(c);
float soft = saturate(1.0 - r);
soft = soft * soft * (3.0 - 2.0 * soft);
float a = atan2(c.y, c.x);
float breakup = 0.75 + 0.25 * sin(a * 3.0 + Seed * 6.2831) * sin(a * 5.0 - Seed * 3.1);
return soft * breakup * Density * Alpha;
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


def vector(mat, name, v, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(v[0], v[1], v[2], 1.0))
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
        unreal.log_error("%s could not connect %s.%s -> %s.%s" % (TAG, src.get_name(), out, dst.get_name(), inp))


def finish(mat, name):
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    mat.set_editor_property("used_with_niagara_sprites", True)
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("%s not compiled here: %s" % (TAG, ex))
        errors = []
    for e in errors or []:
        unreal.log_error("%s compile: %s" % (TAG, e))
    for prop in ("MP_OPACITY", "MP_NORMAL", "MP_BASE_COLOR", "MP_EMISSIVE_COLOR"):
        try:
            node = mel.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, prop))
            unreal.log("%s %s %s <- %s" % (TAG, name, prop, node.get_name() if node else "NOTHING"))
        except Exception as ex:
            unreal.log_warning("%s readback %s: %s" % (TAG, prop, ex))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    back = unreal.load_asset(PATH + "/" + name)
    unreal.log("%s %s saved=%s expressions=%d errors=%d used_with_niagara_sprites=%s blend=%s tlm=%s" % (
        TAG, name, ok, mel.get_num_material_expressions(back), len(errors or []),
        back.get_editor_property("used_with_niagara_sprites"), back.get_editor_property("blend_mode"),
        back.get_editor_property("translucency_lighting_mode")))


def build_collection():
    """MPC_FxLight: Spill, set every frame by UDeepFxSubsystem from the local diver: 1 with the lamp on, less near the
    hatch light, a floor in the blue water at 100 m, 0 in black water with the lamp off."""
    full = PATH + "/MPC_FxLight"
    mpc = unreal.load_asset(full)
    if not mpc:
        mpc = unreal.AssetToolsHelpers.get_asset_tools().create_asset("MPC_FxLight", PATH, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    p = unreal.CollectionScalarParameter()
    p.set_editor_property("parameter_name", "Spill")
    p.set_editor_property("default_value", 0.0)
    mpc.set_editor_property("scalar_parameters", [p])
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mpc.get_outermost()], False)
    unreal.log("%s MPC_FxLight saved=%s scalars=%d" % (TAG, ok, len(mpc.get_editor_property("scalar_parameters"))))
    return mpc


def build_bubble(mpc):
    name = "M_Bubble"
    mat = fresh(name)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    pc = mel.create_material_expression(mat, unreal.MaterialExpressionParticleColor, -900, 200)

    rim = custom(mat, RIM_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["UV", "RimPower", "Clear", "Alpha"], "BubbleRim", -500, 0)
    link(uv, "", rim, "UV")
    link(scalar(mat, "RimPower", 2.5, -800, 80), "", rim, "RimPower")
    link(scalar(mat, "Clear", 0.04, -800, 140), "", rim, "Clear")
    link(pc, "A", rim, "Alpha")
    mel.connect_material_property(rim, "", unreal.MaterialProperty.MP_OPACITY)

    nrm = custom(mat, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["UV", "Bend"], "BubbleNormal", -500, 300)
    link(uv, "", nrm, "UV")
    link(scalar(mat, "NormalBend", 0.55, -800, 360), "", nrm, "Bend")
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)

    # A neutral film: the colour comes from the light that falls on it (lamp #F2F4EE, hatch amber, water blue).
    mel.connect_material_property(vector(mat, "FilmColour", (0.50, 0.53, 0.53), -500, -200), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Specular", 1.0, -500, 460), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(scalar(mat, "Roughness", 0.15, -500, 520), "", unreal.MaterialProperty.MP_ROUGHNESS)

    # The lamp's spill on the rim only (orchestrator, try 4): the lamp's narrow cone never reaches a bubble at the
    # mask, so the rim answers the diver's own light through Spill. With Spill 0 (black water, lamp off) it is dark.
    spill = mel.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, -500, 640)
    spill.set_editor_property("collection", mpc)
    spill.set_editor_property("parameter_name", "Spill")
    m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 600)
    link(rim, "", m1, "A")
    link(spill, "", m1, "B")
    m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 600)
    link(m1, "", m2, "A")
    link(vector(mat, "SpillColour", (0.55, 0.62, 0.66), -250, 720), "", m2, "B")
    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat, name)


def build_silt():
    name = "M_SiltPuff"
    mat = fresh(name)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    pc = mel.create_material_expression(mat, unreal.MaterialExpressionParticleColor, -900, 200)
    rnd = mel.create_material_expression(mat, unreal.MaterialExpressionParticleRandom, -900, 300)

    puff = custom(mat, SILT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["UV", "Seed", "Density", "Alpha"], "SiltPuff", -500, 0)
    link(uv, "", puff, "UV")
    link(rnd, "", puff, "Seed")
    link(scalar(mat, "Density", 0.8, -800, 120), "", puff, "Density")
    link(pc, "A", puff, "Alpha")

    fade = mel.create_material_expression(mat, unreal.MaterialExpressionDepthFade, -250, 0)
    link(puff, "", fade, "Opacity")
    link(scalar(mat, "FadeDistance", 40.0, -500, 160), "", fade, "FadeDistance")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)

    mel.connect_material_property(vector(mat, "SiltColour", (0.13, 0.12, 0.10), -500, -200), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 1.0, -500, 260), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat, name)


build_bubble(build_collection())
build_silt()
