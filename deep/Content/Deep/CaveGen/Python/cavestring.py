"""DEEP cave string tools (editor Python): CSV data, placement, joint and
overlap checks, the hull sweep, sockets and camera spots.

Chunk frame: entry joint centre at the origin, +X into the chunk, Z up; cm, degrees.
    import cavestring as cs
    rows = cs.write_chunks_csv()[1]; cs.place_string(rows); cs.joint_checks(rows)
Recipe and numbers: deep-claude-brain/03 - Research/Cave Spike Recipe.md
"""
import math
import os

import unreal

import cavegen as cg

U = unreal
GS_Q = U.GeometryScript_MeshQueries
GS_L = U.GeometryScript_List
GS_A = U.GeometryScript_AssetUtils
GS_E = U.GeometryScript_MeshEdits
GS_N = U.GeometryScript_Normals
GS_S = U.GeometryScript_MeshSpatial

STRING = ['A', 'T1', 'C1', 'B']
HULL_HALF = (615.0, 165.0, 250.0)     # 12.3 x 3.3 x 5.0 m
yaw_dirs = cg.yaw_dirs


def data_dir():
    return os.path.normpath(os.path.join(U.Paths.convert_relative_path_to_full(U.Paths.project_content_dir()), 'Data'))


def mesh_path(name):
    return '%s/SM_CaveChunk_%s.SM_CaveChunk_%s' % (cg.MESH_DIR, name, name)


_MESH_CACHE = {}


def read_mesh(name, fresh=False):
    if name in _MESH_CACHE and not fresh:
        return _MESH_CACHE[name]
    sm = U.load_asset(mesh_path(name))
    m = U.DynamicMesh()
    o = U.GeometryScriptCopyMeshFromAssetOptions()
    o.apply_build_settings = False
    GS_A.copy_mesh_from_static_mesh(sm, m, o, U.GeometryScriptMeshReadLOD())
    _MESH_CACHE[name] = m
    return m


def verts(mesh):
    res = GS_Q.get_all_vertex_positions(mesh, False)
    return GS_L.convert_vector_list_to_array(res[1])


# ---------------------------------------------------------------- CSV
CHUNK_COLS = ['Name', 'Mesh', 'Type', 'ExitX', 'ExitY', 'ExitZ', 'ExitYaw',
              'MinX', 'MinY', 'MinZ', 'MaxX', 'MaxY', 'MaxZ']
SOCKET_COLS = ['Chunk', 'Socket', 'X', 'Y', 'Z', 'Yaw']


def _fmt(v):
    if isinstance(v, float):
        s = ('%.3f' % v).rstrip('0').rstrip('.')
        return '0' if s in ('-0', '') else s
    return str(v)


def write_csv(path, cols, rows):
    with open(path, 'w', newline='\n', encoding='utf-8') as f:
        f.write(','.join(cols) + '\n')
        for r in rows:
            f.write(','.join(_fmt(r[c]) for c in cols) + '\n')


def read_csv(path):
    with open(path, encoding='utf-8') as f:
        lines = [l.strip() for l in f if l.strip()]
    cols = lines[0].split(',')
    rows = []
    for l in lines[1:]:
        r = dict(zip(cols, l.split(',')))
        for k, v in r.items():
            try:
                r[k] = float(v)
            except ValueError:
                pass
        rows.append(r)
    return rows


def chunk_record(name):
    spec = cg.SPECS[name]()
    bb = GS_Q.get_mesh_bounding_box(read_mesh(name, fresh=True))
    ex = spec['exit']
    return {'Name': name, 'Mesh': mesh_path(name), 'Type': spec['type'],
            'ExitX': float(ex[0]), 'ExitY': float(ex[1]), 'ExitZ': float(ex[2]), 'ExitYaw': float(ex[3]),
            'MinX': round(bb.min.x, 1), 'MinY': round(bb.min.y, 1), 'MinZ': round(bb.min.z, 1),
            'MaxX': round(bb.max.x, 1), 'MaxY': round(bb.max.y, 1), 'MaxZ': round(bb.max.z, 1)}


def write_chunks_csv():
    rows = [chunk_record(n) for n in ['A', 'B', 'T1', 'C1']]
    p = os.path.join(data_dir(), 'CaveChunks.csv')
    write_csv(p, CHUNK_COLS, rows)
    return p, rows


def load_chunks():
    return read_csv(os.path.join(data_dir(), 'CaveChunks.csv'))


# ---------------------------------------------------------------- frames
def compose(place, local):
    """place = (x, y, z, yaw) of a chunk's entry frame; local point or frame -> world."""
    px, py, pz, pyaw = place
    (fx, fy), (rx, ry) = yaw_dirs(pyaw)
    lx, ly, lz = local[0], local[1], local[2]
    w = (px + fx * lx + rx * ly, py + fy * lx + ry * ly, pz + lz)
    if len(local) > 3:
        return w + (pyaw + local[3],)
    return w


def to_local(place, p):
    px, py, pz, pyaw = place
    (fx, fy), (rx, ry) = yaw_dirs(pyaw)
    dx, dy, dz = p[0] - px, p[1] - py, p[2] - pz
    return (dx * fx + dy * fy, dx * rx + dy * ry, dz)


def string_places(rows, names=STRING):
    """World entry frame of each chunk: P_next = P + Rz(yaw) * exit, yaw_next = yaw + exit yaw."""
    byname = {r['Name']: r for r in rows}
    place = (0.0, 0.0, 0.0, 0.0)
    out = []
    for n in names:
        r = byname[n]
        out.append((n, place))
        place = compose(place, (r['ExitX'], r['ExitY'], r['ExitZ'], r['ExitYaw']))
    return out, place


def place_string(rows):
    """Remove the old chunk actors and spawn the string from the CSV rows (by script)."""
    eas = U.get_editor_subsystem(U.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith('Chunk_'):
            eas.destroy_actor(a)
    byname = {r['Name']: r for r in rows}
    places, end = string_places(rows)
    labels = []
    for i, (n, pl) in enumerate(places):
        sm = U.load_asset(byname[n]['Mesh'])
        a = eas.spawn_actor_from_object(sm, U.Vector(pl[0], pl[1], pl[2]), U.Rotator(roll=0.0, pitch=0.0, yaw=pl[3]))
        a.set_actor_label('Chunk_%d_%s' % (i, n))
        a.set_folder_path('CaveString')
        labels.append((a.get_actor_label(), tuple(round(c, 3) for c in pl)))
    return places, end, labels


def chunk_actors():
    eas = U.get_editor_subsystem(U.EditorActorSubsystem)
    return [a for a in eas.get_all_level_actors() if a.get_actor_label().startswith('Chunk_')]


# ---------------------------------------------------------------- joint checks
def _loop(mesh, centre, fwd, tol=0.5):
    """Vertices on a joint plane, sorted by angle about the joint axis."""
    cx, cy, cz = centre
    fx, fy = fwd
    lp = []
    for p in verts(mesh):
        dx, dy, dz = p.x - cx, p.y - cy, p.z - cz
        if abs(dx * fx + dy * fy) <= tol:
            lat = -dx * fy + dy * fx
            r = math.hypot(lat, dz)
            if r > 50:
                lp.append((math.atan2(dz, lat), r, (p.x, p.y, p.z)))
    lp.sort()
    return lp


def _seg_dist(p, a, b):
    ab = [b[i] - a[i] for i in range(3)]
    ap = [p[i] - a[i] for i in range(3)]
    l2 = sum(c * c for c in ab)
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, sum(ap[i] * ab[i] for i in range(3)) / l2))
    return math.sqrt(sum((p[i] - a[i] - ab[i] * t) ** 2 for i in range(3)))


def joint_checks(rows):
    """Radius of every joint loop, and the gap between each pair of loops that meet."""
    byname = {r['Name']: r for r in rows}
    places, end = string_places(rows)
    loops, rep = {}, []
    rmin, rmax = 1e9, 0.0
    for n in sorted(set(STRING)):
        r = byname[n]
        (fx, fy), _ = yaw_dirs(r['ExitYaw'])
        m = read_mesh(n)
        ent = _loop(m, (0, 0, 0), (1, 0))
        exl = _loop(m, (r['ExitX'], r['ExitY'], r['ExitZ']), (fx, fy))
        loops[n] = (ent, exl)
        for tag, lp in (('entry', ent), ('exit', exl)):
            rs = [x[1] for x in lp]
            angs = [x[0] for x in lp]
            gaps = [angs[i + 1] - angs[i] for i in range(len(angs) - 1)] + [angs[0] + 2 * math.pi - angs[-1]]
            rmin, rmax = min(rmin, min(rs)), max(rmax, max(rs))
            rep.append('%s %s: %d verts, r %.2f..%.2f, widest angle step %.1f deg' % (
                n, tag, len(lp), min(rs), max(rs), math.degrees(max(gaps))))
    worst = 0.0
    for i in range(len(places) - 1):
        (na, pa), (nb, pb) = places[i], places[i + 1]
        wa = [compose(pa, x[2]) for x in loops[na][1]]
        wb = [compose(pb, x[2]) for x in loops[nb][0]]

        def gap(src, dst):
            nd = len(dst)
            return max(min(_seg_dist(p, dst[k], dst[(k + 1) % nd]) for k in range(nd)) for p in src)
        g = max(gap(wa, wb), gap(wb, wa))
        worst = max(worst, g)
        rep.append('joint %s->%s: max gap %.3f cm' % (na, nb, g))
    return {'radius': (round(rmin, 2), round(rmax, 2)), 'worst_gap_cm': round(worst, 3)}, rep


# ---------------------------------------------------------------- overlap check
def overlap_checks(rows, stride=4, joint_clear=300.0):
    """AABB test for every pair; for pairs whose boxes touch, test sampled wall
    vertices of each against the other's void (winding number), away from joints."""
    byname = {r['Name']: r for r in rows}
    places, end = string_places(rows)
    water = {}
    for n in set(STRING):
        om = U.DynamicMesh()
        GS_E.append_mesh(om, read_mesh(n), U.Transform())
        GS_N.flip_normals(om)            # outward: the void as a solid
        water[n] = (om, GS_S.build_bvh_for_mesh(om)[1])

    def world_box(n, pl):
        r = byname[n]
        xs, ys = [], []
        for x in (r['MinX'], r['MaxX']):
            for y in (r['MinY'], r['MaxY']):
                w = compose(pl, (x, y, 0))
                xs.append(w[0]); ys.append(w[1])
        return (min(xs), min(ys), pl[2] + r['MinZ']), (max(xs), max(ys), pl[2] + r['MaxZ'])
    joints = [places[0][1][:3]] + [p[1][:3] for p in places[1:]] + [end[:3]]
    qo = U.GeometryScriptSpatialQueryOptions()
    rep, bad = [], 0
    for i in range(len(places)):
        for j in range(i + 1, len(places)):
            (na, pa), (nb, pb) = places[i], places[j]
            ba, bb_ = world_box(na, pa), world_box(nb, pb)
            if any(ba[1][k] < bb_[0][k] or bb_[1][k] < ba[0][k] for k in range(3)):
                rep.append('%s/%s: bounds apart' % (na, nb))
                continue
            inside = tested = 0
            for (src, ps, dst, pd) in ((na, pa, nb, pb), (nb, pb, na, pa)):
                om, bvh = water[dst]
                pts = verts(read_mesh(src))
                for k in range(0, len(pts), stride):
                    w = compose(ps, (pts[k].x, pts[k].y, pts[k].z))
                    if min(math.dist(w, jt) for jt in joints) < joint_clear + cg.JOINT_R:
                        continue
                    tested += 1
                    if GS_S.is_point_inside_mesh(om, bvh, U.Vector(*to_local(pd, w)), qo)[1]:
                        inside += 1
            bad += inside
            rep.append('%s/%s: bounds touch; %d of %d sampled wall points inside the other void' % (na, nb, inside, tested))
    return bad, rep


# ---------------------------------------------------------------- traces
def world():
    return U.get_editor_subsystem(U.UnrealEditorSubsystem).get_editor_world()


CH = U.TraceTypeQuery.TRACE_TYPE_QUERY1     # Visibility


def line(a, b):
    """First blocking hit from a to b: (point, normal, actor label) or None."""
    h = U.SystemLibrary.line_trace_single(world(), U.Vector(*a), U.Vector(*b), CH, True, [], U.DrawDebugTrace.NONE)
    if h is None:
        return None
    t = h.to_tuple()
    if not t[0]:
        return None
    p, n = t[5], t[7]
    act = t[9]
    return (p.x, p.y, p.z), (n.x, n.y, n.z), act.get_actor_label() if act else None


def in_void(name, local_pt):
    om = U.DynamicMesh()
    GS_E.append_mesh(om, read_mesh(name), U.Transform())
    GS_N.flip_normals(om)
    bvh = GS_S.build_bvh_for_mesh(om)[1]
    return GS_S.is_point_inside_mesh(om, bvh, U.Vector(*local_pt), U.GeometryScriptSpatialQueryOptions())[1]


# ---------------------------------------------------------------- C1 sockets
def c1_sockets(rows):
    """Park, Salvage (by trace onto ledges, floor and pile) and Serpent, in C1's frame."""
    places, end = string_places(rows)
    pl = dict(places)['C1']
    spec = cg.spec_C1()
    cx, cy = cg.C1_COL
    park = spec['sockets']['Park']
    rep = []
    # keel-to-floor gap under Park
    pw = compose(pl, park[:3])
    h = line(pw, (pw[0], pw[1], pw[2] - 3000))
    floor_z = to_local(pl, h[0])[2]
    keel = park[2] - cg.HULL_HALF_Z if hasattr(cg, 'HULL_HALF_Z') else park[2] - HULL_HALF[2]
    rep.append('Park keel %.0f floor %.1f gap %.1f cm' % (keel, floor_z, keel - floor_z))
    # salvage candidates: (label, local xy, start z)
    cands = []
    for k, (az, ztop, wid) in enumerate(spec['ledges']):
        (ax, ay), _ = yaw_dirs(az)
        r = cg.C1_COL_R + 450.0
        cands.append(('ledge%d' % int(az), (cx + ax * r, cy + ay * r), ztop + 250.0))
    cands.append(('floor', (cx + 650.0, cy + 450.0), park[2]))
    cands.append(('pile', spec['pile'], park[2]))
    cands.append(('pocket', (cx - 800.0, 1000.0), park[2]))
    sal = []
    for lab, (x, y), z0 in cands:
        start_l = (x, y, z0)
        if not in_void('C1', start_l):
            rep.append('%s: start in rock, skipped' % lab)
            continue
        sw = compose(pl, start_l)
        h = line(sw, (sw[0], sw[1], sw[2] - 2500))
        if not h or h[1][2] < 0.5:
            rep.append('%s: no floor-like hit' % lab)
            continue
        hl = to_local(pl, h[0])
        d = math.dist(hl, park[:3])
        if d > 2000.0:
            rep.append('%s: %.0f cm from Park, too far' % (lab, d))
            continue
        sal.append((lab, (hl[0], hl[1], hl[2] + 20.0), d, h[1][2]))
    sal = sal[:5]
    out = [{'Chunk': 'C1', 'Socket': 'Park', 'X': park[0], 'Y': park[1], 'Z': park[2], 'Yaw': park[3]}]
    for i, (lab, p, d, nz) in enumerate(sal):
        out.append({'Chunk': 'C1', 'Socket': 'Salvage%d' % (i + 1), 'X': round(p[0], 1), 'Y': round(p[1], 1),
                    'Z': round(p[2], 1), 'Yaw': float(round((i * 73) % 360))})
        rep.append('Salvage%d (%s): %.0f cm from Park, surface nz %.2f' % (i + 1, lab, d, nz))
    sp = spec['sockets']['Serpent']
    ok = in_void('C1', sp[:3])
    # how enclosed is the pocket: distance to rock in six directions
    sw = compose(pl, sp[:3])
    dists = []
    for dv in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)):
        h = line(sw, (sw[0] + dv[0] * 4000, sw[1] + dv[1] * 4000, sw[2] + dv[2] * 4000))
        dists.append(round(math.dist(h[0], sw)) if h else -1)
    rep.append('Serpent in void %s, rock at +-X +-Y +-Z (world) %s cm' % (ok, dists))
    out.append({'Chunk': 'C1', 'Socket': 'Serpent', 'X': sp[0], 'Y': sp[1], 'Z': sp[2], 'Yaw': sp[3]})
    return out, rep


def write_sockets_csv(rows):
    srows, rep = c1_sockets(rows)
    p = os.path.join(data_dir(), 'CaveChunkSockets.csv')
    write_csv(p, SOCKET_COLS, srows)
    return p, srows, rep


# ---------------------------------------------------------------- the dive path and the hull sweep
def dive_path(rows, step=25.0, turn_step=2.0):
    """World samples (x, y, z, yaw) of the hull centre: level and nose first in tunnels,
    a sink and an in-place turn in C1."""
    places = dict(string_places(rows)[0])
    byname = {r['Name']: r for r in rows}
    pts = []

    def run(pl, a, b, z=0.0, yaw=0.0, x0=0.0, y0=0.0):
        L = math.dist(a, b)
        n = max(1, int(math.ceil(L / step)))
        for i in range(n + 1):
            t = i / n
            pts.append(compose(pl, (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, yaw)))
    # A: straight
    run(places['A'], (0, 0, 0), (byname['A']['ExitX'], 0, 0))
    # T1: its centreline
    p = cg.t1_path()
    n = int(math.ceil(p.length / step))
    for i in range(n + 1):
        x, y, z, yaw = p.at(p.length * i / n)
        pts.append(compose(places['T1'], (x, y, z, yaw)))
    # C1: in to the column, sink, turn, out
    pl = places['C1']
    cx, cy = cg.C1_COL
    run(pl, (0, 0, 0), (cx, cy, 0))
    run(pl, (cx, cy, 0), (cx, cy, cg.C1_PARK_Z))
    k = int(abs(cg.C1_EXIT_YAW) / turn_step)
    for i in range(1, k + 1):
        pts.append(compose(pl, (cx, cy, cg.C1_PARK_Z, cg.C1_EXIT_YAW * i / k)))
    ex = cg.c1_exit()
    run(pl, (cx, cy, cg.C1_PARK_Z), ex[:3], yaw=cg.C1_EXIT_YAW)
    # B: straight
    run(places['B'], (0, 0, 0), (byname['B']['ExitX'], 0, 0))
    return pts


def _hits(a, b, yaw, half):
    hs = U.SystemLibrary.box_trace_multi(world(), U.Vector(*a), U.Vector(*b), U.Vector(*half),
                                         U.Rotator(roll=0.0, pitch=0.0, yaw=yaw), CH, True, [], U.DrawDebugTrace.NONE)
    return [h for h in (hs or []) if h.to_tuple()[0] or h.to_tuple()[1]]


def clearance(p, lo=0.0, hi=200.0, iters=7):
    """Largest margin (cm) the hull box can grow by at p without touching rock."""
    a = p[:3]
    b = (a[0] + 0.5, a[1], a[2])
    if _hits(a, b, p[3], HULL_HALF):
        return -1.0
    for _ in range(iters):
        m = 0.5 * (lo + hi)
        half = (HULL_HALF[0] + m, HULL_HALF[1] + m, HULL_HALF[2] + m)
        if _hits(a, b, p[3], half):
            hi = m
        else:
            lo = m
    return lo


def sweep(pts, offset=0.0, clear_every=4):
    """Box sweeps between consecutive samples. offset moves the path sideways (control)."""
    if offset:
        q = []
        for x, y, z, yaw in pts:
            _, (rx, ry) = yaw_dirs(yaw)
            q.append((x + rx * offset, y + ry * offset, z, yaw))
        pts = q
    contacts = 0
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        yaw = b[3]
        if a[:3] == b[:3]:
            b = (a[0] + 0.5, a[1], a[2], yaw)
        if _hits(a[:3], b[:3], yaw, HULL_HALF):
            contacts += 1
    mins = []
    if clear_every:
        for i in range(0, len(pts), clear_every):
            mins.append((clearance(pts[i]), i))
        mins.sort()
    return contacts, len(pts) - 1, mins[:5]


def column_check(rows, dirs=36, dz=100.0):
    """Narrowest clear diameter of C1's sink column, from the entry level to Park."""
    pl = dict(string_places(rows)[0])['C1']
    cx, cy = cg.C1_COL
    worst = (1e9, None)
    z = 0.0
    while z >= cg.C1_PARK_Z - HULL_HALF[2]:
        c = compose(pl, (cx, cy, z))
        rs = []
        for k in range(dirs):
            a = 2 * math.pi * k / dirs
            e = (c[0] + 4000 * math.cos(a), c[1] + 4000 * math.sin(a), c[2])
            h = line(c, e)
            rs.append(math.dist(c, h[0]) if h else 4000.0)
        # clear diameter: the narrowest pair of opposite rays
        d = min(rs[k] + rs[(k + dirs // 2) % dirs] for k in range(dirs // 2))
        if d < worst[0]:
            worst = (d, z)
        z -= dz
    return round(worst[0], 1), worst[1]
