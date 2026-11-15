# Builds M_DragonHead, the material that turns the Dragon Cave dragon (a whole winged animal, read-only pack) into
# the sea serpent's head and neck. Run headless like build_serpent_skin.py:
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -ModelContextProtocolPort=8011
# Recipe (devlog, Nov 15 "dragon head"; sources there):
#   Clip  : Masked blend mode. Opacity mask from the mesh's LOCAL position: keep a pixel only in front of a plane
#           through the neck (dot(P - CutOrigin, CutNormal) > 0) and inside a capsule round the neck and head
#           (CutOrigin -> KeepEnd, KeepRadius), so the body, wings and legs are never drawn. Nanite supports Masked
#           (Epic, Nanite docs). No VertexInterpolator: a known 5.8 preview bug makes Nanite draw it fully opaque.
#           ADeepSerpent sets the clip per pose through a dynamic instance (mesh-local centimetres).
#   Skin  : the serpent body's look, not the pack's: the dragon's normal map for its scales, its colour taken to
#           grey and darkened to the body's near-black, no emissive, clear coat for the wet film (Clear Coat 0.8,
#           roughness 0.18, as M_SerpentSkin), base roughness from the ORM green channel squeezed into the wet range.
#           Wet = darker albedo, lower roughness, a thin coat (Epic shading models doc; community wet-surface recipes).
# The pack textures are virtual textures (r.VirtualTextures is on): the samplers must be the Virtual types.
import unreal

PATH = "/Game/Deep/Serpent"
NAME = "M_DragonHead"
TEX = "/Game/DragonCave/Textures/"
mel = unreal.MaterialEditingLibrary

MASK_HLSL = r"""
float3 d = LP - CutOrigin.xyz;
float side = dot(d, normalize(CutNormal.xyz));
float3 ax = KeepEnd.xyz - CutOrigin.xyz;
float len = max(length(ax), 1.0);
float3 u = ax / len;
float t = clamp(dot(d, u), 0.0, len);
float r = length(d - u * t);
return (side > 0.0 && r < KeepRadius) ? 1.0 : 0.0;
"""

GRADE_HLSL = r"""
float l = dot(BC.rgb, float3(0.2126, 0.7152, 0.0722));
float3 c = lerp(float3(l, l, l), BC.rgb, Saturation) * Gain;
return lerp(c, Tint.rgb * l * Gain, TintAmount);
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
        unreal.log_error("DRAGONHEAD could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def sample(mat, name, sampler, x, y):
    tex = unreal.load_asset(TEX + name)
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, x, y)
    if tex:
        e.set_editor_property("texture", tex)
    else:
        unreal.log_warning("DRAGONHEAD missing texture " + name + " (pack not installed?)")
    virtual = bool(tex and tex.get_editor_property("virtual_texture_streaming"))
    e.set_editor_property("sampler_type", getattr(unreal.MaterialSamplerType, ("SAMPLERTYPE_VIRTUAL_" if virtual else "SAMPLERTYPE_") + sampler))
    return e


def local_position(mat, x, y):
    cls = getattr(unreal, "MaterialExpressionLocalPosition", None)
    if cls:
        return mel.create_material_expression(mat, cls, x, y)
    # Fallback: absolute world position transformed to local space.
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, x - 300, y)
    tp = mel.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, x, y)
    tp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    tp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    link(wp, "", tp, "")
    return tp


def skin(mat, uv, normal_fix=None):
    """The shared skin: the dragon's textures graded to the body's near-black, wet clear coat. uv = None reads UV0."""
    bc = sample(mat, "T_Dragon_BC", "COLOR", -1400, -400)
    nrm = sample(mat, "T_Dragon_N", "NORMAL", -1400, 0)
    orm = sample(mat, "T_Dragon_ORM", "COLOR", -1400, 400)
    nrm_out = normal_fix(nrm) if normal_fix else nrm
    if uv is not None:
        for t in (bc, nrm, orm):
            link(uv, "", t, "UVs")

    grade = custom(mat, GRADE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                   ["BC", "Saturation", "Gain", "Tint", "TintAmount"], "DragonGrade", -600, -400)
    link(bc, "RGB", grade, "BC")
    link(scalar(mat, "Saturation", 0.25, -1000, -300), "", grade, "Saturation")
    # Near-black like the body (M_SerpentSkin SkinColor 0.008..0.01 linear); tuned by frame next to the body.
    link(scalar(mat, "ColorGain", 0.06, -1000, -260), "", grade, "Gain")
    link(vector(mat, "Tint", (0.8, 1.0, 1.0), -1000, -220), "", grade, "Tint")
    link(scalar(mat, "TintAmount", 0.3, -1000, -180), "", grade, "TintAmount")
    mel.connect_material_property(grade, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Normal: the dragon's scales, a little flattened (wet skin; NormalStrength 1 = as authored).
    flat = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1000, 120)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    nl = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -600, 0)
    link(flat, "", nl, "A")
    link(nrm_out, "RGB" if nrm_out is nrm else "", nl, "B")
    # Arm's length (pass two): no derivatives (Nanite shades in compute; Epic's Normal Curvature to Roughness is
    # reported to break on Nanite), so the anti-sparkle is by distance: the normal flattens and the roughness and the
    # coat's roughness rise from NearCm to FarCm, where the plates are smaller than a pixel and alias.
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1300, 700)
    far = custom(mat, FAR_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["D", "NearCm", "FarCm"], "DragonFar", -1000, 700)
    link(depth, "", far, "D")
    link(scalar(mat, "NearCm", 200.0, -1300, 760), "", far, "NearCm")
    link(scalar(mat, "FarCm", 1500.0, -1300, 800), "", far, "FarCm")
    nstr = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -800, 60)
    link(scalar(mat, "NormalStrength", 0.8, -1000, 60), "", nstr, "A")
    link(scalar(mat, "NormalStrengthFar", 0.4, -1000, 100), "", nstr, "B")
    link(far, "", nstr, "Alpha")
    link(nstr, "", nl, "Alpha")
    mel.connect_material_property(nl, "", unreal.MaterialProperty.MP_NORMAL)

    # Roughness: ORM green squeezed into the wet range (style guide: below 0.25 only on the serpent's wet skin).
    rl = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -600, 400)
    link(scalar(mat, "RoughnessWet", 0.3, -1000, 360), "", rl, "A")
    link(scalar(mat, "RoughnessDry", 0.55, -1000, 400), "", rl, "B")
    link(orm, "G", rl, "Alpha")
    floor = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -800, 460)
    link(scalar(mat, "RoughnessFloor", 0.36, -1000, 440), "", floor, "A")
    link(scalar(mat, "RoughnessFloorFar", 0.5, -1000, 480), "", floor, "B")
    link(far, "", floor, "Alpha")
    rmax = mel.create_material_expression(mat, unreal.MaterialExpressionMax, -450, 420)
    link(rl, "", rmax, "A")
    link(floor, "", rmax, "B")
    mel.connect_material_property(rmax, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(scalar(mat, "Specular", 0.3, -600, 560), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(scalar(mat, "WetCoat", 0.7, -600, 620), "", unreal.MaterialProperty.MP_CUSTOM_DATA0)
    coat = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -450, 680)
    link(scalar(mat, "WetCoatRoughness", 0.26, -800, 660), "", coat, "A")
    link(scalar(mat, "WetCoatRoughnessFar", 0.4, -800, 700), "", coat, "B")
    link(far, "", coat, "Alpha")
    mel.connect_material_property(coat, "", unreal.MaterialProperty.MP_CUSTOM_DATA1)


def finish(mat, name, usages):
    for usage in usages:
        try:
            mel.set_material_usage(mat, usage)
        except Exception as ex:
            unreal.log_warning("DRAGONHEAD usage %s: %s" % (usage, ex))
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DRAGONHEAD not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DRAGONHEAD compile: " + str(e))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DRAGONHEAD %s saved=%s expressions=%d errors=%d used_with_nanite=%s" % (
        name, ok, mel.get_num_material_expressions(mat), len(errors or []), mat.get_editor_property("used_with_nanite")))


def build():
    mat = fresh(NAME)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    skin(mat, None)

    # The clip (mesh-local cm). Defaults keep everything; the serpent sets them per pose.
    lp = local_position(mat, -1000, 800)
    mask = custom(mat, MASK_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                  ["LP", "CutOrigin", "CutNormal", "KeepEnd", "KeepRadius"], "DragonNeckClip", -600, 800)
    link(lp, "", mask, "LP")
    link(vector(mat, "CutOrigin", (0.0, -1e6, 0.0), -1000, 900), "", mask, "CutOrigin")
    link(vector(mat, "CutNormal", (0.0, 1.0, 0.0), -1000, 950), "", mask, "CutNormal")
    link(vector(mat, "KeepEnd", (0.0, 1e6, 0.0), -1000, 1000), "", mask, "KeepEnd")
    link(scalar(mat, "KeepRadius", 1e7, -1000, 1050), "", mask, "KeepRadius")
    mel.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat, NAME, [unreal.MaterialUsage.MATUSAGE_NANITE])


def build_body():
    """M_DragonBody: the same skin on the serpent's tube. A patch of the dragon's own neck skin (one UV island of its
    UDIM tile 1015) is mirror-tiled along and around the body, at the neck's real scale, so the scales at the join are
    the same scales. Mirroring (a triangle wave) keeps every edge continuous; the normal's x/y flip on mirrored tiles."""
    mat = fresh("M_DragonBody")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    tube_uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -2400, 0)
    tube_uv.set_editor_property("coordinate_index", 1)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -2400, 200)
    patch = custom(mat, PATCH_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                   ["UV", "Rad", "PatchOrigin", "PatchSize", "PatchMetres"], "DragonSkinPatch", -1900, 0)
    link(tube_uv, "", patch, "UV")
    link(vc, "R", patch, "Rad")
    # Measured on SM_DragonPose04's neck (UE UVs, tile 1015 is u 4..5, v 1..2): about 95.5 m of mesh per UV unit.
    link(vector(mat, "PatchOrigin", (4.56, 1.55, 0.0), -2300, 300), "", patch, "PatchOrigin")
    link(vector(mat, "PatchSize", (0.12, 0.20, 0.0), -2300, 350), "", patch, "PatchSize")
    metres = vector(mat, "PatchMetres", (0.92, 1.53, 0.0), -2300, 400)
    link(metres, "", patch, "PatchMetres")
    def flip(nrm):
        fix = custom(mat, FLIP_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                     ["N", "UV", "Rad", "PatchMetres"], "DragonPatchNormalFlip", -1000, 160)
        link(nrm, "RGB", fix, "N")
        link(tube_uv, "", fix, "UV")
        link(vc, "R", fix, "Rad")
        link(metres, "", fix, "PatchMetres")
        return fix
    skin(mat, patch, flip)
    finish(mat, "M_DragonBody", [])


FAR_HLSL = r"""
return saturate((D - NearCm) / max(FarCm - NearCm, 1.0));
"""

FLIP_HLSL = r"""
float2 m = float2(UV.x, UV.y * 6.2831853 * max(Rad, 0.04)) / max(PatchMetres.xy, 0.01);
float2 t = frac(m * 0.5) * 2.0;
float2 sgn = float2(t.x < 1.0 ? 1.0 : -1.0, t.y < 1.0 ? 1.0 : -1.0);
return float3(N.xy * sgn, N.z);
"""

PATCH_HLSL = r"""
float2 m = float2(UV.x, UV.y * 6.2831853 * max(Rad, 0.04)) / max(PatchMetres.xy, 0.01);
float2 t = frac(m * 0.5) * 2.0;
float2 tri = 1.0 - abs(t - 1.0);
return PatchOrigin.xy + tri * PatchSize.xy;
"""


build()
build_body()
