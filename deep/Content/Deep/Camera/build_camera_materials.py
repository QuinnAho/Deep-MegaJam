# Rebuilds the camera lane's two post-process materials from text. Run headless (no editor window), in the machine slot:
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering
# Recipe and sources: deep-claude-brain/03 - Research/Camera - Footage Look and Sonar Pulse Recipe.md
#
#   M_SonarPulse  : the sonar ping made visible (Deep.Sonar.Pulse). Post-process, after the tonemapper (Epic: the
#                   preferred location for simple effects; the colour there is display-encoded, so the amber #FFB45A is
#                   added as its own sRGB value). Each pixel's world position is rebuilt from scene depth (Absolute World
#                   Position in a post-process material), its distance to Origin compared with the shell's radius: a thin
#                   bright leading band with a short trail, and behind it a lidar dot grid (world aligned, projected on
#                   the surface's dominant axis) that fades DotFadeS after the shell passed. Added on top of the scene
#                   colour, so it lights nothing. The serpent writes custom depth (Camera/ sets it): no dots on it, only
#                   the band. Driven by a dynamic material instance from UDeepCameraSubsystem.
#   M_FootageLens : slight barrel distortion for the footage look (Deep.Look.Footage). Post-process, after the
#                   tonemapper, priority above the pulse so the overlay bends with the picture. Samples PostProcessInput0
#                   at a radially pushed UV, normalised so the corners stay on the corners (no black edge).
import unreal

PATH = "/Game/Deep/Camera"
mel = unreal.MaterialEditingLibrary

PULSE_HLSL = r"""
float3 wp = WP;
float depth = Depth.r;
float d = length(wp - Origin);
float3 scene = Scene.rgb;
// Sky and anything past the range or inside the hull: untouched.
if (d > MaxRadiusCm + BandCm || d < StartCm || depth > 1.0e6)
{
    return scene;
}
// The shell: a sharp leading edge, a short trail behind it; gone once the shell has reached its range.
float ahead = d - RadiusCm;
float band = ahead > 0.0 ? saturate(1.0 - ahead / (BandCm * 0.35)) : exp(ahead / BandCm);
band *= saturate((ShellS - AgeS) / 0.3) * saturate(RadiusCm / 200.0);
// Lidar dots: a world grid on the surface's dominant plane, lit when the shell passes and fading after.
float3 n = abs(normalize(Normal.xyz * 2.0 - 1.0));
float2 uv = n.z >= max(n.x, n.y) ? wp.xy : (n.x >= n.y ? wp.yz : wp.xz);
float2 g = (frac(uv / SpacingCm) - 0.5) * SpacingCm;
float r = max(1.2, depth * 0.0018);
float dotMask = 1.0 - smoothstep(r * 0.6, r, length(g));
float passed = AgeS - d / max(SpeedCmPerS, 1.0);
float dotFade = passed >= 0.0 ? saturate(1.0 - passed / max(DotFadeS, 0.01)) : 0.0;
float farFade = 1.0 - smoothstep(DotFarCm * 0.5, DotFarCm, depth);
// The serpent writes custom depth: where it is the nearest surface it shows only the band, never the dots.
float serpent = CustomDepth.r < depth + 5.0 ? 1.0 : 0.0;
float dots = dotMask * dotFade * farFade * (1.0 - serpent);
return scene + Color.rgb * Intensity * (band * BandIntensity + dots * DotIntensity);
"""

LENS_HLSL = r"""
float2 c = UV - 0.5;
float r2 = dot(c, c);
return 0.5 + c * (1.0 + K * r2) / (1.0 + K * 0.5);
"""


def fresh(name):
    full = PATH + "/" + name
    mat = unreal.load_asset(full)
    if mat:
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    return mat


def post_process(mat, priority):
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    mat.set_editor_property("blendable_priority", priority)


def scalar(mat, name, value, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector(mat, name, rgba, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgba[0], rgba[1], rgba[2], rgba[3]))
    return e


def scene_texture(mat, tex_id, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, x, y)
    e.set_editor_property("scene_texture_id", tex_id)
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
        unreal.log_error("DEEPCAMMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))
        return False
    return True


def link_any(src, out, dst, inputs):
    for inp in inputs:
        if mel.connect_material_expressions(src, out, dst, inp):
            unreal.log("DEEPCAMMAT connected %s -> %s.%s" % (src.get_name(), dst.get_name(), inp))
            return True
    unreal.log_error("DEEPCAMMAT could not connect %s -> %s (tried %s)" % (src.get_name(), dst.get_name(), inputs))
    return False


def finish(mat):
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DEEPCAMMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DEEPCAMMAT compile %s: %s" % (mat.get_name(), str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DEEPCAMMAT %s saved=%s expressions=%d errors=%d domain=%s location=%s" % (
        mat.get_name(), ok, mel.get_num_material_expressions(mat), len(errors or []),
        mat.get_editor_property("material_domain"), mat.get_editor_property("blendable_location")))


def build_pulse():
    mat = fresh("M_SonarPulse")
    post_process(mat, 0)
    inputs = ["WP", "Depth", "CustomDepth", "Normal", "Scene", "Origin", "RadiusCm", "MaxRadiusCm", "StartCm", "BandCm",
              "AgeS", "ShellS", "SpeedCmPerS", "DotFadeS", "SpacingCm", "DotFarCm", "BandIntensity", "DotIntensity",
              "Intensity", "Color"]
    pulse = custom(mat, PULSE_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs, "SonarPulse", -400, 0)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, -600)
    link(wp, "", pulse, "WP")
    link(scene_texture(mat, unreal.SceneTextureId.PPI_SCENE_DEPTH, -1400, -480), "Color", pulse, "Depth")
    link(scene_texture(mat, unreal.SceneTextureId.PPI_CUSTOM_DEPTH, -1400, -360), "Color", pulse, "CustomDepth")
    link(scene_texture(mat, unreal.SceneTextureId.PPI_WORLD_NORMAL, -1400, -240), "Color", pulse, "Normal")
    link(scene_texture(mat, unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -1400, -120), "Color", pulse, "Scene")
    link(vector(mat, "Origin", (0.0, 0.0, 0.0, 0.0), -1100, 0), "", pulse, "Origin")
    y = 80
    for name, value in [("RadiusCm", 0.0), ("MaxRadiusCm", 6000.0), ("StartCm", 600.0), ("BandCm", 60.0),
                        ("AgeS", 100.0), ("ShellS", 1.5), ("SpeedCmPerS", 4000.0), ("DotFadeS", 2.0),
                        ("SpacingCm", 35.0), ("DotFarCm", 4000.0), ("BandIntensity", 0.9), ("DotIntensity", 0.35),
                        ("Intensity", 0.0)]:
        link(scalar(mat, name, value, -1100, y), "", pulse, name)
        y += 80
    link(vector(mat, "Color", (1.0, 180.0 / 255.0, 90.0 / 255.0, 1.0), -1100, y), "", pulse, "Color")
    mel.connect_material_property(pulse, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


def build_lens():
    mat = fresh("M_FootageLens")
    post_process(mat, 10)
    screen = mel.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -1000, 0)
    bend = custom(mat, LENS_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT2, ["UV", "K"], "Barrel", -600, 0)
    link(screen, "ViewportUV", bend, "UV")
    link(scalar(mat, "K", 0.06, -1000, 150), "", bend, "K")
    color = scene_texture(mat, unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -300, 0)
    link_any(bend, "", color, ["UVs", "Coordinates", "UV"])
    mel.connect_material_property(color, "Color", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


build_pulse()
build_lens()
