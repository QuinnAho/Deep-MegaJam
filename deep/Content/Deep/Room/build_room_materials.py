# Rebuilds the room-and-hull materials from text. Run headless (no editor window):
#   UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script="<abs path to this file>"
#     -EnablePlugins=PythonScriptPlugin -unattended -nopause -nosplash -AllowCommandletRendering -ModelContextProtocolPort=8013
#   M_WornSteel : worn painted steel for any mesh (walls, deck, hull, consoles, salvage, relic). No UVs and no baked detail:
#                 everything comes from 3D value noise of the position relative to the object's pivot, so it has no seams on any
#                 mesh and does not swim when the arena moves an object. Paint chipped to bare steel, rust streaks running
#                 down, grime blotches, a bump from the same noise (world-space normal), and roughness broken up by grime.
#                 Wet (0..1) takes 0.1 off roughness; puddles form on up-facing surfaces inside PuddleRadius of PuddleCentre.
#   M_Screen    : an amber instrument screen (#FFB45A): dim graph lines and bars on near-black glass, with scanlines. UV0.
#   M_Glass     : the helm window. Translucent, lit per pixel (Surface ForwardShading), so Lumen front-layer reflections put
#                 a faint copy of the lit room on it over the dark sea; with ray tracing off it falls back to screen space.
#   MI_FrameSteel, MI_PipeSteel, MI_DullBrass, MI_Rubber, MI_GrimyGlass : instances of M_WornSteel for the room's dressing
#                 (World/DeepRoomDress). EdgeWear (default 0) chips paint on edges found from the instance-local position.
import unreal

PATH = "/Game/Deep/Room"
mel = unreal.MaterialEditingLibrary

# 3D value noise and fbm. P in cm relative to the object's pivot.
NOISE = r"""
struct DeepN {
float h(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float n(float3 x) {
  float3 i = floor(x); float3 f = frac(x); f = f * f * (3.0 - 2.0 * f);
  return lerp(lerp(lerp(h(i), h(i + float3(1,0,0)), f.x), lerp(h(i + float3(0,1,0)), h(i + float3(1,1,0)), f.x), f.y),
              lerp(lerp(h(i + float3(0,0,1)), h(i + float3(1,0,1)), f.x), lerp(h(i + float3(0,1,1)), h(i + float3(1,1,1)), f.x), f.y), f.z);
}
float fbm(float3 x) { float a = 0.5; float s = 0.0; for (int k = 0; k < 4; k++) { s += a * n(x); x = x * 2.03 + 17.1; a *= 0.5; } return s / 0.9375; }
// chip: 1 where paint is gone. rust: 1 in streaks running down. grime: 0..1 blotches.
// Chips: patches a hand across, ragged at the rim (the fine term only moves the edge), not specks.
float chip(float3 p, float wear) { float v = fbm(p * 0.018) * 0.85 + n(p * 0.2) * 0.15; return smoothstep(1.0 - wear, 1.0 - wear + 0.03, v); }
float rust(float3 p, float amt) { float s = fbm(float3(p.x * 0.06, p.y * 0.06, p.z * 0.006)) * 0.7 + fbm(p * 0.02) * 0.3; return smoothstep(1.0 - amt, 1.0 - amt + 0.18, s); }
float grime(float3 p) { return fbm(p * 0.008 + 3.7); }
float height(float3 p, float wear, float amt) { return 0.6 * (1.0 - chip(p, wear)) + 0.3 * rust(p, amt) * n(p * 0.5) + 0.1 * n(p * 1.3); }
// Edge wear (Lane E): near two faces at once = an edge or a corner. Width is in local units (percent of the piece's size),
// broken by noise so it reads as chipped edges, not an outline. Off when EdgeWear is 0.
float edge(float3 lp, float3 p, float wearAmt, float width) {
  float3 d = 50.0 - abs(lp);
  float mid = max(min(d.x, d.y), min(max(d.x, d.y), d.z));
  float e = 1.0 - smoothstep(0.0, max(width, 0.01), mid);
  float v = e * (0.55 + 0.9 * fbm(p * 0.09 + 5.3));
  return step(0.001, wearAmt) * smoothstep(1.0 - wearAmt, 1.0 - wearAmt + 0.08, v);
}
};
DeepN N;
"""

COLOR = NOISE + r"""
float c = max(N.chip(P, Wear), N.edge(LP, P, EdgeWear, EdgeWidth)); float r = N.rust(P, Rust); float g = N.grime(P);
float3 col = lerp(Paint.rgb, Bare.rgb, c);
col = lerp(col, RustCol.rgb * (0.7 + 0.6 * N.n(P * 0.4)), r);
col *= lerp(0.65, 1.05, g);
float up = saturate((Nw.z - 0.8) * 5.0);
float wetHere = saturate(Wet + up * (1.0 - saturate(length(WP.xy - Centre.xy) / max(Radius, 1.0))));
float puddle = up * step(0.55, N.fbm(WP * 0.01 + 1.3)) * saturate(1.0 - length(WP.xy - Centre.xy) / max(Radius, 1.0));
col *= lerp(1.0, 0.6, saturate(wetHere * 0.6 + puddle));
return col;
"""

ROUGH = NOISE + r"""
float c = max(N.chip(P, Wear), N.edge(LP, P, EdgeWear, EdgeWidth)); float r = N.rust(P, Rust); float g = N.grime(P);
float rough = lerp(PaintRough, BareRough, c);
rough = lerp(rough, 0.85, r);
rough += (g - 0.5) * 0.25;
float up = saturate((Nw.z - 0.8) * 5.0);
float near = saturate(1.0 - length(WP.xy - Centre.xy) / max(Radius, 1.0));
float wetHere = saturate(Wet + up * near);
rough -= 0.1 * wetHere;
float puddle = up * near * smoothstep(0.5, 0.58, N.fbm(WP * 0.01 + 1.3));
rough = lerp(rough, 0.12, puddle);
return clamp(rough, 0.08, 0.95);
"""

METAL = NOISE + r"""
float c = max(N.chip(P, Wear), N.edge(LP, P, EdgeWear, EdgeWidth)); float r = N.rust(P, Rust);
return saturate(lerp(PaintMetal, 1.0, c) * (1.0 - r));
"""

# World-space normal: bump from the height's gradient (finite differences of 3D noise), projected onto the surface.
NORMAL = NOISE + r"""
float e = 0.6;
float h0 = N.height(P, Wear, Rust);
float3 gr = float3(N.height(P + float3(e,0,0), Wear, Rust) - h0, N.height(P + float3(0,e,0), Wear, Rust) - h0, N.height(P + float3(0,0,e), Wear, Rust) - h0) / e;
float fade = saturate(1.0 - Dist / 3000.0);
float3 t = gr - dot(gr, Nw) * Nw;
float up = saturate((Nw.z - 0.8) * 5.0);
float puddle = up * smoothstep(0.5, 0.58, N.fbm(WP * 0.01 + 1.3)) * saturate(1.0 - length(WP.xy - Centre.xy) / max(Radius, 1.0));
return normalize(Nw - t * Bump * fade * (1.0 - puddle));
"""

SCREEN = r"""
float2 uv = UV;
float scan = 0.82 + 0.18 * sin(uv.y * 520.0);
float grid = step(0.985, frac(uv.x * 12.0)) + step(0.97, frac(uv.y * 6.0));
float wave = 1.0 - smoothstep(0.0, 0.012, abs(uv.y - (0.55 + 0.18 * sin(uv.x * 11.0 + Time * 0.6) * sin(uv.x * 3.1))));
float row = floor(uv.y * 9.0);
float len = 0.06 + 0.22 * frac(sin(row * 12.9898) * 43758.5);
float bars = step(0.05, uv.x) * step(uv.x, 0.05 + len) * step(0.3, frac(uv.y * 9.0)) * step(frac(uv.y * 9.0), 0.7);
float edge = smoothstep(0.0, 0.04, uv.x) * smoothstep(0.0, 0.04, 1.0 - uv.x) * smoothstep(0.0, 0.06, uv.y) * smoothstep(0.0, 0.06, 1.0 - uv.y);
float v = (0.10 + 0.12 * grid + 0.9 * wave + 0.45 * bars) * scan * edge;
return Col.rgb * v * Glow;
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
        unreal.log_error("DEEPROOMMAT could not connect %s.%s -> %s.%s" % (src.get_name(), out, dst.get_name(), inp))


def finish(mat):
    try:
        errors = mel.recompile_material(mat)
    except Exception as ex:
        unreal.log_warning("DEEPROOMMAT not compiled here: " + str(ex))
        errors = []
    for e in errors or []:
        unreal.log_error("DEEPROOMMAT compile %s: %s" % (mat.get_name(), str(e)))
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
    unreal.log("DEEPROOMMAT %s saved=%s expressions=%d errors=%d" % (
        mat.get_name(), ok, mel.get_num_material_expressions(mat), len(errors or [])))


def build_steel():
    mat = fresh("M_WornSteel")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("tangent_space_normal", False)
    # Room dressing is drawn as instanced static meshes (World/DeepRoomDress).
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1800, 0)
    obj = mel.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -1800, 120)
    rel = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1600, 60)
    link(wp, "", rel, "A")
    link(obj, "", rel, "B")
    nw = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1800, 240)
    dist = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1800, 340)
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1800, 440)  # instance-local

    # Paint: muted grey-green (sRGB about 62,68,64; saturation under 10%). Bare steel: grey, metallic. Rust: brown under 35%.
    p = {
        "Paint": vector(mat, "PaintColor", (0.048, 0.056, 0.052), -1300, -500),
        "Bare": vector(mat, "BareColor", (0.2, 0.2, 0.195), -1300, -420),
        "RustCol": vector(mat, "RustColor", (0.10, 0.062, 0.042), -1300, -340),
        "Wear": scalar(mat, "Wear", 0.22, -1300, -260),
        "Rust": scalar(mat, "Rust", 0.3, -1300, -200),
        "Wet": scalar(mat, "Wet", 0.0, -1300, -140),
        "Centre": vector(mat, "PuddleCentre", (0.0, 0.0, 0.0), -1300, -80),
        "Radius": scalar(mat, "PuddleRadius", 0.0, -1300, -20),
        "PaintRough": scalar(mat, "PaintRoughness", 0.62, -1300, 40),
        "BareRough": scalar(mat, "BareRoughness", 0.42, -1300, 100),
        "PaintMetal": scalar(mat, "PaintMetal", 0.0, -1300, 160),
        "Bump": scalar(mat, "Bump", 0.35, -1300, 220),
        "EdgeWear": scalar(mat, "EdgeWear", 0.0, -1300, 280),
        "EdgeWidth": scalar(mat, "EdgeWidth", 4.0, -1300, 340),
    }

    def node(code, out, names, desc, y):
        e = custom(mat, code, out, ["P", "WP", "Nw", "Dist", "LP"] + names, desc, -700, y)
        link(lp, "", e, "LP")
        link(rel, "", e, "P")
        link(wp, "", e, "WP")
        link(nw, "", e, "Nw")
        link(dist, "", e, "Dist")
        for n in names:
            link(p[n], "", e, n)
        return e

    f3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    f1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
    color = node(COLOR, f3, ["Paint", "Bare", "RustCol", "Wear", "Rust", "Wet", "Centre", "Radius", "EdgeWear", "EdgeWidth"], "SteelColor", -400)
    rough = node(ROUGH, f1, ["PaintRough", "BareRough", "Wear", "Rust", "Wet", "Centre", "Radius", "EdgeWear", "EdgeWidth"], "SteelRoughness", -100)
    metal = node(METAL, f1, ["Wear", "Rust", "PaintMetal", "EdgeWear", "EdgeWidth"], "SteelMetallic", 150)
    normal = node(NORMAL, f3, ["Wear", "Rust", "Bump", "Centre", "Radius"], "SteelNormal", 400)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    finish(mat)


def build_screen():
    mat = fresh("M_Screen")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1200, 120)
    e = custom(mat, SCREEN, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["UV", "Time", "Col", "Glow"], "Screen", -600, 0)
    link(uv, "", e, "UV")
    link(time, "", e, "Time")
    link(vector(mat, "ScreenColor", (1.0, 0.456, 0.102), -1000, 200), "", e, "Col")  # #FFB45A in linear
    link(scalar(mat, "Glow", 6.0, -1000, 300), "", e, "Glow")
    mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(vector(mat, "GlassColor", (0.01, 0.01, 0.01), -600, 300), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 0.18, -600, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


def build_glass():
    mat = fresh("M_Glass")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -800, 200)
    lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -500, 200)
    link(scalar(mat, "OpacityFacing", 0.1, -800, 100), "", lerp, "A")
    link(scalar(mat, "OpacityGrazing", 0.5, -800, 160), "", lerp, "B")
    link(fres, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(vector(mat, "Tint", (0.012, 0.016, 0.016), -500, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 0.05, -500, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Specular", 0.5, -500, 360), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(mat)


# Instances of M_WornSteel for the room's dressing (World/DeepRoomDress, Lane E). Epic's material skill: instance before new
# material. Colours are linear; every surface stays at or under 35% saturation (style guide), roughness 0.4..0.9 except glass.
INSTANCES = {
    # Ribs, lockers, the tray, cages: the hull's grey-green paint, worn through on every edge a shoulder or a boot meets.
    "MI_FrameSteel": dict(vec={"PaintColor": (0.042, 0.049, 0.045), "BareColor": (0.17, 0.17, 0.165), "RustColor": (0.09, 0.055, 0.037)},
                          sca={"Wear": 0.28, "Rust": 0.4, "EdgeWear": 0.45, "EdgeWidth": 5.0, "PaintRoughness": 0.66,
                               "BareRoughness": 0.45, "PaintMetal": 0.0, "Bump": 0.35}),
    # Pipes and valve bodies: a warmer, dirtier paint, rust running from the flanges.
    "MI_PipeSteel": dict(vec={"PaintColor": (0.058, 0.055, 0.046), "BareColor": (0.15, 0.148, 0.14), "RustColor": (0.10, 0.058, 0.036)},
                         sca={"Wear": 0.3, "Rust": 0.55, "EdgeWear": 0.3, "EdgeWidth": 6.0, "PaintRoughness": 0.6,
                              "BareRoughness": 0.45, "PaintMetal": 0.0, "Bump": 0.3}),
    # Valve wheels, bezels, handles: tarnished brass, rubbed back to bright brass where hands go.
    "MI_DullBrass": dict(vec={"PaintColor": (0.13, 0.10, 0.06), "BareColor": (0.26, 0.20, 0.115), "RustColor": (0.045, 0.058, 0.045)},
                         sca={"Wear": 0.25, "Rust": 0.35, "EdgeWear": 0.5, "EdgeWidth": 8.0, "PaintRoughness": 0.55,
                              "BareRoughness": 0.4, "PaintMetal": 1.0, "Bump": 0.2}),
    # Cables, the cushion, vents: black rubber, grime only.
    "MI_Rubber": dict(vec={"PaintColor": (0.013, 0.013, 0.013), "BareColor": (0.02, 0.02, 0.019), "RustColor": (0.04, 0.038, 0.035)},
                      sca={"Wear": 0.05, "Rust": 0.1, "EdgeWear": 0.0, "PaintRoughness": 0.85, "BareRoughness": 0.8,
                           "PaintMetal": 0.0, "Bump": 0.15}),
    # Gauge faces, bulbs, the strip's cover: near-black glass with grime blotches (opaque: these are small and seen dark).
    "MI_GrimyGlass": dict(vec={"PaintColor": (0.006, 0.008, 0.008), "BareColor": (0.012, 0.013, 0.013), "RustColor": (0.03, 0.03, 0.025)},
                          sca={"Wear": 0.0, "Rust": 0.2, "EdgeWear": 0.0, "PaintRoughness": 0.18, "BareRoughness": 0.3,
                               "PaintMetal": 0.0, "Bump": 0.05}),
}


def build_instances():
    parent = unreal.load_asset(PATH + "/M_WornSteel")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, spec in INSTANCES.items():
        mi = unreal.load_asset(PATH + "/" + name)
        if not mi:
            mi = tools.create_asset(name, PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        for k, v in spec["vec"].items():
            mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
        for k, v in spec["sca"].items():
            mel.set_material_instance_scalar_parameter_value(mi, k, v)
        mel.update_material_instance(mi)
        ok = unreal.EditorLoadingAndSavingUtils.save_packages([mi.get_outermost()], False)
        unreal.log("DEEPROOMMAT %s saved=%s parent=%s" % (name, ok, parent.get_name() if parent else None))


build_steel()
build_screen()
build_glass()
build_instances()
