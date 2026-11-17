# Builds BT-01's materials from the imported textures, then assigns them to SM_BT01's slots.
# Run in the editor (tools\ue.ps1 editor, then execute_python through tools\mcp.ps1). The textures come first:
# T_BT01_<set>_<BC|N|R|M|AO|E> in /Game/Deep/Subs/Textures (Substance maps; the OpenGL normals are imported with
# flip_green_channel on, R/M/AO as masks without sRGB, capped 2048 for the hull, 1024 big parts, 512 the rest).
#   M_SubPBR      : plain PBR. Base colour is graded to the Style Guide (section 4): lerp(luma, colour, Saturation)
#                   times Tint, Saturation 0.7 and a darker, slightly cool Tint. Static switches cover sets without a
#                   normal or AO map; Emissive is only on for the cabin's screens and lamps (seen through the bow glass).
#   MI_BT01_<set> : one instance per texture set, named after the mesh's material slot it goes in.
import unreal

PATH = "/Game/Deep/Subs"
TEX = PATH + "/Textures"
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

SATURATION = 0.7
TINT = (0.62, 0.66, 0.66, 1.0)


def tex(set_name, short):
    path = "%s/T_BT01_%s_%s" % (TEX, set_name, short)
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def fresh(name, cls, factory):
    path = "%s/%s" % (PATH, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return tools.create_asset(name, PATH, cls, factory)


def sampler(mat, name, default, kind, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", default)
    e.set_editor_property("sampler_type", kind)
    return e


def switch(mat, name, default, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionStaticSwitchParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    return e


def build_parent():
    mat = fresh("M_SubPBR", unreal.Material, unreal.MaterialFactoryNew())
    S = unreal.MaterialSamplerType
    bc = sampler(mat, "BaseColor", tex("main", "BC"), S.SAMPLERTYPE_COLOR, -900, -400)
    nrm = sampler(mat, "Normal", tex("main", "N"), S.SAMPLERTYPE_NORMAL, -900, 0)
    rough = sampler(mat, "Roughness", tex("main", "R"), S.SAMPLERTYPE_MASKS, -900, 250)
    metal = sampler(mat, "Metallic", tex("main", "M"), S.SAMPLERTYPE_MASKS, -900, 500)
    ao = sampler(mat, "AO", tex("main", "AO"), S.SAMPLERTYPE_MASKS, -900, 750)
    emi = sampler(mat, "Emissive", tex("Interior_lights", "E"), S.SAMPLERTYPE_COLOR, -900, 1000)

    # Grade: Desaturation with Fraction = 1 - Saturation, then times Tint.
    sat = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, -600)
    sat.set_editor_property("parameter_name", "Saturation")
    sat.set_editor_property("default_value", SATURATION)
    one_minus = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -650, -600)
    mel.connect_material_expressions(sat, "", one_minus, "")
    desat = mel.create_material_expression(mat, unreal.MaterialExpressionDesaturation, -500, -400)
    mel.connect_material_expressions(bc, "RGB", desat, "")
    mel.connect_material_expressions(one_minus, "", desat, "Fraction")
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -650)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(*TINT))
    graded = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -450)
    mel.connect_material_expressions(desat, "", graded, "A")
    mel.connect_material_expressions(tint, "RGB", graded, "B")
    mel.connect_material_property(graded, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Normal: the map, or the flat normal when the set has none.
    use_n = switch(mat, "UseNormal", True, -400, 0)
    flat = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -650, 120)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    mel.connect_material_expressions(nrm, "RGB", use_n, "True")
    mel.connect_material_expressions(flat, "", use_n, "False")
    mel.connect_material_property(use_n, "", unreal.MaterialProperty.MP_NORMAL)

    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "R", unreal.MaterialProperty.MP_METALLIC)

    use_ao = switch(mat, "UseAO", True, -400, 750)
    one = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -650, 870)
    one.set_editor_property("r", 1.0)
    mel.connect_material_expressions(ao, "R", use_ao, "True")
    mel.connect_material_expressions(one, "", use_ao, "False")
    mel.connect_material_property(use_ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    use_e = switch(mat, "UseEmissive", False, -200, 1000)
    gain = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 1250)
    gain.set_editor_property("parameter_name", "EmissiveGain")
    gain.set_editor_property("default_value", 2.0)
    lit = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 1050)
    mel.connect_material_expressions(emi, "RGB", lit, "A")
    mel.connect_material_expressions(gain, "", lit, "B")
    black = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 1250)
    mel.connect_material_expressions(lit, "", use_e, "True")
    mel.connect_material_expressions(black, "", use_e, "False")
    mel.connect_material_property(use_e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mat.set_editor_property("used_with_nanite", True)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def build_instances(parent, mesh):
    made = []
    for slot in mesh.static_materials:
        set_name = str(slot.material_slot_name)
        mi = fresh("MI_BT01_" + set_name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        for param, short in (("BaseColor", "BC"), ("Normal", "N"), ("Roughness", "R"), ("Metallic", "M"), ("AO", "AO"), ("Emissive", "E")):
            t = tex(set_name, short)
            if t:
                mel.set_material_instance_texture_parameter_value(mi, param, t)
        mel.set_material_instance_static_switch_parameter_value(mi, "UseNormal", tex(set_name, "N") is not None)
        mel.set_material_instance_static_switch_parameter_value(mi, "UseAO", tex(set_name, "AO") is not None)
        mel.set_material_instance_static_switch_parameter_value(mi, "UseEmissive", tex(set_name, "E") is not None)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        made.append((set_name, mi))
    return made


mesh = unreal.load_asset(PATH + "/SM_BT01")
parent = build_parent()
made = build_instances(parent, mesh)
mats = mesh.static_materials
for i, slot in enumerate(mats):
    for set_name, mi in made:
        if set_name == str(slot.material_slot_name):
            mesh.set_material(i, mi)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
"%d instances on %d slots" % (len(made), len(mats))
