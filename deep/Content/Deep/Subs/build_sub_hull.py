# BT-01's hull tools. They need a full editor (a renderer, the editor subsystems and an editor world), so run them in
# one, inside the DeepUnrealSlot mutex: UnrealEditor.exe deep\Deep.uproject -ExecutePythonScript="<this file> <mode>"
# (then quit), or execute_python through tools\mcp.ps1 in an open editor. A -run=pythonscript commandlet has none of these.
# Modes:
#   [hulls] [verts] [precision] [measure]  gives SM_BT01 its simple collision with the editor's own convex decomposition
#                                          (saved into the asset; "measure" skips it), then measures the fit: traces from
#                                          the four sides, above and below, simple collision against the per-triangle surface.
#   parts per_part tol max                 Geometry Script decomposition part by part, capped at max shapes
#   merged per_part tol merge max [drop] [open cm]
#                                          what SM_BT01 holds: "merged 512 1.5 2 2048 drop open 4". Every part decomposed
#                                          (13205 hulls), then merged down to max; drop = THIN_PARTS get no hulls; open =
#                                          merges keep open water of that radius (mesh cm). Runs testgrid after.
#   testgrid                               Deep.World.SubHullShape's ray grid in Python, logging each bad ray and its part
#   diag                                   section collision flags; the nearest source triangle at DIAG_POINTS
#   profile                                cross sections of the visible surface along the hull (sub-local cm)
#   probe name sx sy sz ex ey ez ...       per-triangle traces along sub-local rays: where the lamps go (DeepHull::ModelLamp)
#   bulb                                   builds M_SubBulb, the lamps' glowing glass
# Prints one line per result, prefixed "SUBCOL". The C++ tests Deep.World.SubHullShape and SubModelLamps check the same.
import sys
import unreal

MESH = "/Game/Deep/Subs/SM_BT01"
ENVELOPE = (1230.0, 330.0, 500.0)  # DeepHull::ModelEnvelopeCm: the fit's uniform scale makes the model this size
FAR = unreal.Vector(0.0, 0.0, 2000000.0)  # measured far from anything else in the editor world

# The hull count is the one knob that matters; the others are the editor dialog's maximums for a tight fit.
HULLS = 32
VERTS = 32
PRECISION = 1000000


def log(msg):
    unreal.log("SUBCOL " + msg)
    print("SUBCOL " + msg)


def decompose(mesh, hulls, verts, precision):
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    ok = sub.set_convex_decomposition_collisions(mesh, hulls, verts, precision)
    # The default trace (the game's) uses the hulls; per-triangle traces still see the render surface.
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    log("decompose hulls=%d verts=%d precision=%d ok=%s convex=%d" % (hulls, verts, precision, ok, sub.get_convex_collision_count(mesh)))


def decompose_parts(mesh, per_part, tolerance_cm, max_shapes):
    """Geometry Script's convex decomposition: each connected part of the model on its own, split until the volume error
    is under tolerance_cm (mesh-local cm; the fit scales it by about 1.9), the largest max_shapes shapes kept. It follows
    the deck's hollows and the gunwale far closer than the voxel decomposition, which bridges them."""
    dm = unreal.DynamicMesh()
    read = unreal.GeometryScriptMeshReadLOD()
    read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    read.lod_index = 0
    res = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
        mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read, True)
    log("copied source model: %s triangles" % dm.get_triangle_count() if hasattr(dm, "get_triangle_count") else str(res))
    opt = unreal.GeometryScriptCollisionFromMeshOptions()
    opt.emit_transaction = False
    opt.method = unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS
    opt.max_convex_hulls_per_mesh = per_part
    opt.convex_decomposition_error_tolerance = tolerance_cm
    opt.convex_decomposition_search_factor = 0.5
    opt.convex_hull_target_face_count = 48
    opt.remove_fully_contained_shapes = True
    opt.max_shape_count = max_shapes
    keep = unreal.GeometryScriptSetStaticMeshCollisionOptions()
    keep.mark_as_customized = True
    unreal.GeometryScript_Collision.set_static_mesh_collision_from_mesh(dm, mesh, opt, keep)
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    log("decompose parts per_part=%d tolerance=%.1f max_shapes=%d convex=%d" % (
        per_part, tolerance_cm, max_shapes, sub.get_convex_collision_count(mesh)))


def decompose_merged(mesh, per_part, tolerance_cm, merge_cm, max_shapes, drop=(), keep_open_cm=0.0):
    """As decompose_parts, but no shape is dropped: every part is decomposed, then neighbouring shapes are merged (the
    cheapest merges first) down to max_shapes, so small parts join their neighbours instead of losing their collision."""
    dm = unreal.DynamicMesh()
    read = unreal.GeometryScriptMeshReadLOD()
    read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    read.lod_index = 0
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
        mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read, True)
    if drop:
        # Thin parts the swimmer may brush through get no hulls: their triangles leave the copy before decomposing.
        names = [m.material_interface.get_name() if m.material_interface else "" for m in mesh.static_materials]
        removed = 0
        for index, name in enumerate(names):
            if any(p in name for p in drop):
                sel = unreal.GeometryScript_MeshSelection.select_mesh_elements_by_material_id(
                    dm, index, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
                if isinstance(sel, tuple):
                    sel = sel[-1] if not isinstance(sel[0], unreal.GeometryScriptMeshSelection) else sel[0]
                res = unreal.GeometryScript_MeshEdits.delete_selected_triangles_from_mesh(dm, sel)
                n = res[-1] if isinstance(res, tuple) else 0
                removed += n
                log("left out %s (slot %d): %s triangles" % (name, index, n))
        log("left out %d triangles of thin parts" % removed)
    lib = unreal.GeometryScript_Collision
    opt = unreal.GeometryScriptCollisionFromMeshOptions()
    opt.emit_transaction = False
    opt.method = unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS
    opt.max_convex_hulls_per_mesh = per_part
    opt.convex_decomposition_error_tolerance = tolerance_cm
    opt.convex_decomposition_search_factor = 0.5
    opt.convex_hull_target_face_count = 48
    opt.remove_fully_contained_shapes = True
    opt.max_shape_count = 0
    coll = lib.generate_collision_from_mesh(dm, opt)
    before = lib.get_simple_collision_shape_count(coll)
    merge = unreal.GeometryScriptMergeSimpleCollisionOptions()
    merge.max_shape_count = max_shapes
    merge.error_tolerance = merge_cm
    if keep_open_cm > 0.0:
        # Open water between parts stays open: spheres of at least keep_open_cm (mesh cm) that reach the outside
        # forbid any merge whose hull would fill them, so merged shapes cannot bridge the arms, skids or tower rails.
        merge.compute_negative_space = True
        neg = unreal.ComputeNegativeSpaceOptions()
        neg.sample_method = unreal.NegativeSpaceSampleMethod.VOXEL_SEARCH
        neg.only_connected_to_hull = True
        neg.max_voxels_per_dim = 256
        neg.target_num_samples = 4000
        neg.min_sample_spacing = keep_open_cm
        neg.negative_space_tolerance = 1.0
        neg.min_radius = keep_open_cm
        merge.compute_negative_space_options = neg
    merged = lib.merge_simple_collision_shapes(coll, merge)
    if isinstance(merged, tuple):
        merged = merged[0]
    after = lib.get_simple_collision_shape_count(merged)
    keep = unreal.GeometryScriptSetStaticMeshCollisionOptions()
    keep.mark_as_customized = True
    lib.set_simple_collision_of_static_mesh(merged, mesh, unreal.GeometryScriptSetSimpleCollisionOptions(emit_transaction=False), keep)
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    log("decompose merged per_part=%d tolerance=%.1f merge=%.1f max_shapes=%d: %d shapes, merged to %d, convex=%d" % (
        per_part, tolerance_cm, merge_cm, max_shapes, before, after, sub.get_convex_collision_count(mesh)))


def fit_scale(mesh):
    size = mesh.get_bounding_box().max - mesh.get_bounding_box().min
    return min(ENVELOPE[0] / size.x, ENVELOPE[1] / size.y, ENVELOPE[2] / size.z)


def trace(world, a, b, complex_):
    hit = unreal.SystemLibrary.line_trace_single(world, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, complex_, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    if not t[0]:
        return None
    return t


THIN_PARTS = ("Rope", "Propellers", "mechanical_hands", "drill")  # as Deep.World.SubHullShape: no hulls of their own
PROX_FROM = 10.0  # cm: rays whose hits differ by more than this are judged by proximity instead
PROX_STEPS = (2.5, 5.0, 7.5, 10.0, 12.5, 15.0, 17.5, 20.0, 25.0, 30.0, 40.0, 60.0, 100.0, 200.0)


def nearest(world, p, complex_):
    """The radius of the smallest sphere round p that touches the visible surface (complex_) or the hulls, cm."""
    for r in PROX_STEPS:
        h = unreal.SystemLibrary.sphere_trace_single(world, p, p + unreal.Vector(0.0, 0.0, 0.01), r,
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, complex_, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        if h is not None and h.to_tuple()[0]:
            return r
    return 999.0


def measure(mesh):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    scale = fit_scale(mesh)
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_object(mesh, FAR)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    comp = actor.static_mesh_component
    comp.set_collision_profile_name("BlockAll")
    comp.recreate_physics_state() if hasattr(comp, "recreate_physics_state") else None
    box = mesh.get_bounding_box()
    lo = FAR + box.min * scale
    hi = FAR + box.max * scale
    log("scale %.5f visible box %s .. %s (size %s)" % (scale, box.min * scale, box.max * scale, (box.max - box.min) * scale))
    origin, extent = actor.get_actor_bounds(False)
    log("actor bounds origin %s extent %s" % (origin - FAR, extent))

    worst = []
    excluded = 0
    n = 0
    miss_simple = 0
    miss_complex = 0
    devs = []
    steps = 24
    reach = 300.0
    for axis in range(3):
        for sign in (-1.0, 1.0):
            a1, a2 = [i for i in range(3) if i != axis]
            for i in range(steps + 1):
                for j in range(steps + 1):
                    p = [0.0, 0.0, 0.0]
                    p[a1] = lo.to_tuple()[a1] + (hi.to_tuple()[a1] - lo.to_tuple()[a1]) * (i + 0.5) / (steps + 1)
                    p[a2] = lo.to_tuple()[a2] + (hi.to_tuple()[a2] - lo.to_tuple()[a2]) * (j + 0.5) / (steps + 1)
                    start = list(p)
                    end = list(p)
                    start[axis] = (hi.to_tuple()[axis] + reach) if sign > 0 else (lo.to_tuple()[axis] - reach)
                    end[axis] = (FAR.to_tuple()[axis] + (box.min.to_tuple()[axis] + box.max.to_tuple()[axis]) * 0.5 * scale)
                    s = unreal.Vector(*start)
                    e = unreal.Vector(*end)
                    hs = trace(world, s, e, False)
                    hc = trace(world, s, e, True)
                    if hs is None and hc is None:
                        continue
                    slot = ""
                    if hc is not None:
                        try:
                            mat, section = comp.get_material_from_collision_face_index(hc[15])
                            slot = mat.get_name() if mat else ""
                        except Exception:
                            slot = "?"
                    if any(t in slot for t in THIN_PARTS):
                        excluded += 1
                        continue
                    n += 1
                    if hs is None:
                        miss_simple += 1
                    elif hc is None:
                        miss_complex += 1
                    dev = abs(hs[3] - hc[3]) if hs is not None and hc is not None else 999.0
                    if dev > PROX_FROM:
                        # The rays part ways (a hull bridging a hollow, or a part with no hull): how far is the other surface?
                        dev = 0.0
                        if hs is not None:
                            dev = max(dev, nearest(world, hs[5], True))
                        if hc is not None:
                            dev = max(dev, nearest(world, hc[5], False))
                    devs.append(dev)
                    hit = hc if hc is not None else hs
                    q = to_sub(mesh, (hit[5] - FAR) * (1.0 / scale), scale)
                    worst.append((dev, "dir %s%s at sub (%.0f %.0f %.0f) slot %s simple %s complex %s" % (
                        "+" if sign > 0 else "-", "xyz"[axis], q.x, q.y, q.z, slot,
                        "%.1f" % hs[3] if hs else "miss", "%.1f" % hc[3] if hc else "miss")))
    devs.sort()
    worst.sort(key=lambda w: -w[0])
    if devs:
        over = sum(1 for d in devs if d > 15.0)
        log("traces %d (thin parts excluded %d), hulls-only %d, surface-only %d, median %.1f, p90 %.1f, p95 %.1f, p99 %.1f, max %.1f cm, over 15 cm %d" % (
            n, excluded, miss_complex, miss_simple, devs[len(devs) // 2], devs[int(len(devs) * 0.9)], devs[int(len(devs) * 0.95)],
            devs[int(len(devs) * 0.99)], devs[-1], over))
    by_slot = {}
    for dev, text in worst:
        if dev > 15.0:
            slot = text.split(" slot ")[1].split(" ")[0] or "(hulls only)"
            by_slot[slot] = by_slot.get(slot, 0) + 1
    log("over 15 cm by part: %s" % ", ".join("%s %d" % kv for kv in sorted(by_slot.items(), key=lambda kv: -kv[1])))
    for w in worst[:40]:
        log("worst %.1f %s" % w)
    actor.destroy_actor()


HATCH_XY = (45.5, 0.0)  # DeepHull::ModelHatchLocalCm
OUTER_Z = 170.0  # the room box's outer half height: the hatch sits at the belly, z -OUTER_Z


def to_sub(mesh, p_mesh, scale):
    """Mesh-local cm -> sub-local cm, the same placement as DeepHull::ModelTransform."""
    mn = mesh.get_bounding_box().min
    return unreal.Vector((p_mesh.x - HATCH_XY[0]) * scale, (p_mesh.y - HATCH_XY[1]) * scale, (p_mesh.z - mn.z) * scale - OUTER_Z)


def to_mesh(mesh, p_sub, scale):
    mn = mesh.get_bounding_box().min
    return unreal.Vector(p_sub.x / scale + HATCH_XY[0], p_sub.y / scale + HATCH_XY[1], (p_sub.z + OUTER_Z) / scale + mn.z)


def slot_of(comp, hit):
    try:
        mat, _ = comp.get_material_from_collision_face_index(hit[15])
        return mat.get_name() if mat else ""
    except Exception:
        return "?"


def touches(world, p, r, complex_):
    h = unreal.SystemLibrary.sphere_trace_single(world, p, p + unreal.Vector(0.0, 0.0, 0.1), r,
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, complex_, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return h is not None and h.to_tuple()[0]


def testgrid(mesh):
    """Deep.World.SubHullShape's model check, ray for ray: its grid, its 20 cm gap, its grazing rule. Logs each bad ray
    with the part it meets, the slots of the materials, and the far ends of the visible surface and of the hulls."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    scale = fit_scale(mesh)
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_object(mesh, FAR)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    comp = actor.static_mesh_component
    comp.set_collision_profile_name("BlockAll")
    box = mesh.get_bounding_box()
    log("asset box %s..%s, bounds extension +%s -%s" % (box.min, box.max, mesh.get_editor_property("positive_bounds_extension"),
                                                       mesh.get_editor_property("negative_bounds_extension")))
    dm = unreal.DynamicMesh()
    read = unreal.GeometryScriptMeshReadLOD()
    read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read, True)
    log("source triangles box %s" % unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dm))
    log("slots: %s" % ", ".join(m.material_interface.get_name() if m.material_interface else "-" for m in mesh.static_materials))
    vmin = to_sub(mesh, mesh.get_bounding_box().min, scale)
    vmax = to_sub(mesh, mesh.get_bounding_box().max, scale)
    c = (vmin + vmax) * 0.5
    e = (vmax - vmin) * 0.5 + unreal.Vector(300.0, 300.0, 300.0)
    rays = []
    x = vmin.x + 30.0
    while x < vmax.x:
        z = vmin.z + 20.0
        while z < vmax.z:
            rays.append(((x, c.y + e.y, z), (x, c.y - e.y, z)))
            rays.append(((x, c.y - e.y, z), (x, c.y + e.y, z)))
            z += 40.0
        y = vmin.y + 15.0
        while y < vmax.y:
            rays.append(((x, y, c.z + e.z), (x, y, c.z - e.z)))
            rays.append(((x, y, c.z - e.z), (x, y, c.z + e.z)))
            y += 30.0
        x += 60.0
    y = vmin.y + 15.0
    while y < vmax.y:
        z = vmin.z + 20.0
        while z < vmax.z:
            rays.append(((c.x + e.x, y, z), (c.x - e.x, y, z)))
            rays.append(((c.x - e.x, y, z), (c.x + e.x, y, z)))
            z += 40.0
        y += 30.0

    def world_of(p):
        return FAR + to_mesh(mesh, unreal.Vector(*p), scale) * scale

    def sub_of(w):
        return to_sub(mesh, (w - FAR) * (1.0 / scale), scale)

    met = excluded = bad = grazing = 0
    for s, t in rays:
        ws, wt = world_of(s), world_of(t)
        g = trace(world, ws, wt, False)
        v = trace(world, ws, wt, True)
        if g is None and v is None:
            continue
        vslot = slot_of(comp, v) if v is not None else ""
        if v is not None and any(p in vslot for p in THIN_PARTS):
            excluded += 1
            continue
        met += 1
        gap = abs(g[3] - v[3]) if (g is not None and v is not None) else 1e9
        if gap <= 20.0:
            continue
        if (g is None or touches(world, g[5], 20.0, True)) and (v is None or touches(world, v[5], 20.0, False)):
            grazing += 1
            continue
        bad += 1
        gp = sub_of(g[5]) if g is not None else None
        vp = sub_of(v[5]) if v is not None else None
        log("bad ray (%.0f %.0f %.0f)->(%.0f %.0f %.0f): hulls %s, visible %s %s" % (
            s[0], s[1], s[2], t[0], t[1], t[2],
            "(%.0f %.0f %.0f)" % (gp.x, gp.y, gp.z) if gp else "miss",
            "(%.0f %.0f %.0f)" % (vp.x, vp.y, vp.z) if vp else "miss", vslot))
    log("testgrid: %d rays, %d met, %d thin, %d grazing, %d bad" % (len(rays), met, excluded, grazing, bad))

    # The ends: along each axis, the farthest point of the visible surface and of the hulls, and what is there.
    for axis, sign in ((0, -1), (0, 1), (1, -1), (1, 1), (2, -1), (2, 1)):
        best = {True: None, False: None}
        a1, a2 = [i for i in range(3) if i != axis]
        for i in range(41):
            for j in range(41):
                p = [0.0, 0.0, 0.0]
                p[a1] = vmin.to_tuple()[a1] + (vmax.to_tuple()[a1] - vmin.to_tuple()[a1]) * i / 40.0
                p[a2] = vmin.to_tuple()[a2] + (vmax.to_tuple()[a2] - vmin.to_tuple()[a2]) * j / 40.0
                s = list(p)
                t = list(p)
                s[axis] = (vmax.to_tuple()[axis] + 200.0) if sign > 0 else (vmin.to_tuple()[axis] - 200.0)
                t[axis] = c.to_tuple()[axis]
                for cx in (True, False):
                    h = trace(world, world_of(s), world_of(t), cx)
                    if h is None:
                        continue
                    q = sub_of(h[5])
                    val = q.to_tuple()[axis] * sign
                    if best[cx] is None or val > best[cx][0]:
                        best[cx] = (val, q, slot_of(comp, h) if cx else "")
        log("end %s%s: visible %s %s, hulls %s" % (
            "-+"[sign > 0], "xyz"[axis],
            "%.1f at (%.0f %.0f %.0f)" % (best[True][0] * sign, best[True][1].x, best[True][1].y, best[True][1].z) if best[True] else "-",
            best[True][2] if best[True] else "",
            "%.1f" % (best[False][0] * sign) if best[False] else "-"))
    actor.destroy_actor()


def probe(mesh, rays):
    """Per-triangle traces along sub-local rays (start, end): where each meets the visible surface, in both frames."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    scale = fit_scale(mesh)
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_object(mesh, FAR)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    mn = mesh.get_bounding_box().min
    log("centre offset: model box centre in sub-local %s" % to_sub(mesh, (mesh.get_bounding_box().min + mesh.get_bounding_box().max) * 0.5, scale))
    log("model box sub-local %s .. %s" % (to_sub(mesh, mesh.get_bounding_box().min, scale), to_sub(mesh, mesh.get_bounding_box().max, scale)))
    for name, s, e in rays:
        ws = FAR + to_mesh(mesh, s, scale) * scale
        we = FAR + to_mesh(mesh, e, scale) * scale
        h = trace(world, ws, we, True)
        if h is None:
            log("probe %s miss" % name)
            continue
        p_mesh = (h[5] - FAR) * (1.0 / scale)
        log("probe %s sub %s normal %s mesh %s" % (name, to_sub(mesh, p_mesh, scale), h[7], p_mesh))
    actor.destroy_actor()


def profile(mesh):
    """Cross sections of the visible surface (sub-local cm): top, bottom and half widths at stations along the hull."""
    rays = []
    for x in range(-700, 701, 50):
        rays.append(("top x%d" % x, unreal.Vector(x, 0, 600), unreal.Vector(x, 0, -400)))
        rays.append(("bot x%d" % x, unreal.Vector(x, 0, -400), unreal.Vector(x, 0, 600)))
        for z in (-150, -100, -50, 0, 50, 100, 150, 200, 250):
            rays.append(("stbd x%d z%d" % (x, z), unreal.Vector(x, 400, z), unreal.Vector(x, 0, z)))
    for z in range(-160, 330, 20):
        rays.append(("bow z%d" % z, unreal.Vector(900, 0, z), unreal.Vector(0, 0, z)))
        rays.append(("stern z%d" % z, unreal.Vector(-900, 0, z), unreal.Vector(0, 0, z)))
    probe(mesh, rays)


def build_bulb():
    """M_SubBulb: the glass of the hull's lamps. Unlit, emissive = Color x Glow (ADeepSubmarine sets both per lamp)."""
    path = "/Game/Deep/Subs/M_SubBulb"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mel = unreal.MaterialEditingLibrary
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_SubBulb", "/Game/Deep/Subs", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(1.0, 0.9, 0.8, 1.0))
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 250)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 20.0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 100)
    mel.connect_material_expressions(col, "RGB", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    log("bulb material saved %s" % path)


def main(argv):
    mesh = unreal.load_asset(MESH)
    if argv and argv[0] == "bulb":
        build_bulb()
        return
    if argv and argv[0] == "parts":
        # parts [per_part] [tolerance_cm] [max_shapes]
        decompose_parts(mesh, int(argv[1]) if len(argv) > 1 else 16, float(argv[2]) if len(argv) > 2 else 4.0,
                        int(argv[3]) if len(argv) > 3 else 256)
        measure(mesh)
        return
    if argv and argv[0] == "merged":
        # merged per_part tolerance_cm merge_cm max_shapes [drop] [open cm]  (drop: no hulls for THIN_PARTS;
        # open: merges keep open water of that radius, mesh cm)
        decompose_merged(mesh, int(argv[1]), float(argv[2]), float(argv[3]), int(argv[4]),
                         THIN_PARTS if "drop" in argv[5:] else (),
                         float(argv[argv.index("open") + 1]) if "open" in argv[5:] else 0.0)
        testgrid(mesh)
        return
    if argv and argv[0] == "testgrid":
        testgrid(mesh)
        return
    if argv and argv[0] == "diag":
        diag(mesh)
        return
    if argv and argv[0] == "profile":
        profile(mesh)
        return
    if argv and argv[0] == "probe":
        # probe name sx sy sz ex ey ez [name ...]
        rest = argv[1:]
        rays = []
        while len(rest) >= 7:
            v = [float(x) for x in rest[1:7]]
            rays.append((rest[0], unreal.Vector(*v[:3]), unreal.Vector(*v[3:])))
            rest = rest[7:]
        probe(mesh, rays)
        return
    hulls = int(argv[0]) if len(argv) > 0 else HULLS
    verts = int(argv[1]) if len(argv) > 1 else VERTS
    precision = int(argv[2]) if len(argv) > 2 else PRECISION
    measure_only = len(argv) > 3 and argv[3] == "measure"
    nanite = mesh.get_editor_property("nanite_settings")
    log("nanite enabled %s fallback %s" % (nanite.get_editor_property("enabled"), nanite.get_editor_property("fallback_relative_error")))
    if not measure_only:
        decompose(mesh, hulls, verts, precision)
    measure(mesh)


if __name__ == "__main__":
    main(sys.argv[1:])


DIAG_POINTS = ((387, 15, 10), (387, -83, 93), (387, -83, 67), (447, 0, 210), (130, -23, 290), (-176, -143, 50), (-273, 40, -70),
               (-513, 67, 70), (87, -4, 290), (447, -1, 10), (3, 67, -110))


def diag(mesh):
    """Why a spot fails: section collision flags, and at each spot the nearest source triangle (its slot, how far)
    and how many hull boxes hold it."""
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    names = [m.material_interface.get_name() if m.material_interface else "" for m in mesh.static_materials]
    off = [i for i in range(mesh.get_num_sections(0)) if not sub.is_section_collision_enabled(mesh, 0, i)]
    log("sections %d, collision off: %s" % (mesh.get_num_sections(0), ", ".join("%d" % i for i in off) or "none"))
    scale = fit_scale(mesh)
    dm = unreal.DynamicMesh()
    read = unreal.GeometryScriptMeshReadLOD()
    read.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read, True)
    bvh = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(dm)
    bvh = bvh[-1] if isinstance(bvh, tuple) else bvh
    for p in DIAG_POINTS:
        q = to_mesh(mesh, unreal.Vector(*p), scale)
        r = unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh(dm, bvh, q, unreal.GeometryScriptSpatialQueryOptions())
        tp = [x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, unreal.GeometryScriptTrianglePoint)]
        desc = "?"
        if tp and tp[0].valid:
            mid = unreal.GeometryScript_Materials.get_triangle_material_id(dm, tp[0].triangle_id)
            mid = mid[0] if isinstance(mid, tuple) else mid
            d = (tp[0].position - q).length() * scale
            desc = "nearest triangle %.1f cm, %s" % (d, names[mid] if 0 <= mid < len(names) else mid)
        log("spot (%d %d %d): %s" % (p[0], p[1], p[2], desc))
