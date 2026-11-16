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

# Nov 15 ascent pass. Two faults seen at 1 m in the lamp at the bottom of the well (S10-relic-grab): the grain was a
# hash of floor(p * 40) and floor(p * 60), so it was made of square cells 2.5 and 1.7 cm wide that read as blocks up
# close; and every term used WP.xy only (a planar projection from above), so on the sinkhole's steep walls the pattern
# stretched into vertical streaks. Now: grain is smooth value noise in 3D (trilinear with a smoothstep, a sine-free
# hash, so no cells and no stretch on any slope), in octaves from 25 cm down to 7 mm so it still has detail at 0.5 m;
# the ripples are projected three ways and blended by the surface normal (triplanar, as WorldAlignedTexture does; walls
# get a third of the ripple); and the normal is built in world space (tangent_space_normal off), so it is right on
# walls without relying on the procedural mesh's tangents. Detail fades with distance so it does not shimmer far off.
# Sources in the dev log (Nov 15, ascent pass).
SILT_NOISE_HLSL = r"""
struct FSiltNoise
{
    float Hash(float3 c)
    {
        c = c - 1024.0 * floor(c / 1024.0);
        float3 p3 = frac(c * 0.1031);
        p3 += dot(p3, p3.zyx + 31.32);
        return frac((p3.x + p3.y) * p3.z);
    }
    float Value(float3 p)
    {
        float3 i = floor(p);
        float3 f = p - i;
        float3 u = f * f * (3.0 - 2.0 * f);
        float a = lerp(Hash(i), Hash(i + float3(1, 0, 0)), u.x);
        float b = lerp(Hash(i + float3(0, 1, 0)), Hash(i + float3(1, 1, 0)), u.x);
        float c = lerp(Hash(i + float3(0, 0, 1)), Hash(i + float3(1, 0, 1)), u.x);
        float d = lerp(Hash(i + float3(0, 1, 1)), Hash(i + float3(1, 1, 1)), u.x);
        return lerp(lerp(a, b, u.y), lerp(c, d, u.y), u.z);
    }
    // Grain height in metres: bumps of a few millimetres at 4.5 cm, 1.6 cm and 7 mm.
    float Grain(float3 p, float Near)
    {
        return 0.0040 * Value(p * 22.0) + 0.0016 * Value(p * 62.0 + 11.0) + 0.0007 * Near * Value(p * 140.0 + 29.0);
    }
    // Sand ripples ~12 cm apart whose direction bends slowly, and a softer cross set: slope (d height / d metre).
    float2 Ripple(float2 p)
    {
        float bend = 0.6 * sin(0.11 * p.x + 0.07 * p.y) + 0.3 * sin(0.23 * p.y);
        float2 d1 = float2(cos(0.5 + bend), sin(0.5 + bend));
        float2 d2 = float2(cos(2.2 + 0.5 * bend), sin(2.2 + 0.5 * bend));
        float k1 = 6.2831853 / 0.12;
        float k2 = 6.2831853 / 0.31;
        float a1 = k1 * dot(p, d1) + 1.5 * sin(0.8 * p.y) + 2.5 * sin(0.37 * p.x - 0.29 * p.y) + 0.9 * sin(1.9 * p.x + 1.3 * p.y);
        float a2 = k2 * dot(p, d2);
        float s1 = cos(a1) + 0.35 * cos(2.0 * a1);
        return d1 * (0.0035 * k1 * s1) + d2 * (0.002 * k2 * cos(a2));
    }
};
"""

# World position in cm -> metres. Returns float3 base colour.
SILT_COLOR_HLSL = SILT_NOISE_HLSL + r"""
FSiltNoise S;
float3 p = WP * 0.01;
float n = 0.0;
n += 0.50 * sin(0.37 * p.x + 0.23 * p.z + 1.7 * sin(0.21 * p.y + 0.4));
n += 0.30 * sin(0.93 * p.y - 0.61 * p.x + 0.41 * p.z + 2.0 * sin(0.47 * p.x));
n += 0.20 * sin(2.30 * p.x + 1.90 * p.y + 1.30 * p.z + 3.0 * sin(0.9 * p.y));
float m = saturate(0.5 + 0.5 * n);
float near = saturate(1.0 - Dist / 2500.0);
// Blotches of darker and lighter sediment at 25 cm and 8 cm, and fine grain: all smooth, no cells.
float blot = S.Value(p * 4.0 + 5.0) - 0.5;
float spot = S.Value(p * 12.0 + 3.0) - 0.5;
float grain = 0.6 * S.Value(p * 30.0 + 7.0) + 0.4 * S.Value(p * 85.0 + 13.0) - 0.5;
float3 c = lerp(Dark.rgb, Light.rgb, saturate(m * 0.8 + 0.1 + 0.45 * blot));
return c * (1.0 + 0.16 * spot + near * 0.22 * grain);
"""

# World-space normal (the material's tangent_space_normal is off): the vertex normal bent by the gradient of the
# grain height and by triplanar ripples, the gradient taken along the surface only.
SILT_NORMAL_HLSL = SILT_NOISE_HLSL + r"""
FSiltNoise S;
float3 p = WP * 0.01;
float3 nrm = normalize(VN);
float3 w = pow(abs(nrm), 4.0);
w /= (w.x + w.y + w.z + 1e-5);
float far = saturate(1.0 - Dist / 4000.0);
float near = saturate(1.0 - Dist / 1500.0);
// Ripples: settled silt on the flat, a third of it on the walls.
float2 rz = S.Ripple(p.xy);
float2 rx = S.Ripple(p.yz + 31.0);
float2 ry = S.Ripple(p.xz + 57.0);
float3 g = (float3(rz.x, rz.y, 0.0) * w.z + (float3(0.0, rx.x, rx.y) * w.x + float3(ry.x, 0.0, ry.y) * w.y) * 0.35) * Strength * far;
// Grain: central differences of the 3D height, 3 mm apart.
float e = 0.003;
float3 gg;
gg.x = S.Grain(p + float3(e, 0, 0), near) - S.Grain(p - float3(e, 0, 0), near);
gg.y = S.Grain(p + float3(0, e, 0), near) - S.Grain(p - float3(0, e, 0), near);
gg.z = S.Grain(p + float3(0, 0, e), near) - S.Grain(p - float3(0, 0, e), near);
g += gg / (2.0 * e) * Grain * saturate(1.0 - Dist / 3000.0);
g -= nrm * dot(g, nrm);
return normalize(nrm - g);
"""

SILT_ROUGH_HLSL = r"""
float3 p = WP * 0.01;
float n = 0.5 * sin(0.53 * p.x + 0.3 * p.z + 1.1 * sin(0.31 * p.y)) + 0.5 * sin(0.77 * p.y - 0.4 * p.x + 0.35 * p.z);
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

    color = custom(mat, SILT_COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["WP", "Dark", "Light", "Dist"], "SiltColor", -500, -300)
    link(wp, "", color, "WP")
    # Wet silt: grey-brown, saturation under 25%, linear albedo 0.09 to 0.16 (wet soil 0.08 to 0.12, wet sand about 0.23).
    # The first try at 0.05 to 0.09 was too dark to carry the lamp: the beam has to land on something it can light.
    link(vector(mat, "SiltDark", (0.095, 0.088, 0.076), -1000, -350), "", color, "Dark")
    link(vector(mat, "SiltLight", (0.165, 0.152, 0.13), -1000, -200), "", color, "Light")

    link(dist, "", color, "Dist")

    # The normal is worked out in world space (see SILT_NORMAL_HLSL), so the material must not treat it as tangent space.
    mat.set_editor_property("tangent_space_normal", False)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 150)
    normal = custom(mat, SILT_NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["WP", "VN", "Dist", "Strength", "Grain"], "SiltNormal", -500, 0)
    link(wp, "", normal, "WP")
    link(vn, "", normal, "VN")
    link(dist, "", normal, "Dist")
    # 0.7: at 1.0 the ripples read as a regular pattern under the beam (first frames).
    link(scalar(mat, "RippleStrength", 0.7, -1000, 50), "", normal, "Strength")
    # A multiplier on the grain's height (a few millimetres): slopes of about 0.25 at 1.0.
    link(scalar(mat, "GrainStrength", 1.0, -1000, 130), "", normal, "Grain")

    rough = custom(mat, SILT_ROUGH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["WP", "Base", "Var"], "SiltRoughness", -500, 250)
    link(wp, "", rough, "WP")
    link(scalar(mat, "Roughness", 0.9, -1000, 250), "", rough, "Base")
    link(scalar(mat, "RoughnessVariation", 0.05, -1000, 330), "", rough, "Var")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Specular", 0.35, -500, 400), "", unreal.MaterialProperty.MP_SPECULAR)
    # Only the procedural bed wears it (local vertex factory: no usage flag). Read back from the saved asset.
    finish(mat)
    saved = unreal.load_asset(PATH + "/M_Silt")
    flags = ["tangent_space_normal", "two_sided", "used_with_instanced_static_meshes", "used_with_nanite",
             "used_with_skeletal_mesh", "used_with_static_lighting"]
    unreal.log("DEEPLOOKMAT M_Silt readback " + " ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in flags))


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
