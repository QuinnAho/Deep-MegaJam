# Builds the dressing lane's one material from text. Run headless (no editor window):
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering -ModelContextProtocolPort=8012
#
#   M_DressSilt : skin for pack rock and ruin on the sea floor (ADeepDressing). The loader makes one dynamic instance per
#                 mesh slot and copies that slot's own textures in (BaseColorMap _BC, NormalMap _N, ORMMap _ORM), so the
#                 Dragon Cave pack is never edited. On top of the pack's surface it does three things:
#     1. Grade (style guide 4): saturation x 0.65, value x 0.7, nothing brighter than 0.7 linear. Lava-warm stone turns
#        grey-brown and the depth light supplies the colour.
#     2. Wet rock (style guide 4): roughness pulled into 0.35..0.6 from the pack's own roughness, so the lamp catches
#        wet faces and the grime variation in the pack's roughness breaks the highlight up.
#     3. Silt on the upward faces. Recipe from Epic's WorldAlignedBlend material function
#        (Engine/Content/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedBlend; docs: "World Aligned Blend",
#        Material Functions reference) as used for snow and moss on rock: mask = saturate((dot(N, up) + bias) * sharpness).
#        N blends the vertex normal with the normal map turned to world space (the pack's surface detail breaks the edge
#        up instead of a clean line), plus a slow world-space wobble and a fine grain, so it reads as settled sediment,
#        not paint. Under the mask the surface becomes the bed's silt (M_Silt's albedo 0.095..0.165, roughness 0.9) and
#        the normal flattens 80%: settled silt hides the rock's small detail but keeps its big shape.
#        WorldAlignedBlend itself is not wired in: it reads PixelNormalWS by default, which a material that writes Normal
#        cannot read (a loop), so its maths is written out in a Custom node with the vertex and texture normals.
import unreal

PATH = "/Game/Deep/Dressing"
mel = unreal.MaterialEditingLibrary

MASK_HLSL = r"""
float3 nt = normalize(NWS);
float up = lerp(VN.z, nt.z, 0.6);
float2 p = WP.xy * 0.01;
float wob = 0.5 * sin(1.7 * p.x + 1.3 * sin(0.9 * p.y)) + 0.5 * sin(2.3 * p.y - 1.1 * p.x + 0.7);
return saturate((up - (1.0 - Amount) + 0.12 * wob) * Sharpness);
"""

COLOR_HLSL = r"""
float l = dot(BC.rgb, float3(0.2126, 0.7152, 0.0722));
float3 rock = min(lerp(l.xxx, BC.rgb, Saturation) * Value, 0.7);
float2 p = WP.xy * 0.01;
float m = saturate(0.5 + 0.5 * sin(0.37 * p.x + 1.7 * sin(0.21 * p.y + 0.4)));
float3 silt = lerp(SiltDark.rgb, SiltLight.rgb, m);
return lerp(rock, silt, Mask);
"""

ROUGH_HLSL = r"""
float rock = lerp(WetMin, WetMax, saturate(ORM.g));
return lerp(rock, 0.9, Mask);
"""

NORMAL_HLSL = r"""
return normalize(lerp(N.rgb, float3(0.0, 0.0, 1.0), Mask * 0.8));
"""

METAL_HLSL = r"""
return ORM.b * (1.0 - Mask);
"""


def fresh(name):
    full = PATH + "/" + name
    mat = unreal.load_asset(full)
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def scalar(mat, name, value, x, y, group="Dressing"):
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
        unreal.log_error("DEEPDRESSMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def describe(path):
    t = unreal.load_asset(path)
    if not t:
        unreal.log_warning("DEEPDRESSMAT no texture %s" % path)
        return None
    cs = t.get_editor_property("compression_settings")
    srgb = t.get_editor_property("srgb")
    vt = t.get_editor_property("virtual_texture_streaming")
    unreal.log("DEEPDRESSMAT texture %s compression=%s srgb=%s vt=%s" % (path, cs, srgb, vt))
    return (cs, srgb, vt)


def sampler_for(info):
    cs, srgb, vt = info
    S = unreal.MaterialSamplerType
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return S.SAMPLERTYPE_VIRTUAL_NORMAL if vt else S.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS:
        return S.SAMPLERTYPE_VIRTUAL_MASKS if vt else S.SAMPLERTYPE_MASKS
    if not srgb:
        return S.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR if vt else S.SAMPLERTYPE_LINEAR_COLOR
    return S.SAMPLERTYPE_VIRTUAL_COLOR if vt else S.SAMPLERTYPE_COLOR


# Engine textures with each sampler type's settings, for the parameters' defaults (no pack content in the lane asset).
USAGE_FLAGS = ("used_with_instanced_static_meshes", "used_with_nanite")


DEFAULTS_VT = {
    "SAMPLERTYPE_COLOR": "/Engine/EngineMaterials/BaseFlattenDiffuseMap_VT",
    "SAMPLERTYPE_NORMAL": "/Engine/EngineMaterials/BaseFlattenNormalMap_VT",
    "SAMPLERTYPE_LINEAR_COLOR": "/Engine/EngineMaterials/BaseFlattenLinearColor_VT",
}

DEFAULTS = {
    "SAMPLERTYPE_COLOR": "/Engine/EngineMaterials/DefaultDiffuse",
    "SAMPLERTYPE_NORMAL": "/Engine/EngineMaterials/DefaultNormal",
    "SAMPLERTYPE_MASKS": "/Engine/EngineMaterials/DefaultDiffuse_TC_Masks",
    "SAMPLERTYPE_LINEAR_COLOR": "/Engine/EngineMaterials/BaseFlattenLinearColor",
}


def texture_param(mat, name, sampler, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("group", "Pack")
    import re
    m = re.search(r"SAMPLERTYPE_[A-Z_]+", str(sampler).upper())
    key = m.group(0).replace("VIRTUAL_", "") if m else ""
    table = DEFAULTS_VT if "VIRTUAL" in str(sampler).upper() else DEFAULTS
    tex = unreal.load_asset(table[key]) if key in table else None
    if tex:
        e.set_editor_property("texture", tex)
    e.set_editor_property("sampler_type", sampler)
    unreal.log("DEEPDRESSMAT %s sampler %s default %s" % (name, sampler, e.get_editor_property("texture").get_path_name() if e.get_editor_property("texture") else None))
    return e


def build(name, vt):
    bc_info = describe("/Game/DragonCave/Textures/T_LargeRock01_BC")
    n_info = describe("/Game/DragonCave/Textures/T_LargeRock01_N")
    orm_info = describe("/Game/DragonCave/Textures/T_LargeRock01_ORM")
    for t in ("T_CaveRocks01_BC", "T_CaveRocks01_ORM", "T_BridgeSet_BC", "T_BridgeSet_ORM", "T_BridgeSet_N"):
        describe("/Game/DragonCave/Textures/" + t)
    S = unreal.MaterialSamplerType
    bc_s = sampler_for((bc_info[0], bc_info[1], vt))
    n_s = sampler_for((n_info[0], n_info[1], vt))
    orm_s = sampler_for((orm_info[0], orm_info[1], vt))

    mat = fresh(name)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # Usage flags a cooked build cannot set on the fly (it falls back to the default material): instanced meshes (the
    # loader's HISMs), Nanite (most pack meshes are Nanite; the first package showed every one in the default material),
    # and plain static meshes (the relic).
    for flag in USAGE_FLAGS:
        mat.set_editor_property(flag, True)

    bc = texture_param(mat, "BaseColorMap", bc_s, -1600, -400)
    n = texture_param(mat, "NormalMap", n_s, -1600, 0)
    orm = texture_param(mat, "ORMMap", orm_s, -1600, 400)

    # Object (instance) space, not world: in world space the wobble swam over pieces that move with the arena root, and a
    # 4 cm hash grain made a fine checker on the trench rocks while the sub drove (trench frames, pass one).
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1600, 800)
    vn = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1600, 950)
    tw = mel.create_material_expression(mat, unreal.MaterialExpressionTransform, -1200, 100)
    tw.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT)
    tw.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(n, "RGB", tw, "")

    mask = custom(mat, MASK_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["VN", "NWS", "WP", "Amount", "Sharpness"], "SiltMask", -800, 700)
    link(vn, "", mask, "VN")
    link(tw, "", mask, "NWS")
    link(wp, "", mask, "WP")
    # Amount 0.45: faces within about 55 degrees of up start to gather silt; sharpness 6 gives a soft 10-degree edge.
    link(scalar(mat, "SiltAmount", 0.45, -1200, 700, "Silt"), "", mask, "Amount")
    link(scalar(mat, "SiltSharpness", 6.0, -1200, 780, "Silt"), "", mask, "Sharpness")

    color = custom(mat, COLOR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["BC", "Saturation", "Value", "WP", "SiltDark", "SiltLight", "Mask"], "DressColor", -300, -400)
    link(bc, "RGB", color, "BC")
    link(scalar(mat, "Saturation", 0.65, -800, -300, "Grade"), "", color, "Saturation")
    link(scalar(mat, "Value", 0.7, -800, -220, "Grade"), "", color, "Value")
    link(wp, "", color, "WP")
    link(vector(mat, "SiltDark", (0.095, 0.088, 0.076), -800, -140), "", color, "SiltDark")
    link(vector(mat, "SiltLight", (0.165, 0.152, 0.13), -800, -60), "", color, "SiltLight")
    link(mask, "", color, "Mask")

    rough = custom(mat, ROUGH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["ORM", "WetMin", "WetMax", "Mask"], "DressRoughness", -300, 300)
    link(orm, "RGB", rough, "ORM")
    link(scalar(mat, "WetMin", 0.35, -800, 300, "Wet"), "", rough, "WetMin")
    link(scalar(mat, "WetMax", 0.6, -800, 380, "Wet"), "", rough, "WetMax")
    link(mask, "", rough, "Mask")

    normal = custom(mat, NORMAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["N", "Mask"], "DressNormal", -300, 0)
    link(n, "RGB", normal, "N")
    link(mask, "", normal, "Mask")

    metal = custom(mat, METAL_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["ORM", "Mask"], "DressMetal", -300, 500)
    link(orm, "RGB", metal, "ORM")
    link(mask, "", metal, "Mask")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DEEPDRESSMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DEEPDRESSMAT compile %s: %s" % (name, str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DEEPDRESSMAT %s saved=%s expressions=%d errors=%d" % (name, ok, mel.get_num_material_expressions(mat), len(errors or [])))
    # Read the flags back from the saved asset, as proof.
    saved = unreal.EditorAssetLibrary.load_asset(PATH + "/" + name)
    unreal.log("DEEPDRESSMAT %s usage %s" % (name, ", ".join("%s=%s" % (f, saved.get_editor_property(f)) for f in USAGE_FLAGS)))


# Two masters, the same graph: the pack streams some textures virtually (the bridge and ruin set), and a virtual texture
# needs a virtual sampler. DeepDressing::Skin picks the VT one for a slot whose base colour is virtual.
build("M_DressSilt", False)
build("M_DressSiltVT", True)
