"""DEEP cave chunk generator (editor Python, Geometry Script).

Builds each chunk as a closed "water" solid from swept rings, ellipsoids and
blocks, voxelises it with Geometry Script, cuts the ends on the joint planes,
snaps the ends onto the 9 m joint circle, turns the normals inward and bakes a
static mesh in /Game/Deep/CaveGen/Meshes.

Frame of every chunk: the entry joint's centre is the origin, +X runs into the
chunk, Z is up. Units are cm and degrees. Each chunk ends on a level, round,
9 m joint whose transform (position + yaw) is recorded as the chunk's exit.

Run from the editor (Python, or the MCP execute_python tool):
    import cavegen; cavegen.bake('T1')
Recipe and numbers: deep-claude-brain/03 - Research/Cave Spike Recipe.md
"""
import math
import random
import time

import unreal

U = unreal
GS_P = U.GeometryScript_Primitives
GS_E = U.GeometryScript_MeshEdits
GS_Q = U.GeometryScript_MeshQueries
GS_V = U.GeometryScript_MeshVoxelProcessing
GS_B = U.GeometryScript_MeshBooleans
GS_N = U.GeometryScript_Normals
GS_R = U.GeometryScript_MeshRepair
GS_L = U.GeometryScript_List
GS_UV = U.GeometryScript_UVs
GS_A = U.GeometryScript_AssetUtils
GS_NA = U.GeometryScript_NewAssetUtils

JOINT_R = 450.0       # joint radius, cm (9 m round neck)
SNAP = 150.0          # snap zone at each joint, cm
EXT = 300.0           # straight extension past each joint plane before the cut
RING_STEP = 50.0      # ring spacing along a path, cm
RING_N = 64           # points per ring
MESH_DIR = '/Game/Deep/CaveGen/Meshes'
MAT_GREY = '/Game/Deep/CaveGen/Materials/M_CaveGrey'


# ---------------------------------------------------------------- math helpers
def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def sstep(a, b, x):
    if b == a:
        return 1.0 if x >= b else 0.0
    return smooth((x - a) / (b - a))


def _h(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 1440662683 + seed * 2246822519) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    h ^= h >> 16
    return (h & 0xffff) / 32767.5 - 1.0


def vnoise(x, y, z, seed):
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - ix, y - iy, z - iz
    ux, uy, uz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    ix, iy, iz = int(ix), int(iy), int(iz)
    c000 = _h(ix, iy, iz, seed); c100 = _h(ix + 1, iy, iz, seed)
    c010 = _h(ix, iy + 1, iz, seed); c110 = _h(ix + 1, iy + 1, iz, seed)
    c001 = _h(ix, iy, iz + 1, seed); c101 = _h(ix + 1, iy, iz + 1, seed)
    c011 = _h(ix, iy + 1, iz + 1, seed); c111 = _h(ix + 1, iy + 1, iz + 1, seed)
    x00 = c000 + (c100 - c000) * ux; x10 = c010 + (c110 - c010) * ux
    x01 = c001 + (c101 - c001) * ux; x11 = c011 + (c111 - c011) * ux
    y0 = x00 + (x10 - x00) * uy; y1 = x01 + (x11 - x01) * uy
    return y0 + (y1 - y0) * uz


def fbm(x, y, z, seed, base=700.0):
    """Three octaves of value noise, roughly in [-1, 1]."""
    s, a, tot, f = 0.0, 1.0, 0.0, 1.0 / base
    for o in range(3):
        s += a * vnoise(x * f, y * f, z * f, seed + 17 * o)
        tot += a
        a *= 0.5
        f *= 2.13
    return s / tot * 1.6


def yaw_dirs(yaw_deg):
    r = math.radians(yaw_deg)
    c, s = math.cos(r), math.sin(r)
    return (c, s), (-s, c)   # forward, right (both horizontal)


# ---------------------------------------------------------------- paths
class Path:
    """A horizontal centreline made of straights and arcs, plus z(s)."""

    def __init__(self, x=0.0, y=0.0, yaw=0.0):
        self.segs = []
        self.x0, self.y0, self.yaw0 = x, y, yaw
        self.length = 0.0
        self.zf = lambda s: 0.0

    def straight(self, L):
        self.segs.append(('S', L, 0.0)); self.length += L; return self

    def arc(self, R, ang_deg):
        L = abs(math.radians(ang_deg)) * R
        self.segs.append(('A', L, ang_deg / L if L else 0.0)); self.length += L; return self

    def at(self, s):
        """(x, y, z, yaw) at arc length s. s < 0 or > length extends straight."""
        x, y, yaw = self.x0, self.y0, self.yaw0
        rem = s
        if rem < 0:
            (fx, fy), _ = yaw_dirs(yaw)
            return (x + fx * rem, y + fy * rem, self.zf(0.0), yaw)
        for kind, L, k in self.segs:
            t = min(rem, L)
            if kind == 'S':
                (fx, fy), _ = yaw_dirs(yaw)
                x += fx * t; y += fy * t
            else:
                # integrate the arc analytically
                R = 1.0 / math.radians(abs(k)) if k else 1e9
                sgn = 1.0 if k > 0 else -1.0
                da = k * t
                (fx, fy), (rx, ry) = yaw_dirs(yaw)
                cx, cy = x + rx * R * sgn, y + ry * R * sgn
                a0 = math.atan2(y - cy, x - cx)
                a1 = a0 + math.radians(da)
                x, y = cx + R * math.cos(a1), cy + R * math.sin(a1)
                yaw += da
            rem -= t
            if rem <= 1e-6:
                break
        if rem > 1e-6:
            (fx, fy), _ = yaw_dirs(yaw)
            x += fx * rem; y += fy * rem
        return (x, y, self.zf(min(max(s, 0.0), self.length)), yaw)


def keyed(keys, s):
    """Smoothstep interpolation of tuples keyed by s."""
    if s <= keys[0][0]:
        return keys[0][1:]
    for i in range(len(keys) - 1):
        a, b = keys[i], keys[i + 1]
        if s <= b[0]:
            t = smooth((s - a[0]) / (b[0] - a[0]))
            return tuple(a[j] + (b[j] - a[j]) * t for j in range(1, len(a)))
    return keys[-1][1:]


# ---------------------------------------------------------------- shape buffers
class Shapes:
    """Collects positive (water) and negative (rock) closed shapes."""

    def __init__(self, seed):
        self.pos = []   # list of (verts, tris)
        self.neg = []
        self.seed = seed

    def add(self, vt, negative=False):
        (self.neg if negative else self.pos).append(vt)


def tube(path, prof, s0, s1, amp_fn, seed, bed=0.0, n=RING_N, step=RING_STEP):
    """Closed tube along path from s0 to s1.

    prof(s) -> (ry, rz, floor_depth); amp_fn(p, s) -> noise amplitude in cm.
    Rings stand vertical (yaw-only frames), so a level hull meets level joints.
    """
    rings = []
    nseg = max(2, int(math.ceil((s1 - s0) / step)))
    for i in range(nseg + 1):
        s = s0 + (s1 - s0) * i / nseg
        x, y, z, yaw = path.at(s)
        (fx, fy), (rx, ry_) = yaw_dirs(yaw)
        wy, wz, fd = prof(s)
        ring = []
        for j in range(n):
            th = 2 * math.pi * j / n
            u, v = math.cos(th) * wy, math.sin(th) * wz
            if v < -fd:
                v = -fd
            px, py, pz = x + rx * u, y + ry_ * u, z + v
            a = amp_fn((px, py, pz), s)
            k = 1.0
            if a < 0:                       # negative amplitude: outward-only noise
                rad = math.hypot(u, v)
                d = abs(a * fbm(px, py, pz, seed))
                k = (rad + d) / rad if rad > 1 else 1.0
            elif a > 0:
                d = a * fbm(px, py, pz, seed)
                nz = abs(math.sin(th))
                if bed > 0 and nz < 0.75:
                    d += bed * a / max(a, 1.0) * ((((pz + 10000.0) / 140.0) % 1.0) - 0.5) * (1.0 - nz)
                rad = math.hypot(u, v)
                k = max(0.6, (rad + d) / rad) if rad > 1 else 1.0
            ring.append(U.Vector(x + rx * u * k, y + ry_ * u * k, z + v * k))
        rings.append(((x, y, z), ring))
    verts = []
    for c, ring in rings:
        verts.extend(ring)
    tris = []
    for i in range(len(rings) - 1):
        a0, b0 = i * n, (i + 1) * n
        for j in range(n):
            j1 = (j + 1) % n
            tris.append(U.IntVector(a0 + j, a0 + j1, b0 + j))
            tris.append(U.IntVector(a0 + j1, b0 + j1, b0 + j))
    for end, idx in ((0, 0), (len(rings) - 1, (len(rings) - 1) * n)):
        c = rings[end][0]
        ci = len(verts)
        verts.append(U.Vector(*c))
        for j in range(n):
            j1 = (j + 1) % n
            if end == 0:
                tris.append(U.IntVector(ci, idx + j1, idx + j))
            else:
                tris.append(U.IntVector(ci, idx + j, idx + j1))
    return verts, tris


def ellipsoid(c, r, amp, seed, bed=0.0, res=70.0, outward_only=False, rot_yaw=0.0):
    """Noisy ellipsoid centred at c with radii r=(rx, ry, rz); amp in cm."""
    cx, cy, cz = c
    ax, ay, az = r
    nth = max(16, int(2 * math.pi * max(ax, ay) / res))
    nph = max(8, int(math.pi * az / res) + 2)
    (fx, fy), (rx, ry) = yaw_dirs(rot_yaw)
    verts, tris = [], []
    for i in range(1, nph):
        ph = math.pi * i / nph
        for j in range(nth):
            th = 2 * math.pi * j / nth
            ex, ey, ez = math.sin(ph) * math.cos(th), math.sin(ph) * math.sin(th), math.cos(ph)
            lx, ly, lz = ax * ex, ay * ey, az * ez
            px, py, pz = cx + fx * lx + rx * ly, cy + fy * lx + ry * ly, cz + lz
            nx, ny, nz = ex / ax, ey / ay, ez / az
            nl = math.sqrt(nx * nx + ny * ny + nz * nz)
            nx, ny, nz = nx / nl, ny / nl, nz / nl
            wx, wy = fx * nx + rx * ny, fy * nx + ry * ny
            d = 0.0
            if amp > 0:
                d = amp * fbm(px, py, pz, seed)
                if outward_only:
                    d = abs(d)
                if bed > 0 and abs(nz) < 0.75:
                    d += bed * ((((pz + 10000.0) / 140.0) % 1.0) - 0.5) * (1.0 - abs(nz))
            verts.append(U.Vector(px + wx * d, py + wy * d, pz + nz * d))
    top = len(verts); verts.append(U.Vector(cx, cy, cz + az))
    bot = len(verts); verts.append(U.Vector(cx, cy, cz - az))
    for i in range(nph - 2):
        a0, b0 = i * nth, (i + 1) * nth
        for j in range(nth):
            j1 = (j + 1) % nth
            tris.append(U.IntVector(a0 + j, b0 + j, a0 + j1))
            tris.append(U.IntVector(a0 + j1, b0 + j, b0 + j1))
    last = (nph - 2) * nth
    for j in range(nth):
        j1 = (j + 1) % nth
        tris.append(U.IntVector(top, j, j1))
        tris.append(U.IntVector(bot, last + j1, last + j))
    return verts, tris


def vcyl(cx, cy, z0, z1, R, amp, seed, res=70.0):
    """Vertical noisy cylinder; noise only pushes outward (keeps the column clear)."""
    n = max(24, int(2 * math.pi * R / res))
    nz = max(2, int((z1 - z0) / res))
    verts, tris = [], []
    for i in range(nz + 1):
        z = z0 + (z1 - z0) * i / nz
        for j in range(n):
            th = 2 * math.pi * j / n
            x, y = cx + R * math.cos(th), cy + R * math.sin(th)
            d = abs(amp * fbm(x, y, z, seed)) if amp > 0 else 0.0
            verts.append(U.Vector(cx + (R + d) * math.cos(th), cy + (R + d) * math.sin(th), z))
    for i in range(nz):
        a0, b0 = i * n, (i + 1) * n
        for j in range(n):
            j1 = (j + 1) % n
            tris.append(U.IntVector(a0 + j, a0 + j1, b0 + j))
            tris.append(U.IntVector(a0 + j1, b0 + j1, b0 + j))
    for z, idx in ((z0, 0), (z1, nz * n)):
        ci = len(verts); verts.append(U.Vector(cx, cy, z))
        for j in range(n):
            if z == z0:
                tris.append(U.IntVector(ci, idx + (j + 1) % n, idx + j))
            else:
                tris.append(U.IntVector(ci, idx + j, idx + (j + 1) % n))
    return verts, tris


def obox(c, size, yaw=0.0, pitch=0.0, roll=0.0):
    """Oriented box (8 corners)."""
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                x, y, z = sx * hx, sy * hy, sz * hz
                y, z = y * cr - z * sr, y * sr + z * cr          # roll about X
                x, z = x * cp - z * sp, x * sp + z * cp          # pitch about Y
                x, y = x * cy_ - y * sy_, x * sy_ + y * cy_      # yaw about Z
                corners.append(U.Vector(c[0] + x, c[1] + y, c[2] + z))
    # index = sx*4 + sy*2 + sz with s in {0,1}
    f = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    tris = []
    for a, b, c2, d in f:
        tris.append(U.IntVector(a, b, c2)); tris.append(U.IntVector(a, c2, d))
    return corners, tris


def heightfield(x0, x1, y0, y1, zbot, top_fn, step=100.0):
    """Closed slab whose top is z = top_fn(x, y); used as rock (negative)."""
    nx = max(2, int((x1 - x0) / step)); ny = max(2, int((y1 - y0) / step))
    verts, tris = [], []
    for i in range(nx + 1):
        for j in range(ny + 1):
            x = x0 + (x1 - x0) * i / nx; y = y0 + (y1 - y0) * j / ny
            verts.append(U.Vector(x, y, top_fn(x, y)))
    nt = len(verts)
    for i in range(nx + 1):
        for j in range(ny + 1):
            x = x0 + (x1 - x0) * i / nx; y = y0 + (y1 - y0) * j / ny
            verts.append(U.Vector(x, y, zbot))
    idx = lambda i, j: i * (ny + 1) + j
    for i in range(nx):
        for j in range(ny):
            a, b, c, d = idx(i, j), idx(i + 1, j), idx(i + 1, j + 1), idx(i, j + 1)
            tris.append(U.IntVector(a, b, c)); tris.append(U.IntVector(a, c, d))
            tris.append(U.IntVector(nt + a, nt + c, nt + b)); tris.append(U.IntVector(nt + a, nt + d, nt + c))
    border = [idx(i, 0) for i in range(nx)] + [idx(nx, j) for j in range(ny)] + \
             [idx(i, ny) for i in range(nx, 0, -1)] + [idx(0, j) for j in range(ny, 0, -1)]
    for k in range(len(border)):
        a, b = border[k], border[(k + 1) % len(border)]
        tris.append(U.IntVector(a, nt + a, nt + b)); tris.append(U.IntVector(a, nt + b, b))
    return verts, tris


# ---------------------------------------------------------------- mesh ops
def _append(target, vt, negative):
    verts, tris = vt
    tmp = U.DynamicMesh()
    b = U.GeometryScriptSimpleMeshBuffers()
    b.vertices = verts
    b.triangles = tris
    GS_E.append_buffers_to_mesh(tmp, b, 0)
    area, vol = GS_Q.get_mesh_volume_area(tmp)
    if (vol < 0) != negative:
        GS_N.flip_normals(tmp)
    GS_E.append_mesh(target, tmp, U.Transform())


def solidify(mesh, cell):
    o = U.GeometryScriptSolidifyOptions()
    gp = U.GeometryScript3DGridParameters()
    gp.size_method = U.GeometryScriptGridSizingMethod.GRID_CELL_SIZE
    gp.grid_cell_size = cell
    o.grid_parameters = gp
    o.winding_threshold = 0.5
    o.solid_at_boundaries = True
    o.extend_bounds = 2 * cell
    o.surface_search_steps = 4
    o.thicken_shells = False
    GS_V.apply_mesh_solidify(mesh, o)


def close_pinholes(mesh, cell):
    o = U.GeometryScriptMorphologyOptions()
    gp = U.GeometryScript3DGridParameters()
    gp.size_method = U.GeometryScriptGridSizingMethod.GRID_CELL_SIZE
    gp.grid_cell_size = cell
    o.sdf_grid_parameters = gp
    o.operation = U.GeometryScriptMorphologicalOpType.CLOSE
    o.distance = cell
    GS_V.apply_mesh_morphology(mesh, o)


def plane_cut(mesh, point, normal):
    """Removes the part of the mesh on the +normal side of the plane."""
    po = U.GeometryScriptMeshPlaneCutOptions()
    po.fill_holes = False
    rot = U.MathLibrary.make_rot_from_z(U.Vector(*normal))
    GS_B.apply_mesh_plane_cut(mesh, U.Transform(U.Vector(*point), rot, U.Vector(1, 1, 1)), po)


def joint_frames(exit_t):
    ex, ey, ez, eyaw = exit_t
    (fx, fy), _ = yaw_dirs(eyaw)
    return [((0.0, 0.0, 0.0), (1.0, 0.0)), ((ex, ey, ez), (fx, fy))]


def snap_joints(mesh, exit_t):
    """Blend vertices within SNAP of each joint plane onto the exact joint circle."""
    GS_R.compact_mesh(mesh)
    res = GS_Q.get_all_vertex_positions(mesh, False)
    pts = list(GS_L.convert_vector_list_to_array(res[1]))
    frames = joint_frames(exit_t)
    moved = 0
    for i, p in enumerate(pts):
        for (cx, cy, cz), (fx, fy) in frames:
            dx, dy, dz = p.x - cx, p.y - cy, p.z - cz
            along = dx * fx + dy * fy           # distance from the plane, + into the chunk at entry
            if (cx, cy, cz) != (0.0, 0.0, 0.0):
                along = -along                  # at the exit, + points back into the chunk
            if along > SNAP or along < -1.0:
                continue
            lx, ly = dx - along * (fx if (cx, cy, cz) == (0.0, 0.0, 0.0) else -fx), dy - along * (fy if (cx, cy, cz) == (0.0, 0.0, 0.0) else -fy)
            r = math.sqrt(lx * lx + ly * ly + dz * dz)
            if r < 1.0 or r > 900.0:
                continue
            w = 1.0 - smooth(max(along, 0.0) / SNAP)
            k = (r + (JOINT_R - r) * w) / r
            base_x, base_y = cx + (dx - lx), cy + (dy - ly)
            if along <= 0.5:
                # exactly on the plane: put it on the plane too
                base_x, base_y = cx, cy
            pts[i] = U.Vector(base_x + lx * k, base_y + ly * k, cz + dz * k)
            moved += 1
            break
    GS_E.set_all_mesh_vertex_positions(mesh, GS_L.convert_array_to_vector_list(pts))
    return moved


def finish(mesh):
    GS_N.flip_normals(mesh)
    o = U.GeometryScriptCalculateNormalsOptions()
    GS_N.recompute_normals(mesh, o)
    try:
        GS_UV.set_mesh_u_vs_from_box_projection(mesh, 0, U.Transform(U.Vector(0, 0, 0), U.Rotator(0, 0, 0), U.Vector(4, 4, 4)), U.GeometryScriptMeshSelection(), 2)
    except Exception:
        pass


def build_mesh(spec, cell):
    """Voxelise the spec's shapes into a cut, snapped, inward-facing shell."""
    t0 = time.time()
    sh = spec['shapes']
    dbg = spec.setdefault('debug', [])

    def note(tag):
        bb = GS_Q.get_mesh_bounding_box(m)
        dbg.append('%s tris=%d min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f)' % (
            tag, GS_Q.get_num_triangle_i_ds(m), bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z))

    m = U.DynamicMesh()
    for vt in sh.pos:
        _append(m, vt, False)
    note('pos')
    solidify(m, cell)
    note('solid1')
    if sh.neg:
        for vt in sh.neg:
            _append(m, vt, True)
        solidify(m, cell)
        note('solid2')
    close_pinholes(m, cell)
    note('close')
    rs = U.GeometryScriptRemoveSmallComponentOptions()
    rs.min_volume = 1.0e7     # 10 m^3 -> crumbs
    rs.min_triangle_count = 200
    GS_R.remove_small_components(m, rs)
    note('small')
    ex, ey, ez, eyaw = spec['exit']
    (fx, fy), _ = yaw_dirs(eyaw)
    plane_cut(m, (0, 0, 0), (-1, 0, 0))
    plane_cut(m, (ex, ey, ez), (fx, fy, 0))
    note('cut')
    moved = snap_joints(m, spec['exit'])
    finish(m)
    return m, time.time() - t0, moved


# ---------------------------------------------------------------- chunk specs
def joint_fade(p, exit_t, inner=SNAP + 100.0, outer=650.0):
    """0 near either joint plane (within the neck), 1 away from it."""
    best = 1.0
    for (cx, cy, cz), (fx, fy) in joint_frames(exit_t):
        dx, dy, dz = p[0] - cx, p[1] - cy, p[2] - cz
        along = abs(dx * fx + dy * fy)
        lat = math.sqrt(max(0.0, dx * dx + dy * dy + dz * dz - along * along))
        if lat < 1300.0:
            best = min(best, sstep(inner, outer, along))
    return best


def spec_A():
    """Straight 30 m tunnel: neck, oval tube with silt floor, 7.8 m squeeze, tube, neck."""
    L = 3000.0
    ex = (L, 0.0, 0.0, 0.0)
    p = Path().straight(L)
    keys = [(-EXT, 450, 450, 9999), (300, 450, 450, 9999), (800, 560, 480, 360), (1050, 540, 470, 360),
            (1220, 390, 390, 9999), (1780, 390, 390, 9999), (1950, 520, 500, 360), (2250, 590, 470, 360),
            (2650, 450, 450, 9999), (L + EXT, 450, 450, 9999)]
    prof = lambda s: keyed(keys, s)
    def amp(q, s):
        a = 45.0
        if 1150 < s < 1850:
            a = 10.0
        return a * joint_fade(q, ex)
    sh = Shapes(11)
    sh.add(tube(p, prof, -EXT, L + EXT, amp, 11, bed=12.0))
    return {'name': 'A', 'type': 'tunnel', 'exit': ex, 'shapes': sh, 'path': p}


def spec_B():
    """Straight 30 m breakdown chamber between two necks."""
    L = 3000.0
    ex = (L, 0.0, 0.0, 0.0)
    p = Path().straight(L)
    keys = [(-EXT, 450, 450, 9999), (250, 450, 450, 9999), (700, 600, 560, 440), (2300, 600, 560, 440),
            (2750, 450, 450, 9999), (L + EXT, 450, 450, 9999)]
    prof = lambda s: keyed(keys, s)
    amp = lambda q, s: 50.0 * joint_fade(q, ex)
    rnd = random.Random(22)
    sh = Shapes(22)
    sh.add(tube(p, prof, -EXT, L + EXT, amp, 22, bed=12.0))
    for i in range(7):
        cx = 800 + 1400 * i / 6.0 + rnd.uniform(-120, 120)
        c = (cx, rnd.uniform(-350, 350), rnd.uniform(-120, 220))
        r = (rnd.uniform(520, 760), rnd.uniform(600, 820), rnd.uniform(450, 620))
        sh.add(ellipsoid(c, r, 70.0, 22 + i, bed=20.0, rot_yaw=rnd.uniform(-30, 30)))
    # silt floor: rock below z = -520, rising to -470 over the last 4+ m before each neck
    def top(x, y):
        rise = 1.0 - min(sstep(250, 750, x), 1.0 - sstep(2250, 2750, x))
        return -520.0 + 50.0 * rise + 25.0 * fbm(x, y, 0, 23, 300.0)
    sh.add(heightfield(150, 2850, -1100, 1100, -1300, top), True)
    # breakdown: tilted blocks on the floor; near the axis they stay below the keel line
    for i in range(26):
        x = rnd.uniform(800, 2200)
        y = rnd.uniform(-850, 850)
        sz = (rnd.uniform(120, 340), rnd.uniform(120, 300), rnd.uniform(80, 220))
        z = -520 + sz[2] * 0.3 + rnd.uniform(-40, 80)
        lim = -310.0 if abs(y) < 350 + max(sz) * 0.75 else -120.0
        z = min(z, lim - max(sz) * 0.75)
        sh.add(obox((x, y, z), sz, rnd.uniform(0, 180), rnd.uniform(-25, 25), rnd.uniform(-25, 25)), True)
    return {'name': 'B', 'type': 'cavern', 'exit': ex, 'shapes': sh, 'path': p}


T1_R = 1400.0


T1_SQUEEZE = (1260.0, 2640.0)   # level, straight, 7 x 7 m
T1_DROP = (3900.0, 8400.0)      # cosine drop of 6 m; steepest 11.8 deg mid-run


def t1_path():
    # straight for the squeeze plus a hull length (12.3 m) each side, then the bend
    p = Path().straight(3900).arc(T1_R, 90.0).straight(3400)
    d0, d1 = T1_DROP

    def zf(s):
        if s <= d0: return 0.0
        if s >= d1: return -600.0
        return -300.0 * (1.0 - math.cos(math.pi * (s - d0) / (d1 - d0)))
    p.zf = zf
    return p


def spec_T1():
    """Tunnel: level 7 x 7 m squeeze, a 90 degree turn, a 6 m drop, level neck."""
    p = t1_path()
    L = p.length
    x, y, z, yaw = p.at(L)
    ex = (round(x, 3), round(y, 3), round(z, 3), round(yaw, 6))
    q0, q1 = T1_SQUEEZE
    # (s, half width, half height, floor depth); no flat silt floor while it drops,
    # so a level hull's low corner keeps clear of the floor on the slope
    keys = [(-EXT, 450, 450, 9999), (300, 450, 450, 9999), (800, 540, 500, 380), (1050, 520, 480, 380),
            (q0, 350, 350, 9999), (q1, 350, 350, 9999), (q1 + 200, 520, 480, 380), (3300, 590, 550, 410),
            (3700, 600, 600, 560), (4100, 600, 620, 9999), (7900, 600, 620, 9999), (8500, 480, 470, 9999),
            (L - 600, 450, 450, 9999), (L + EXT, 450, 450, 9999)]
    prof = lambda s: keyed(keys, s)

    def amp(q, s):
        if q0 - 100 < s < q1 + 100:
            return -12.0          # squeeze: noise only widens it, never below 7 m
        return 50.0 * joint_fade(q, ex)
    sh = Shapes(33)
    sh.add(tube(p, prof, -EXT, L + EXT, amp, 33, bed=12.0))
    return {'name': 'T1', 'type': 'tunnel', 'exit': ex, 'shapes': sh, 'path': p,
            'squeeze': T1_SQUEEZE}


C1_COL = (3000.0, 0.0)      # sink column axis
C1_PARK_Z = -2400.0
C1_FLOOR = -2950.0
C1_EXIT_YAW = -60.0
C1_EXIT_RUN = 3200.0
C1_COL_R = 1050.0


def c1_exit():
    (fx, fy), _ = yaw_dirs(C1_EXIT_YAW)
    return (round(C1_COL[0] + fx * C1_EXIT_RUN, 3), round(C1_COL[1] + fy * C1_EXIT_RUN, 3), C1_PARK_Z, C1_EXIT_YAW)


def spec_C1():
    """Stop cavern: high entry, a clear sink column, a lower exit turned -60 degrees."""
    ex = c1_exit()
    cx, cy = C1_COL
    rnd = random.Random(44)
    sh = Shapes(44)
    camp = lambda q: 110.0 * joint_fade(q, ex)
    # entry tunnel, level, opening into the cavern
    pin = Path().straight(cx)
    kin = [(-EXT, 450, 450, 9999), (300, 450, 450, 9999), (900, 540, 500, 9999), (cx, 650, 620, 9999)]
    sh.add(tube(pin, lambda s: keyed(kin, s), -EXT, cx, lambda q, s: 60.0 * joint_fade(q, ex), 45, bed=15.0))
    # exit tunnel from the column axis, level at park height
    pout = Path(cx, cy, C1_EXIT_YAW).straight(C1_EXIT_RUN)
    pout.zf = lambda s: C1_PARK_Z
    kout = [(0, 650, 600, 9999), (1300, 620, 560, 9999), (1800, 560, 500, 440), (2400, 470, 460, 9999),
            (C1_EXIT_RUN - 450, 450, 450, 9999), (C1_EXIT_RUN + EXT, 450, 450, 9999)]
    sh.add(tube(pout, lambda s: keyed(kout, s), 0, C1_EXIT_RUN + EXT, lambda q, s: 60.0 * joint_fade(q, ex), 46, bed=15.0))
    # chamber: stacked ellipsoids
    sh.add(ellipsoid((cx, cy, -2400), (2200, 1800, 900), 110.0, 47, bed=25.0, rot_yaw=10))
    sh.add(ellipsoid((cx, cy, -1200), (1700, 1500, 1200), 110.0, 48, bed=25.0, rot_yaw=-15))
    sh.add(ellipsoid((cx - 200, cy, 0), (1600, 1300, 900), 100.0, 49, bed=25.0, rot_yaw=5))
    # lobes and pockets (uneven walls and roof)
    sh.add(ellipsoid((cx + 1300, 900, -1700), (800, 700, 600), 80.0, 50, bed=20.0, rot_yaw=30))
    sh.add(ellipsoid((cx - 1200, -800, -2000), (900, 700, 700), 80.0, 51, bed=20.0, rot_yaw=-20))
    sh.add(ellipsoid((cx + 600, 1100, -400), (700, 600, 500), 80.0, 52, bed=20.0))
    sh.add(ellipsoid((cx - 800, 1000, -2500), (800, 700, 500), 70.0, 53, bed=20.0, rot_yaw=40))   # low pocket
    sh.add(ellipsoid((cx + 900, -600, 450), (600, 500, 450), 70.0, 54))                          # roof dome
    # serpent pocket off the -Y wall, mid height, with a narrow mouth
    sh.add(ellipsoid((cx + 400, -1750, -1500), (520, 450, 380), 50.0, 55, rot_yaw=10))
    sh.add(ellipsoid((cx + 400, -1420, -1500), (300, 360, 260), 30.0, 56))
    # the sink column: kept clear, noise only pushes outward
    sh.add(vcyl(cx, cy, C1_FLOOR + 150, 250, C1_COL_R, 60.0, 57))
    # --- rock
    def floor_top(x, y):
        dpark = math.hypot(x - cx, y - cy)
        mound = 0.0
        if dpark > 1000:
            mound = sstep(1000, 1600, dpark) * (90.0 + 110.0 * fbm(x, y, 0, 58, 500.0))
        # keep the exit run's floor untouched
        (fx, fy), (rx, ry) = yaw_dirs(C1_EXIT_YAW)
        along = (x - cx) * fx + (y - cy) * fy
        lat = abs((x - cx) * rx + (y - cy) * ry)
        if along > 800 and lat < 900:
            mound *= sstep(600, 900, lat)
        return C1_FLOOR + max(-20.0, mound) + 15.0 * fbm(x, y, 1, 59, 150.0)
    sh.add(heightfield(-200, 5800, -3400, 2800, -3800, floor_top), True)
    # ledges: rock shelves from the walls, outside the column
    ledges = [(120.0, -1750.0, 900), (205.0, -2150.0, 800), (60.0, -950.0, 700), (160.0, -600.0, 600)]
    for az, ztop, wid in ledges:
        (ax, ay), _ = yaw_dirs(az)
        r0 = C1_COL_R + 150.0
        mid = r0 + 1200.0
        sh.add(obox((cx + ax * mid, cy + ay * mid, ztop - 120.0), (2400.0, wid, 240.0), az, rnd.uniform(-4, 4), rnd.uniform(-4, 4)), True)
    # breakdown pile, away from the column
    pile = (cx - 300.0, 1650.0)
    for i in range(20):
        a = rnd.uniform(0, 2 * math.pi); d = rnd.uniform(0, 520)
        x, y = pile[0] + d * math.cos(a), pile[1] + d * math.sin(a)
        sz = (rnd.uniform(140, 420), rnd.uniform(120, 340), rnd.uniform(90, 260))
        hd = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2)
        if math.hypot(x - cx, y - cy) - hd < C1_COL_R + 120:
            continue
        z = C1_FLOOR + rnd.uniform(0, 330) * (1.0 - d / 560.0)
        sh.add(obox((x, y, z), sz, rnd.uniform(0, 180), rnd.uniform(-30, 30), rnd.uniform(-30, 30)), True)
    sockets = {
        'Park': (cx, cy, C1_PARK_Z, 0.0),
        'Serpent': (cx + 400, -1750.0, -1500.0, 90.0),
    }
    return {'name': 'C1', 'type': 'stop', 'exit': ex, 'shapes': sh, 'sockets': sockets,
            'ledges': ledges, 'pile': pile}


SPECS = {'A': spec_A, 'B': spec_B, 'T1': spec_T1, 'C1': spec_C1}


# ---------------------------------------------------------------- bake
def grey_material():
    m = U.load_asset(MAT_GREY) if U.EditorAssetLibrary.does_asset_exist(MAT_GREY) else None
    if m:
        return m
    at = U.AssetToolsHelpers.get_asset_tools()
    m = at.create_asset('M_CaveGrey', '/Game/Deep/CaveGen/Materials', U.Material, U.MaterialFactoryNew())
    mel = U.MaterialEditingLibrary
    c = mel.create_material_expression(m, U.MaterialExpressionConstant3Vector, -300, 0)
    c.set_editor_property('constant', U.LinearColor(0.18, 0.18, 0.18, 1.0))
    r = mel.create_material_expression(m, U.MaterialExpressionConstant, -300, 200)
    r.set_editor_property('r', 0.75)
    mel.connect_material_property(c, '', U.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(r, '', U.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property('used_with_nanite', True)
    mel.recompile_material(m)
    U.EditorAssetLibrary.save_loaded_asset(m)
    return m


def apply_mesh_settings(sm, nanite=False):
    """Material slot, complex-as-simple collision and ray tracing geometry that matches the raster mesh.

    Nanite is off by default: it measured cheaper in T1 and C1 (Nov 16), and without Nanite
    ray tracing uses the same triangles as raster. With nanite=True the fallback and the ray
    tracing proxy keep 100% of the triangles instead."""
    mat = grey_material()
    sm.set_editor_property('static_materials', [U.StaticMaterial(material_interface=mat, material_slot_name='Rock')])
    ns = sm.get_editor_property('nanite_settings')
    ns.enabled = nanite
    ns.fallback_target = U.NaniteFallbackTarget.PERCENT_TRIANGLES
    ns.fallback_percent_triangles = 1.0
    ns.keep_percent_triangles = 1.0
    sm.set_editor_property('nanite_settings', ns)
    rt = sm.get_editor_property('ray_tracing_proxy_settings')
    rt.enabled = nanite
    rt.fallback_target = U.NaniteFallbackTarget.PERCENT_TRIANGLES
    rt.fallback_percent_triangles = 1.0
    sm.set_editor_property('ray_tracing_proxy_settings', rt)
    bs = sm.get_editor_property('body_setup')
    bs.set_editor_property('collision_trace_flag', U.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    sm.set_editor_property('body_setup', bs)
    sm.post_edit_change() if hasattr(sm, 'post_edit_change') else None
    U.EditorAssetLibrary.save_loaded_asset(sm)


def write_mesh(name, mesh, nanite=False):
    path = '%s/SM_CaveChunk_%s' % (MESH_DIR, name)
    if U.EditorAssetLibrary.does_asset_exist(path):
        sm = U.load_asset(path)
        o = U.GeometryScriptCopyMeshToAssetOptions()
        o.enable_recompute_normals = False
        o.enable_recompute_tangents = True
        o.emit_transaction = False
        lod = U.GeometryScriptMeshWriteLOD()
        GS_A.copy_mesh_to_static_mesh(mesh, sm, o, lod, True)
    else:
        o = U.GeometryScriptCreateNewStaticMeshAssetOptions()
        o.enable_recompute_normals = False
        o.enable_recompute_tangents = True
        o.enable_nanite = nanite
        o.enable_collision = True
        o.collision_mode = U.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        sm, outcome = GS_NA.create_new_static_mesh_asset_from_mesh(mesh, path, o)
    apply_mesh_settings(sm, nanite)
    return sm


def bake(name, cell=25.0, nanite=False):
    """Build and save one chunk. Returns a dict of numbers for the recipe."""
    t0 = time.time()
    spec = SPECS[name]()
    t1 = time.time()
    mesh, tbuild, moved = build_mesh(spec, cell)
    ntri = GS_Q.get_num_triangle_i_ds(mesh)
    t2 = time.time()
    write_mesh(name, mesh, nanite)
    t3 = time.time()
    bb = GS_Q.get_mesh_bounding_box(mesh)
    return {'name': name, 'type': spec['type'], 'exit': spec['exit'], 'tris': ntri,
            'shapes_s': round(t1 - t0, 2), 'voxel_s': round(tbuild, 2), 'save_s': round(t3 - t2, 2),
            'total_s': round(t3 - t0, 2), 'snapped': moved, 'debug': spec.get('debug'),
            'min': (round(bb.min.x, 1), round(bb.min.y, 1), round(bb.min.z, 1)),
            'max': (round(bb.max.x, 1), round(bb.max.y, 1), round(bb.max.z, 1))}
