# Rebuilds two water-look materials from text. Run headless (no editor window):
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash
#     -AllowCommandletRendering -ModelContextProtocolPort=8012
#   M_Silt          : the sea bed (ADeepAbyssFloor). Dark, rough silt: low, desaturated albedo with soft mottling, roughness
#                     0.85 to 0.95 (fine sediment is not glossy; style guide 4 keeps dry or dusty above 0.7), and a normal
#                     of small sand ripples bent by a long wave, plus fine grain. Worked out from world position, so the
#                     generated bed needs no texture. A scanned surface replaces it by path (FDeepLookTuning::SeabedMaterial).
#   M_SurfaceShafts : light function for the downlight. TexCoord 0 of a directional light function is the light-space
#                     position across the light (LightFunctionCommon.ush), in metres at LightFunctionScale 100. Soft
#                     drifting bands with dark gaps: the surface far above, cut into the volumetric fog as shafts.
import unreal

PATH = "/Game/Deep/Look"
mel = unreal.MaterialEditingLibrary

# World position in cm -> metres. Returns float3 base colour.
SILT_COLOR_HLSL = r"""
float2 p = WP.xy * 0.01;
float n = 0.0;
n += 0.50 * sin(0.37 * p.x + 1.7 * sin(0.21 * p.y + 0.4));
n += 0.30 * sin(0.93 * p.y - 0.61 * p.x + 2.0 * sin(0.47 * p.x));
n += 0.20 * sin(2.30 * p.x + 1.90 * p.y + 3.0 * sin(0.9 * p.y));
float m = saturate(0.5 + 0.5 * n);
float grain = frac(sin(dot(floor(p * 40.0), float2(12.9898, 78.233))) * 43758.5453);
float3 c = lerp(Dark.rgb, Light.rgb, m);
return c * (0.88 + 0.24 * grain);
"""

# Tangent-space normal (the bed's tangent runs along world X): ripples ~12 cm apart whose direction bends slowly,
# a softer cross set, and a hash grain. Slopes are analytic.
SILT_NORMAL_HLSL = r"""
float2 p = WP.xy * 0.01;
float bend = 0.6 * sin(0.11 * p.x + 0.07 * p.y) + 0.3 * sin(0.23 * p.y);
float2 d1 = float2(cos(0.5 + bend), sin(0.5 + bend));
float2 d2 = float2(cos(2.2 + 0.5 * bend), sin(2.2 + 0.5 * bend));
float k1 = 6.2831853 / 0.12;
float k2 = 6.2831853 / 0.31;
float a1 = k1 * dot(p, d1) + 1.5 * sin(0.8 * p.y) + 2.5 * sin(0.37 * p.x - 0.29 * p.y) + 0.9 * sin(1.9 * p.x + 1.3 * p.y);
float a2 = k2 * dot(p, d2);
// Ripples are sharper on the crest: slope of sin plus a little of its second harmonic.
float s1 = cos(a1) + 0.35 * cos(2.0 * a1);
float2 g = d1 * (0.0035 * k1 * s1) + d2 * (0.002 * k2 * cos(a2));
float fade = saturate(1.0 - Dist / 4000.0);
float2 h = frac(sin(float2(dot(floor(p * 60.0), float2(127.1, 311.7)), dot(floor(p * 60.0), float2(269.5, 183.3)))) * 43758.5453) - 0.5;
g = g * Strength * fade + h * Grain;
return normalize(float3(-g.x, -g.y, 1.0));
"""

SILT_ROUGH_HLSL = r"""
float2 p = WP.xy * 0.01;
float n = 0.5 * sin(0.53 * p.x + 1.1 * sin(0.31 * p.y)) + 0.5 * sin(0.77 * p.y - 0.4 * p.x);
return clamp(Base + Var * n, 0.6, 1.0);
"""

# Light-space metres. Soft bands that drift and change; output 0..1 brightness of the downlight.
SHAFT_HLSL = r"""
float2 p = UV.xy;
float t = Time * Speed;
float a = sin(0.55 * p.x + 0.35 * sin(0.4 * p.y + t) + t * 0.7);
float b = sin(0.47 * p.y - 0.30 * p.x + 0.5 * sin(0.3 * p.x - t * 0.8) - t * 0.5);
float c = sin(1.30 * p.x + 0.90 * p.y + t * 1.1);
float v = 0.45 * a + 0.4 * b + 0.15 * c;
float bands = smoothstep(0.05, 0.75, v);
return lerp(Floor, 1.0, bands);
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
        unreal.log_error("DEEPLOOKMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def finish(mat):
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DEEPLOOKMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DEEPLOOKMAT compile %s: %s" % (mat.get_name(), str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DEEPLOOKMAT %s saved=%s expressions=%d errors=%d" % (
        mat.get_name(), ok, mel.get_num_material_expressions(mat), len(errors or [])))


def build_silt():
    mat = fresh("M_Silt")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", True)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    dist = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1400, 300)

    color = custom(mat, SILT_COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["WP", "Dark", "Light"], "SiltColor", -500, -300)
    link(wp, "", color, "WP")
    # Wet silt: grey-brown, saturation under 25%, linear albedo 0.09 to 0.16 (wet soil 0.08 to 0.12, wet sand about 0.23).
    # The first try at 0.05 to 0.09 was too dark to carry the lamp: the beam has to land on something it can light.
    link(vector(mat, "SiltDark", (0.095, 0.088, 0.076), -1000, -350), "", color, "Dark")
    link(vector(mat, "SiltLight", (0.165, 0.152, 0.13), -1000, -200), "", color, "Light")

    normal = custom(mat, SILT_NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["WP", "Dist", "Strength", "Grain"], "SiltNormal", -500, 0)
    link(wp, "", normal, "WP")
    link(dist, "", normal, "Dist")
    # 0.7: at 1.0 the ripples read as a regular pattern under the beam (first frames).
    link(scalar(mat, "RippleStrength", 0.7, -1000, 50), "", normal, "Strength")
    link(scalar(mat, "GrainStrength", 0.12, -1000, 130), "", normal, "Grain")

    rough = custom(mat, SILT_ROUGH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["WP", "Base", "Var"], "SiltRoughness", -500, 250)
    link(wp, "", rough, "WP")
    link(scalar(mat, "Roughness", 0.9, -1000, 250), "", rough, "Base")
    link(scalar(mat, "RoughnessVariation", 0.05, -1000, 330), "", rough, "Var")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Specular", 0.35, -500, 400), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(mat)


def build_shafts():
    mat = fresh("M_SurfaceShafts")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1200, 150)
    shafts = custom(mat, SHAFT_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["UV", "Time", "Speed", "Floor"], "SurfaceShafts", -500, 0)
    link(uv, "", shafts, "UV")
    link(time, "", shafts, "Time")
    link(scalar(mat, "Speed", 0.12, -1000, 250), "", shafts, "Speed")
    link(scalar(mat, "Floor", 0.15, -1000, 330), "", shafts, "Floor")
    mel.connect_material_property(shafts, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


build_silt()
build_shafts()
