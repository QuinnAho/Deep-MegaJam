# Rebuilds the camera lane's post-process materials from text. Run headless (no editor window), in the machine slot:
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
#                   the band. Every pixel inside the box round the sub's parts (HullCentre, HullX/Y/Z = the box's axes
#                   over its half extents) is skipped, so the shell never lights the sub it leaves from. Driven by a
#                   dynamic material instance from UDeepCameraSubsystem.
#   M_FootageLens : slight barrel distortion for the footage look (Deep.Look.Footage). Post-process, after the
#                   tonemapper, priority above the pulse so the overlay bends with the picture. Samples PostProcessInput0
#                   at a radially pushed UV, normalised so the corners stay on the corners (no black edge).
#   M_FeedInterference : the feed breaking up as the serpent closes (Deep.Look.Interference). Post-process, after the
#                   tonemapper, priority above the lens (the signal fails after the optics). Bands of lines tear sideways and
#                   a rolling band wobbles (UV offsets per band, hashed per frame); red and blue sample apart (the same UV
#                   offsets per channel); dim static snow, scanlines and a rare dropout line. Strength S 0..1 from C++.
import unreal

PATH = "/Game/Deep/Camera"
mel = unreal.MaterialEditingLibrary

PULSE_HLSL = r"""
float3 wp = WP;
float depth = Depth.r;
float d = length(wp - Origin.xyz);
float3 scene = Scene.rgb;
// Sky and anything past the range: untouched.
if (d > MaxRadiusCm + BandCm || d < StartCm || depth > 1.0e6)
{
    return scene;
}
// The sub itself: a box round its parts (axes pre-divided by the half extents, so inside = all three |q| < 1). The
// shell starts inside the hull and would otherwise light the bow, skids and window frame as it leaves.
float3 rel = wp - HullCentre.xyz;
float3 q = abs(float3(dot(rel, HullX.xyz), dot(rel, HullY.xyz), dot(rel, HullZ.xyz)));
if (max(q.x, max(q.y, q.z)) < 1.0)
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
              "Intensity", "Color", "HullCentre", "HullX", "HullY", "HullZ"]
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
    # The hull mask (C++ fills them from the sub each frame). Defaults: a 1 cm box at the world origin, masking nothing.
    y += 80
    for name, value in [("HullCentre", (0.0, 0.0, 0.0, 0.0)), ("HullX", (1.0, 0.0, 0.0, 0.0)),
                        ("HullY", (0.0, 1.0, 0.0, 0.0)), ("HullZ", (0.0, 0.0, 1.0, 0.0))]:
        link(vector(mat, name, value, -1100, y), "", pulse, name)
        y += 80
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


FEED_UV_HLSL = r"""
float t = floor(TimeS * 24.0);
float band = floor(UV.y * Bands);
// Hoskins hash (the sin hash streaks diagonally on the GPU at large inputs).
float3 q = frac(float3(band, t, band + t) * float3(0.1031, 0.1030, 0.0973));
q += dot(q, q.yzx + 33.33);
float h = frac((q.x + q.y) * q.z);
float h2 = frac((q.x + q.z) * q.y);
// A few bands tear sideways: more of them, and further, as the strength rises.
float tear = step(1.0 - 0.3 * S, h) * (h2 - 0.5) * 2.0;
// A slow rolling band where the lines wobble.
float roll = frac(UV.y + TimeS * 0.21);
float wob = exp(-pow((roll - 0.5) * 10.0, 2.0)) * sin(UV.y * 600.0 + TimeS * 50.0);
float dx = (tear * Tear + wob * Tear * 0.25) * S * S;
return float2(UV.x + dx, UV.y);
"""

FEED_HLSL = r"""
float3 c = float3(R.r, G.g, B.b);
float t = floor(TimeS * 30.0);
float2 p = floor(UV * float2(480.0, 270.0));
// Hoskins hash with the frame folded in, so the snow has no diagonal streaks.
float3 q = frac((float3(p.x, p.y, p.x) + float3(t * 17.0, t * 31.0, t * 7.0)) * float3(0.1031, 0.1030, 0.0973));
q += dot(q, q.yzx + 33.33);
float n = frac((q.x + q.y) * q.z);
// Snow mostly modulates the picture (black stays black), with a faint floor so the dark still crawls.
c = lerp(c, c * n * 2.0 + n * 0.03, saturate(S * Static));
// Scanlines darken alternate lines a little.
float scan = 0.5 + 0.5 * sin(UV.y * 6.28318 * 270.0);
c *= 1.0 - 0.2 * S * scan;
// Now and then a dim dropout line (about one a frame at full strength).
float row = floor(UV.y * 270.0);
float3 r = frac(float3(row, t, row - t) * float3(0.1031, 0.1030, 0.0973));
r += dot(r, r.yzx + 33.33);
float d = frac((r.x + r.y) * r.z);
c += step(1.0 - 0.004 * S, d) * 0.12 * S;
return c;
"""


def build_interference():
    mat = fresh("M_FeedInterference")
    post_process(mat, 20)
    screen = mel.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -1600, 0)
    s = scalar(mat, "S", 0.0, -1600, 150)
    time = scalar(mat, "TimeS", 0.0, -1600, 230)
    uv = custom(mat, FEED_UV_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT2, ["UV", "S", "TimeS", "Tear", "Bands"], "FeedUV", -1200, 0)
    link(screen, "ViewportUV", uv, "UV")
    link(s, "", uv, "S")
    link(time, "", uv, "TimeS")
    link(scalar(mat, "Tear", 0.03, -1600, 310), "", uv, "Tear")
    link(scalar(mat, "Bands", 70.0, -1600, 390), "", uv, "Bands")
    split = scalar(mat, "SplitUV", 0.0, -1600, 470)
    right = custom(mat, "return UV + float2(Off, 0.0);", unreal.CustomMaterialOutputType.CMOT_FLOAT2, ["UV", "Off"], "SplitRed", -900, -200)
    left = custom(mat, "return UV - float2(Off, 0.0);", unreal.CustomMaterialOutputType.CMOT_FLOAT2, ["UV", "Off"], "SplitBlue", -900, 200)
    for node in (right, left):
        link(uv, "", node, "UV")
        link(split, "", node, "Off")
    red = scene_texture(mat, unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -600, -200)
    green = scene_texture(mat, unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -600, 0)
    blue = scene_texture(mat, unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -600, 200)
    link_any(right, "", red, ["UVs", "Coordinates", "UV"])
    link_any(uv, "", green, ["UVs", "Coordinates", "UV"])
    link_any(left, "", blue, ["UVs", "Coordinates", "UV"])
    feed = custom(mat, FEED_HLSL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["R", "G", "B", "UV", "S", "TimeS", "Static"], "Feed", -300, 0)
    link(red, "Color", feed, "R")
    link(green, "Color", feed, "G")
    link(blue, "Color", feed, "B")
    link(screen, "ViewportUV", feed, "UV")
    link(s, "", feed, "S")
    link(time, "", feed, "TimeS")
    link(scalar(mat, "Static", 0.3, -600, 400), "", feed, "Static")
    mel.connect_material_property(feed, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


# DEEPCAMMAT_ONLY=M_FeedInterference (environment) rebuilds just that one; unset rebuilds all three.
import os
ONLY = os.environ.get("DEEPCAMMAT_ONLY", "")
for name, build in (("M_SonarPulse", build_pulse), ("M_FootageLens", build_lens), ("M_FeedInterference", build_interference)):
    if not ONLY or ONLY == name:
        build()
