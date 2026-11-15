# Builds the cave lane's one material from text. Run headless (no editor window):
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering
#
#   M_CaveRock : skin for the generated cave wall (a procedural mesh with no usable UVs) and for instanced rock. Everything
#                is sampled in world space, so no UVs are read. Pack rock textures are Dragon Cave's T_CaveRocks01 and 02.
#     1. Triplanar (world-aligned) sampling, one Custom node per output (colour, normal, ORM). Recipe: Epic's WorldAlignedTexture /
#        WorldAlignedNormal material functions (Engine/Content/Functions/Engine_MaterialFunctions01/Texturing; Material Functions
#        reference) and Ben Golus, "Normal Mapping for a Triplanar Shader". Weights = pow(abs(vertex normal), 4) normalised;
#        OBJECT-SPACE projection (the cave moves past the sub; world space would swim): LocalPosition and the vertex normal
#        transformed world->local; the blended normal is transformed local->world for output. WP/VN below mean local values.
#        planes: X uses WP.yz, Y uses WP.xz, Z uses WP.xy. Written in a Custom node because the engine functions read
#        PixelNormalWS, which a material that writes Normal cannot read (a loop), so the vertex normal is used.
#     2. Two scales (TileSmallM, TileLargeM) blended by a slow world mask to break the repeat, then texture set 01 and set 02
#        blended by another world mask.
#     3. Normals: UDN-style blend. Each plane's tangent xy offsets the vertex normal along that plane's world axes
#        (U, V = the world axes the plane maps to), weighted by the triplanar weights; the result is a WORLD-space normal,
#        so the material's tangent_space_normal is OFF (same as M_Silt in the look lane).
#     4. Silt on up faces (vertex normal z plus a slow wobble; albedo 0.095..0.165, roughness 0.9) and the normal flattened
#        80% under it. Same recipe as the dressing lane's M_DressSilt.
#     5. Grade (style guide 4): chroma x Saturation then hard cap at HSV saturation 0.35, value x Value (0.35) then at most 0.25 linear (the silt bed is 0.09..0.16);
#        wet-rock roughness remapped into RoughMin..RoughMax (0.35..0.55) from the ORM green channel; AO from ORM red.
import unreal

PATH = "/Game/Deep/Cave"
TEX = "/Game/DragonCave/Textures/"
mel = unreal.MaterialEditingLibrary
errors_seen = []


# ---- HLSL generators (straight-line code: a Custom node body cannot define functions) --------------------------------------

def smp(name, uv):
    return "Texture2DSample(%s, %sSampler, %s)" % (name, name, uv)


def tri(name, scale, sw):
    """Triplanar sample of one texture at one tile size, weights w (declared in PRELUDE)."""
    return "(w.x * %s.%s + w.y * %s.%s + w.z * %s.%s)" % (
        smp(name, "WP.yz / " + scale), sw, smp(name, "WP.xz / " + scale), sw, smp(name, "WP.xy / " + scale), sw)


PRELUDE = r"""
float3 Nv = normalize(VN);
float3 w = pow(abs(Nv), 4.0);
w = w / (w.x + w.y + w.z + 0.00001);
float sS = max(TileSmallM, 0.1) * 100.0;
float sL = max(TileLargeM, 0.1) * 100.0;
float a1 = sin(WP.x * 0.0021 + 1.3 * sin(WP.y * 0.0017)) + sin(WP.y * 0.0019 - WP.z * 0.0023 + 0.7);
float mScale = saturate(0.5 + 0.35 * a1);
float a2 = sin(WP.y * 0.0013 + 1.1 * sin(WP.z * 0.0011 + 0.4)) + sin(WP.z * 0.0016 + WP.x * 0.0012 + 2.1);
float mSet = smoothstep(0.3, 0.7, saturate(0.5 + 0.35 * a2));
"""

MASK_HLSL = r"""
float3 nt = normalize(VN);
float2 p = WP.xy * 0.01;
float wob = 0.5 * sin(1.7 * p.x + 1.3 * sin(0.9 * p.y)) + 0.5 * sin(2.3 * p.y - 1.1 * p.x + 0.7);
return saturate((nt.z - (1.0 - Amount) + 0.12 * wob) * Sharpness);
"""

COLOR_HLSL = PRELUDE + """
float3 cA = lerp(%s, %s, mScale);
float3 cB = lerp(%s, %s, mScale);
float3 c = lerp(cA, cB, mSet);
float l = dot(c, float3(0.2126, 0.7152, 0.0722));
float3 rock = lerp(l.xxx, c, Saturation) * Value;
float mx = max(rock.r, max(rock.g, rock.b));
float mn = min(rock.r, min(rock.g, rock.b));
float s = (mx - mn) / max(mx, 0.0001);
float k = (s > 0.35) ? (0.35 / s) : 1.0;
rock = mx - (mx - rock) * k;
rock = min(rock, 0.25);
float2 p = WP.xy * 0.01;
float m = saturate(0.5 + 0.5 * sin(0.37 * p.x + 1.7 * sin(0.21 * p.y + 0.4)));
float3 silt = lerp(SiltDark.rgb, SiltLight.rgb, m);
return lerp(rock, silt, Mask);
""" % (tri("B1", "sS", "rgb"), tri("B1", "sL", "rgb"), tri("B2", "sS", "rgb"), tri("B2", "sL", "rgb"))


def normal_off(name, scale):
    # Three tangent samples (one per plane); tangent x,y become offsets along that plane's world axes.
    return ("""
float2 tx_{n}{s} = {sx}.xy * 2.0 - 1.0;
float2 ty_{n}{s} = {sy}.xy * 2.0 - 1.0;
float2 tz_{n}{s} = {sz}.xy * 2.0 - 1.0;
float3 off_{n}{s} = w.x * float3(0.0, tx_{n}{s}.x, tx_{n}{s}.y) + w.y * float3(ty_{n}{s}.x, 0.0, ty_{n}{s}.y) + w.z * float3(tz_{n}{s}.x, tz_{n}{s}.y, 0.0);
""").format(n=name, s=scale, sx=smp(name, "WP.yz / " + scale), sy=smp(name, "WP.xz / " + scale), sz=smp(name, "WP.xy / " + scale))


NORMAL_HLSL = PRELUDE + normal_off("N1", "sS") + normal_off("N1", "sL") + normal_off("N2", "sS") + normal_off("N2", "sL") + r"""
float3 offA = lerp(off_N1sS, off_N1sL, mScale);
float3 offB = lerp(off_N2sS, off_N2sL, mScale);
float3 off = lerp(offA, offB, mSet) * Strength;
float3 nrock = normalize(Nv + off);
return normalize(lerp(nrock, Nv, Mask * 0.8));
"""

ORM_HLSL = PRELUDE + """
float3 oA = lerp(%s, %s, mScale);
float3 oB = lerp(%s, %s, mScale);
float3 o = lerp(oA, oB, mSet);
float rock = lerp(RoughMin, RoughMax, saturate(o.g));
return float2(o.r, lerp(rock, 0.9, Mask));
""" % (tri("O1", "sS", "rgb"), tri("O1", "sL", "rgb"), tri("O2", "sS", "rgb"), tri("O2", "sL", "rgb"))

PICK_X = "return V.x;"
PICK_Y = "return V.y;"


# ---- helpers (same API usage as build_dressing_materials.py) --------------------------------------------------------------

def fresh(name):
    full = PATH + "/" + name
    mat = unreal.load_asset(full)
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def scalar(mat, name, value, x, y, group="Cave"):
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
        msg = "CAVEMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp)
        errors_seen.append(msg)
        unreal.log_error(msg)


def sampler_for(path):
    t = unreal.load_asset(path)
    if not t:
        msg = "CAVEMAT no texture %s" % path
        errors_seen.append(msg)
        unreal.log_error(msg)
        return None, None
    cs = t.get_editor_property("compression_settings")
    srgb = t.get_editor_property("srgb")
    vt = t.get_editor_property("virtual_texture_streaming")
    unreal.log("CAVEMAT texture %s compression=%s srgb=%s vt=%s" % (path, cs, srgb, vt))
    if vt:
        unreal.log_warning("CAVEMAT %s streams virtually; a Custom-node Texture2DSample cannot read a virtual texture" % path)
    S = unreal.MaterialSamplerType
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return t, S.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS:
        return t, S.SAMPLERTYPE_MASKS
    if not srgb:
        return t, S.SAMPLERTYPE_LINEAR_COLOR
    return t, S.SAMPLERTYPE_COLOR


def tex_object(mat, path, x, y):
    t, sampler = sampler_for(path)
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObject, x, y)
    if t:
        e.set_editor_property("texture", t)
        e.set_editor_property("sampler_type", sampler)
    return e


def build():
    name = "M_CaveRock"
    mat = fresh(name)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", False)
    # Normal is built in world space (UDN triplanar blend), so the material must not treat it as tangent space.
    mat.set_editor_property("tangent_space_normal", False)
    # Flags a cooked build cannot set on the fly: instanced meshes and Nanite (as in M_DressSilt). The procedural wall
    # uses the local vertex factory and needs no flag.
    for flag in ("used_with_instanced_static_meshes", "used_with_nanite"):
        mat.set_editor_property(flag, True)

    # OBJECT (local) space, not world: the cave moves past the fixed sub every frame, so world-aligned projection would swim.
    # Position = LocalPosition (as in M_DressSilt); normal = vertex normal turned world -> local with a Transform node.
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1800, 800)
    vnw = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -2100, 950)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, -1800, 950)
    vn.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    vn.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(vnw, "", vn, "")
    tile_s = scalar(mat, "TileSmallM", 4.0, -1800, 1100, "Tiling")
    tile_l = scalar(mat, "TileLargeM", 13.0, -1800, 1180, "Tiling")

    mask = custom(mat, MASK_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["VN", "WP", "Amount", "Sharpness"], "SiltMask", -900, 900)
    link(vn, "", mask, "VN")
    link(wp, "", mask, "WP")
    link(scalar(mat, "SiltAmount", 0.45, -1300, 900, "Silt"), "", mask, "Amount")
    link(scalar(mat, "SiltSharpness", 6.0, -1300, 980, "Silt"), "", mask, "Sharpness")

    def common(node):
        link(wp, "", node, "WP")
        link(vn, "", node, "VN")
        link(tile_s, "", node, "TileSmallM")
        link(tile_l, "", node, "TileLargeM")

    color = custom(mat, COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ["B1", "B2", "WP", "VN", "TileSmallM", "TileLargeM", "Saturation", "Value", "SiltDark", "SiltLight", "Mask"],
                   "CaveColor", -300, -400)
    link(tex_object(mat, TEX + "T_CaveRocks01_BC", -1300, -600), "", color, "B1")
    link(tex_object(mat, TEX + "T_CaveRocks02_BC", -1300, -500), "", color, "B2")
    common(color)
    link(scalar(mat, "Saturation", 0.5, -1300, -300, "Grade"), "", color, "Saturation")
    link(scalar(mat, "Value", 0.35, -1300, -220, "Grade"), "", color, "Value")
    link(vector(mat, "SiltDark", (0.095, 0.088, 0.076), -1300, -140), "", color, "SiltDark")
    link(vector(mat, "SiltLight", (0.165, 0.152, 0.13), -1300, -60), "", color, "SiltLight")
    link(mask, "", color, "Mask")

    normal = custom(mat, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    ["N1", "N2", "WP", "VN", "TileSmallM", "TileLargeM", "Strength", "Mask"], "CaveNormal", -300, 0)
    link(tex_object(mat, TEX + "T_CaveRocks01_N", -1300, 100), "", normal, "N1")
    link(tex_object(mat, TEX + "T_CaveRocks02_N", -1300, 200), "", normal, "N2")
    common(normal)
    link(scalar(mat, "NormalStrength", 1.0, -1300, 300, "Grade"), "", normal, "Strength")
    link(mask, "", normal, "Mask")

    orm = custom(mat, ORM_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                 ["O1", "O2", "WP", "VN", "TileSmallM", "TileLargeM", "RoughMin", "RoughMax", "Mask"], "CaveORM", -300, 400)
    link(tex_object(mat, TEX + "T_CaveRocks01_ORM", -1300, 500), "", orm, "O1")
    link(tex_object(mat, TEX + "T_CaveRocks02_ORM", -1300, 600), "", orm, "O2")
    common(orm)
    link(scalar(mat, "RoughMin", 0.35, -1300, 700, "Wet"), "", orm, "RoughMin")
    link(scalar(mat, "RoughMax", 0.55, -1300, 780, "Wet"), "", orm, "RoughMax")
    link(mask, "", orm, "Mask")

    ao = custom(mat, PICK_X, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["V"], "CaveAO", 100, 350)
    link(orm, "", ao, "V")
    rough = custom(mat, PICK_Y, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["V"], "CaveRoughness", 100, 450)
    link(orm, "", rough, "V")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # The blended normal is local-space; back to world for the output (tangent_space_normal is off).
    nworld = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, 100, 0)
    nworld.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    nworld.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(normal, "", nworld, "")
    mel.connect_material_property(nworld, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("CAVEMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        errors_seen.append(str(e))
        unreal.log_error("CAVEMAT compile %s: %s" % (name, str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    saved = unreal.EditorAssetLibrary.load_asset(PATH + "/" + name)
    flags = ["tangent_space_normal", "two_sided", "used_with_instanced_static_meshes", "used_with_nanite"]
    unreal.log("CAVEMAT readback " + " ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in flags))
    if ok and not errors_seen:
        unreal.log("CAVEMAT ok %s saved=%s expressions=%d" % (name, ok, mel.get_num_material_expressions(mat)))
    else:
        unreal.log_error("CAVEMAT FAILED %s saved=%s problems=%d: %s" % (name, ok, len(errors_seen), "; ".join(errors_seen[:5])))


build()
