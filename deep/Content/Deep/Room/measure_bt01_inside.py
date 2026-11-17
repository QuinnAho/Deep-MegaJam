# What fits inside BT-01 (SM_BT01, read only): the room set (ADeepSubRoom) is sized from this. Run it in a full editor,
# inside the DeepUnrealSlot mutex, through tools\mcp.ps1 (execute_python) or -ExecutePythonScript="<this file> [mode]".
# Modes:
#   slots    every material slot's box at the game's fit scale, in sub-local cm (X ahead, Y starboard, Z up, the hull's
#            hatch at x 0 y 0 and the old box hull's centre at z 0, as DeepHull::ModelTransform places it)
#   layers   per-triangle traces through the model along sub-local lines; every surface each line crosses, with its slot
#   frame x y z pitch yaw png
#            a 1280x720 frame from that sub-local point inside the model, lit by one temporary lamp (BT-01's cabin as found)
# Writes one JSON per mode to deep/Saved/Harness/bt01-<mode>.json and prints "BT01IN" lines. Spawns the mesh far from
# everything in the open editor world and removes it again; it saves nothing.
import json
import os
import sys
import unreal

MESH = "/Game/Deep/Subs/SM_BT01"
ENVELOPE = (1230.0, 330.0, 500.0)  # DeepHull::ModelEnvelopeCm
HATCH_XY = (45.5, 0.0)  # DeepHull::ModelHatchLocalCm
OUTER_Z = 170.0  # the old box hull's outer half height: the hull's hatch sits at z -OUTER_Z
FAR = unreal.Vector(0.0, 0.0, 2000000.0)
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Harness")


def log(msg):
    unreal.log("BT01IN " + msg)
    print("BT01IN " + msg)


def fit_scale(mesh):
    b = mesh.get_bounding_box()
    size = b.max - b.min
    return min(ENVELOPE[0] / size.x, ENVELOPE[1] / size.y, ENVELOPE[2] / size.z)


def to_sub(mesh, p, scale):
    mn = mesh.get_bounding_box().min
    return ((p.x - HATCH_XY[0]) * scale, (p.y - HATCH_XY[1]) * scale, (p.z - mn.z) * scale - OUTER_Z)


def to_world(mesh, s, scale):
    """Sub-local cm -> the world point on the model spawned at FAR with the fit scale."""
    mn = mesh.get_bounding_box().min
    m = unreal.Vector(s[0] / scale + HATCH_XY[0], s[1] / scale + HATCH_XY[1], (s[2] + OUTER_Z) / scale + mn.z)
    return FAR + m * scale


def spawn(mesh, scale):
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_object(mesh, FAR)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    return actor


def write(name, data):
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    path = os.path.join(OUT, "bt01-%s.json" % name)
    with open(path, "w") as f:
        json.dump(data, f, indent=1)
    log("wrote %s" % path)


def slots(mesh):
    scale = fit_scale(mesh)
    b = mesh.get_bounding_box()
    log("scale %.5f mesh box %s .. %s" % (scale, b.min, b.max))
    log("sub-local box %s .. %s" % (to_sub(mesh, b.min, scale), to_sub(mesh, b.max, scale)))
    read = unreal.GeometryScriptMeshReadLOD()
    read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    read.lod_index = 0
    dm = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read, True)
    got = unreal.GeometryScript_AssetUtils.get_section_material_list_from_static_mesh(mesh, read)
    names = [str(n) for n in got[2]] if isinstance(got, tuple) and len(got) >= 3 else []
    pool = unreal.DynamicMeshPool()
    split = None
    for attr in dir(unreal.GeometryScript_MeshDecomposition):
        if "material" in attr and attr.startswith("split"):
            split = getattr(unreal.GeometryScript_MeshDecomposition, attr)
    out = split(dm, pool)
    parts, ids = out[1], out[2]  # (target, component meshes, material ids)
    rows = []
    for part, mid in zip(parts, ids):
        box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(part)
        lo = to_sub(mesh, box.min, scale)
        hi = to_sub(mesh, box.max, scale)
        tris = unreal.GeometryScript_MeshQueries.get_num_triangle_i_ds(part) if hasattr(unreal.GeometryScript_MeshQueries, "get_num_triangle_i_ds") else -1
        name = names[mid] if 0 <= mid < len(names) else str(mid)
        rows.append({"slot": name, "id": mid, "tris": tris, "min": lo, "max": hi})
        log("slot %-28s tris %7d  x %7.1f..%7.1f  y %7.1f..%7.1f  z %7.1f..%7.1f" % (name, tris, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
    write("slots", {"scale": scale, "slots": rows})


def slot_of(comp, hit):
    try:
        mat, _ = comp.get_material_from_collision_face_index(hit[15])
        return mat.get_name().replace("MI_BT01_", "") if mat else ""
    except Exception:
        return "?"


def layers_along(world, comp, mesh, scale, a, b):
    """Every surface crossed from a to b (sub-local cm): [distance from a in sub cm, slot, normal z]."""
    wa = to_world(mesh, a, scale)
    wb = to_world(mesh, b, scale)
    d = wb - wa
    length = d.length()
    if length <= 0.0:
        return []
    u = d * (1.0 / length)
    hits = []
    start = wa
    travelled = 0.0
    for _ in range(80):
        h = unreal.SystemLibrary.line_trace_single(world, start, wb, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                   unreal.DrawDebugTrace.NONE, True)
        t = h.to_tuple() if h is not None else None
        if not t or not t[0]:
            break
        travelled += t[3]
        n = t[7]
        hits.append([round(travelled, 1), slot_of(comp, t), round(n.x, 2), round(n.y, 2), round(n.z, 2)])
        start = t[5] + u * 0.3
        travelled += 0.3
        if travelled >= length:
            break
    return hits


def layers(mesh):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    scale = fit_scale(mesh)
    actor = spawn(mesh, scale)
    comp = actor.static_mesh_component
    rays = []
    for x in range(-700, 701, 25):
        rays.append({"name": "down x%d" % x, "a": (x, 0, 400), "b": (x, 0, -250)})
        for z in range(-160, 341, 20):
            rays.append({"name": "side x%d z%d" % (x, z), "a": (x, 250, z), "b": (x, -250, z)})
    for z in range(-160, 341, 10):
        for y in (0, -50, 50):
            rays.append({"name": "along y%d z%d" % (y, z), "a": (800, y, z), "b": (-800, y, z)})
    for r in rays:
        r["hits"] = layers_along(world, comp, mesh, scale, r["a"], r["b"])
    actor.destroy_actor()
    write("layers", {"scale": scale, "rays": rays})
    log("layers: %d rays" % len(rays))


def frame(mesh, x, y, z, pitch, yaw, png):
    """A frame from inside the model: a camera at a sub-local point and one lamp beside it, both removed after."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    scale = fit_scale(mesh)
    actor = spawn(mesh, scale)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    at = to_world(mesh, (x, y, z), scale)
    rot = unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)
    cam = eas.spawn_actor_from_class(unreal.CameraActor, at, rot)
    cam.camera_component.set_editor_property("field_of_view", 90.0)
    lamp = eas.spawn_actor_from_class(unreal.PointLight, at + unreal.Vector(0.0, 0.0, 30.0))
    lamp.point_light_component.set_intensity(4000.0)
    lamp.point_light_component.set_attenuation_radius(1200.0)
    unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, png, cam)
    log("frame queued %s from sub (%.0f %.0f %.0f) pitch %.0f yaw %.0f; remove with mode 'clear'" % (png, x, y, z, pitch, yaw))


def clear():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    n = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_location().z > FAR.z * 0.5:
            a.destroy_actor()
            n += 1
    log("cleared %d actors" % n)


def main(argv):
    mesh = unreal.load_asset(MESH)
    mode = argv[0] if argv else "slots"
    if mode == "slots":
        slots(mesh)
    elif mode == "layers":
        layers(mesh)
    elif mode == "frame":
        frame(mesh, float(argv[1]), float(argv[2]), float(argv[3]), float(argv[4]), float(argv[5]), argv[6])
    elif mode == "clear":
        clear()


if __name__ == "__main__":
    main(sys.argv[1:])
