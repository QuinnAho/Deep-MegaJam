"""DEEP cave chunk generator (editor Python, Geometry Script).

Each chunk is a closed "water" solid, voxelised by Geometry Script in three
passes:
  1. water shapes: passage rings, alcoves, domes, chambers, joint slots;
  2. minus rock shapes: breakdown blocks, ledges, floors;
  3. plus the clear core: the hull's swept box along the flight path, grown by
     1.0 m (0.75 m in squeezes), so rock can come close but never into it.
Then pinholes are closed, crumbs dropped, the ends cut on the joint planes and
snapped onto the 9 m joint circle, the normals turned inward, and a static
mesh baked in /Game/Deep/CaveGen/Meshes.

Round 3 (rock that catches the lamp): passages change section along each
tunnel (canyon, keyhole, wide-low, round, rift) and the walls are displaced by
a rock structure: horizontal beds (ledges, overhangs, stepped roofs and floors)
and two families of vertical joints (blocky faces, straight slots). Rock only
moves toward the core as far as a soft clamp allows, so the core never shows.

Frame of every chunk: the entry joint's centre is the origin, +X runs into the
chunk, Z is up. Units are cm and degrees. Each chunk ends on a level, round,
9 m joint whose transform (position + yaw) is recorded as the chunk's exit.

Run from the editor (Python, or the MCP execute_python tool):
    import cavegen; cavegen.bake('T1')
Recipe and numbers: deep-claude-brain/03 - Research/Cave Spike Recipe.md
"""
import bisect
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
RING_STEP = 33.0      # ring spacing along a path, cm
RING_N = 200          # points per ring, spaced by arc length
NA = 900              # polar samples per section before resampling
MESH_DIR = '/Game/Deep/CaveGen/Meshes'
MAT_GREY = '/Game/Deep/CaveGen/Materials/M_CaveGrey'

# the hull and its clear core
HULL_HALF = (615.0, 165.0, 250.0)   # 12.3 x 3.3 x 5.0 m
HULL_HALF_Z = HULL_HALF[2]
M_BODY = 100.0        # clearance in tunnels and caverns
M_SQUEEZE = 75.0      # clearance in squeezes
CORE_E = 15.0         # extra so the core's rounding never eats the margin
CORE_RHO = 35.0       # core corner radius
DELTA = 12.0          # displaced rock stays at least this far outside the core
BASE_GAP = 30.0       # the undisplaced wall stays at least this far out along each ray
SMAX_K = 60.0         # blend width where the wall meets the core's offset


# ---------------------------------------------------------------- math helpers
def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def sstep(a, b, x):
    if b == a:
        return 1.0 if x >= b else 0.0
    return smooth((x - a) / (b - a))


def smax(a, b, k):
    """Smooth maximum: never below max(a, b)."""
    h = max(k - abs(a - b), 0.0) / k
    return max(a, b) + h * h * k * 0.25


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


def ring_s(s0, s1, step=RING_STEP):
    n = max(2, int(math.ceil((s1 - s0) / step)))
    return [s0 + (s1 - s0) * i / n for i in range(n + 1)]


# ---------------------------------------------------------------- rock structure
class Rock:
    """Beds (horizontal, slightly dipping) and two families of vertical joints.

    Walls move along their horizontal normal by a per-bed offset (resistant beds
    stand out as ledges, weak ones recess under overhangs), a slow stretched
    variation, a per-block offset between joints (blocky faces with straight
    vertical edges), a groove at each bedding plane and a little fine noise.
    Roofs snap up to the bedding plane above, rock floors down to the one below."""

    def __init__(self, seed, zmin=-5200.0, zmax=4200.0, bed=(55.0, 210.0),
                 joints=((25.0, 150.0, 500.0), (110.0, 150.0, 500.0)), span=30000.0,
                 dip=(1.2, 30.0), a_res=45.0, a_var=30.0, a_blk=25.0, a_notch=18.0, a_fine=10.0):
        rnd = random.Random(seed)
        self.seed = seed
        zs = [zmin]
        while zs[-1] < zmax:
            zs.append(zs[-1] + rnd.uniform(*bed))
        self.beds = zs
        self.res = [rnd.uniform(-1.0, 1.0) for _ in zs]
        self.fam = []
        for ang, lo, hi in joints:
            r = math.radians(ang)
            n = (-math.sin(r), math.cos(r))
            ds = [-span]
            while ds[-1] < span:
                ds.append(ds[-1] + rnd.uniform(lo, hi))
            self.fam.append((ang, n, ds))
        t = math.tan(math.radians(dip[0]))
        self.dip = (t * math.cos(math.radians(dip[1])), t * math.sin(math.radians(dip[1])))
        self.a_res, self.a_var, self.a_blk, self.a_notch, self.a_fine = a_res, a_var, a_blk, a_notch, a_fine

    def zb(self, x, y, z):
        """Height in the bed frame (beds dip a little)."""
        return z - (x * self.dip[0] + y * self.dip[1])

    def bed(self, zb):
        k = bisect.bisect_right(self.beds, zb) - 1
        return max(0, min(len(self.beds) - 2, k))

    def cell(self, x, y, f):
        ang, n, ds = self.fam[f]
        return bisect.bisect_right(ds, x * n[0] + y * n[1])

    def wall(self, x, y, zb, k):
        z0, z1 = self.beds[k], self.beds[k + 1]
        t = (zb - z0) / (z1 - z0)
        ja, jb = self.cell(x, y, 0), self.cell(x, y, 1)
        d = self.a_res * self.res[k]
        d += self.a_var * fbm(x / 3.5, y / 3.5, 0.5 * (z0 + z1), self.seed, 300.0)
        d += self.a_blk * _h(k, ja, jb, self.seed + 7)
        d += self.a_notch * (1.0 - math.sqrt(max(0.0, math.sin(math.pi * t))))
        d += self.a_fine * fbm(x, y, zb, self.seed + 3, 120.0)
        return d

    def displace(self, p, n, kind='silt'):
        """Displacement (dx, dy, dz) for a surface point p with unit normal n
        pointing into the rock. Positive along n means away from the water."""
        x, y, z = p
        nx, ny, nz = n
        nh = math.sqrt(nx * nx + ny * ny)
        ww = sstep(0.45, 0.8, nh)
        zb = self.zb(x, y, z)
        k = self.bed(zb)
        dx = dy = dz = 0.0
        if ww > 0.0:
            d = ww * self.wall(x, y, zb, k)
            dx, dy = d * nx / nh, d * ny / nh
        rw = 1.0 - ww
        if rw > 0.0:
            if nz > 0.0:      # roof: up to the bedding plane above
                up = self.beds[k + 1] - zb + 0.5 * self.a_fine * fbm(x, y, zb, self.seed + 5, 150.0)
                dz = rw * max(0.0, up)
            elif kind == 'rock':   # rock floor: down to the bedding plane below, blocky
                ja, jb = self.cell(x, y, 0), self.cell(x, y, 1)
                down = (self.beds[k] - zb) - 25.0 * (0.5 + 0.5 * _h(ja, jb, k, self.seed + 11))
                dz = rw * min(0.0, down)
            else:                  # silt floor: a soft swell, only downward
                dz = -rw * 12.0 * (1.0 + fbm(x, y, 0.0, self.seed + 9, 260.0))
        return dx, dy, dz


# ---------------------------------------------------------------- sections
def r_sel(c, s, aR, aL, bT, bB, n):
    """Polar radius of an asymmetric superellipse (right/left, top/bottom)."""
    a = aR if c >= 0 else aL
    b = bT if s >= 0 else bB
    return ((abs(c) / a) ** n + (abs(s) / b) ** n) ** (-1.0 / n)


def r_box(c, s, uL, uR, vB, vT):
    """Polar radius of a box [-uL, uR] x [-vB, vT] that contains the origin."""
    r = 1e9
    if c > 1e-9:
        r = min(r, uR / c)
    elif c < -1e-9:
        r = min(r, uL / -c)
    if s > 1e-9:
        r = min(r, vT / s)
    elif s < -1e-9:
        r = min(r, vB / -s)
    return r


ANG = [(math.cos(-0.5 * math.pi + 2 * math.pi * k / NA), math.sin(-0.5 * math.pi + 2 * math.pi * k / NA))
       for k in range(NA)]


def sec_neck():
    return [JOINT_R] * NA


def sec_round():
    return [r_sel(c, s, 480.0, 480.0, 500.0, 470.0, 2.2) for c, s in ANG]


def sec_canyon(w=400.0, off=0.0, off2=0.0):
    """Tall narrow canyon: 8 m wide at the hull, stepping in to 6 m and a 3.6 m slot, 18 m tall."""
    out = []
    for c, s in ANG:
        r = r_sel(c, s, w, w, 500.0, 470.0, 6.0)
        r = max(r, r_box(c, s, 300.0 - off, 300.0 + off, 100.0, 900.0))
        r = max(r, r_box(c, s, 180.0 - off2, 180.0 + off2, 100.0, 1400.0))
        out.append(r)
    return out


def sec_keyhole(off=0.0):
    """Rounded tube over a narrow slot."""
    return [max(r_sel(c, s, 450.0, 450.0, 480.0, 420.0, 2.5), r_box(c, s, 130.0 - off, 130.0 + off, 950.0, 100.0))
            for c, s in ANG]


def sec_widelow(aR, aL, bT=450.0, bB=440.0, n=3.5):
    """Wide low passage; its roof snaps to a bedding plane."""
    return [r_sel(c, s, aR, aL, bT, bB, n) for c, s in ANG]


def sec_rift():
    """Squeeze: a narrow rift with a roof slot; the core sets its width."""
    return [max(r_sel(c, s, 285.0, 285.0, 450.0, 400.0, 6.0), r_box(c, s, 120.0, 120.0, 100.0, 1000.0))
            for c, s in ANG]


class Sections:
    """Section shapes along a path. anchors: [(s, name)]; between two anchors with
    different names the radii blend smoothly. defs: name -> (fn(s) -> radii, floor kind, amp, varies)."""

    def __init__(self, anchors, defs):
        self.anchors = anchors
        self.defs = defs
        self.cache = {}

    def radii(self, name, s):
        fn, kind, amp, varies = self.defs[name]
        if varies:
            return fn(s)
        if name not in self.cache:
            self.cache[name] = fn(s)
        return self.cache[name]

    def at(self, s):
        """(radii, floor kind, amplitude, dominant name) at s."""
        an = self.anchors
        if s <= an[0][0]:
            n = an[0][1]
            return self.radii(n, s), self.defs[n][1], self.defs[n][2], n
        for i in range(len(an) - 1):
            (sa, na), (sb, nb) = an[i], an[i + 1]
            if s <= sb:
                if na == nb:
                    return self.radii(na, s), self.defs[na][1], self.defs[na][2], na
                t = smooth((s - sa) / (sb - sa))
                ra, rb = self.radii(na, s), self.radii(nb, s)
                dom = na if t < 0.5 else nb
                amp = self.defs[na][2] + (self.defs[nb][2] - self.defs[na][2]) * t
                return [a + (b - a) * t for a, b in zip(ra, rb)], self.defs[dom][1], amp, dom
        n = an[-1][1]
        return self.radii(n, s), self.defs[n][1], self.defs[n][2], n

    def name_at(self, s):
        return self.at(s)[3]


# ---------------------------------------------------------------- the clear core
def core_half(m):
    return (HULL_HALF[0] + m + CORE_E, HULL_HALF[1] + m + CORE_E, HULL_HALF[2] + m + CORE_E)


def core_slice(path, s, margin_fn, reach=900.0, step=30.0):
    """Bounding rectangle (u0, u1, v0, v1) in ring s's vertical plane of the hull's
    grown boxes for hull centres within +-reach of s (exact box/plane slab clips)."""
    x, y, z, yaw = path.at(s)
    (fx, fy), (rx, ry) = yaw_dirs(yaw)
    u0 = v0 = 1e9
    u1 = v1 = -1e9
    n = int(2 * reach / step)
    for i in range(n + 1):
        sp = s - reach + 2 * reach * i / n
        bx, by, bz, byaw = path.at(sp)
        hx, hy, hz = core_half(margin_fn(sp))
        (gx, gy), (qx, qy) = yaw_dirs(byaw)
        dx, dy = x - bx, y - by
        lo, hi = -1e9, 1e9
        ok = True
        for c0, c1, h in ((dx * gx + dy * gy, rx * gx + ry * gy, hx), (dx * qx + dy * qy, rx * qx + ry * qy, hy)):
            if abs(c1) < 1e-9:
                if abs(c0) > h:
                    ok = False
                    break
            else:
                t0, t1 = (-h - c0) / c1, (h - c0) / c1
                if t0 > t1:
                    t0, t1 = t1, t0
                lo, hi = max(lo, t0), min(hi, t1)
        if not ok or lo > hi:
            continue
        u0, u1 = min(u0, lo), max(u1, hi)
        v0, v1 = min(v0, bz - z - hz), max(v1, bz - z + hz)
    return (u0, u1, v0, v1)


def rrect_sdf(u, v, sl, rho=CORE_RHO):
    """Signed distance and unit gradient of a rounded rectangle (u0, u1, v0, v1)."""
    u0, u1, v0, v1 = sl
    cu, cv = 0.5 * (u0 + u1), 0.5 * (v0 + v1)
    pu, pv = u - cu, v - cv
    qu, qv = abs(pu) - (0.5 * (u1 - u0) - rho), abs(pv) - (0.5 * (v1 - v0) - rho)
    ou, ov = max(qu, 0.0), max(qv, 0.0)
    lo = math.sqrt(ou * ou + ov * ov)
    if lo > 1e-9:
        d, gu, gv = lo - rho, ou / lo, ov / lo
    elif qu > qv:
        d, gu, gv = qu - rho, 1.0, 0.0
    else:
        d, gu, gv = qv - rho, 0.0, 1.0
    return d, (gu if pu >= 0 else -gu), (gv if pv >= 0 else -gv)


def ray_rrect(c, s, sl, rho=CORE_RHO, ou=0.0, ov=0.0):
    """Distance from (ou, ov) along (c, s) to the rounded rectangle's edge (origin inside)."""
    u0, u1, v0, v1 = sl[0] - ou, sl[1] - ou, sl[2] - ov, sl[3] - ov
    t = 1e9
    if c > 1e-12:
        t = min(t, u1 / c)
    elif c < -1e-12:
        t = min(t, u0 / c)
    if s > 1e-12:
        t = min(t, v1 / s)
    elif s < -1e-12:
        t = min(t, v0 / s)
    pu, pv = t * c, t * s
    cu = (u1 - rho) if pu > u1 - rho else ((u0 + rho) if pu < u0 + rho else None)
    cv = (v1 - rho) if pv > v1 - rho else ((v0 + rho) if pv < v0 + rho else None)
    if cu is None or cv is None:
        return t
    dc = c * cu + s * cv
    disc = dc * dc - (cu * cu + cv * cv) + rho * rho
    return dc + math.sqrt(disc) if disc >= 0 else t


def rrect_pts(sl, n=64, rho=CORE_RHO):
    """Points round a rounded rectangle, by rays from its centre."""
    cu, cv = 0.5 * (sl[0] + sl[1]), 0.5 * (sl[2] + sl[3])
    out = []
    for j in range(n):
        a = -0.5 * math.pi + 2 * math.pi * j / n
        c, s = math.cos(a), math.sin(a)
        r = ray_rrect(c, s, sl, rho, cu, cv)
        out.append((cu + r * c, cv + r * s))
    return out


def rbox_sdf(p, c, half, yaw, rho=CORE_RHO):
    (fx, fy), (rx, ry) = yaw_dirs(yaw)
    dx, dy, dz = p[0] - c[0], p[1] - c[1], p[2] - c[2]
    l = (dx * fx + dy * fy, dx * rx + dy * ry, dz)
    q = [abs(l[i]) - (half[i] - rho) for i in range(3)]
    o = [max(t, 0.0) for t in q]
    lo = math.sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2])
    if lo > 1e-9:
        d, g = lo - rho, [o[i] / lo for i in range(3)]
    else:
        i = max(range(3), key=lambda k: q[k])
        d, g = q[i] - rho, [1.0 if k == i else 0.0 for k in range(3)]
    g = [g[i] if l[i] >= 0 else -g[i] for i in range(3)]
    return d, (g[0] * fx + g[1] * rx, g[0] * fy + g[1] * ry, g[2])


def rcyl_sdf(p, c, R, hz, rho=CORE_RHO):
    dx, dy = p[0] - c[0], p[1] - c[1]
    r = math.sqrt(dx * dx + dy * dy)
    d, gr, gz = rrect_sdf(r, p[2] - c[2], (-R, R, -hz, hz), rho)
    if r > 1e-9:
        return d, (gr * dx / r, gr * dy / r, gz)
    return d, (0.0, 0.0, gz if gz else 1.0)


class Core:
    """The clear core: grown hull boxes swept along the flight path (per-ring slices),
    plus boxes and vertical cylinders for sinks and in-place turns."""

    def __init__(self):
        self.boxes = []    # (centre, half, yaw)
        self.cyls = []     # (centre, R, half height)
        self.rings = []    # (s, (x, y, z, yaw), slice)

    def add_tube(self, path, s_list, slices):
        for s, sl in zip(s_list, slices):
            self.rings.append((s, path.at(s), sl))

    def sdf(self, p):
        best = (1e9, (0.0, 0.0, 1.0))
        for c, h, yaw in self.boxes:
            r = rbox_sdf(p, c, h, yaw)
            if r[0] < best[0]:
                best = r
        for c, R, hz in self.cyls:
            r = rcyl_sdf(p, c, R, hz)
            if r[0] < best[0]:
                best = r
        if self.rings:
            r = self._tube_sdf(p)
            if r[0] < best[0]:
                best = r
        return best

    def _ring_sdf(self, i, p):
        s, (x, y, z, yaw), sl = self.rings[i]
        _, (rx, ry) = yaw_dirs(yaw)
        d, gu, gv = rrect_sdf((p[0] - x) * rx + (p[1] - y) * ry, p[2] - z, sl)
        return d, (gu * rx, gu * ry, gv)

    def _along(self, i, p):
        s, (x, y, z, yaw), sl = self.rings[i]
        (fx, fy), _ = yaw_dirs(yaw)
        return (p[0] - x) * fx + (p[1] - y) * fy

    def _tube_sdf(self, p):
        """2D distance in the ring planes either side of p (the smaller), so a point is
        measured in its own cross-section, not in the plane of the nearest centre."""
        rings = self.rings
        n = len(rings)
        k = min(range(0, n, 4), key=lambda i: (rings[i][1][0] - p[0]) ** 2 + (rings[i][1][1] - p[1]) ** 2 +
                (rings[i][1][2] - p[2]) ** 2)
        lo, hi = max(0, k - 16), min(n - 1, k + 16)
        a_prev = self._along(lo, p)
        for i in range(lo, hi):
            a_next = self._along(i + 1, p)
            if a_prev >= 0.0 >= a_next:
                r0, r1 = self._ring_sdf(i, p), self._ring_sdf(i + 1, p)
                return r0 if r0[0] <= r1[0] else r1
            a_prev = a_next
        return self._ring_sdf(k, p)

    def meshes(self):
        out = []
        if self.rings:
            rings, cents = [], []
            for s, (x, y, z, yaw), sl in self.rings:
                _, (rx, ry) = yaw_dirs(yaw)
                rings.append([U.Vector(x + rx * u, y + ry * u, z + v) for u, v in rrect_pts(sl)])
                cents.append((x, y, z))
            out.append(ring_tube(rings, cents))
        for c, h, yaw in self.boxes:
            (fx, fy), (rx, ry) = yaw_dirs(yaw)
            pts = rrect_pts((-h[1], h[1], -h[2], h[2]))
            rings, cents = [], []
            for a in (-h[0], h[0]):
                bx, by = c[0] + fx * a, c[1] + fy * a
                rings.append([U.Vector(bx + rx * u, by + ry * u, c[2] + v) for u, v in pts])
                cents.append((bx, by, c[2]))
            out.append(ring_tube(rings, cents))
        for c, R, hz in self.cyls:
            n = 96
            rings, cents = [], []
            for z in (c[2] - hz, c[2] + hz):
                rings.append([U.Vector(c[0] + R * math.cos(2 * math.pi * j / n), c[1] + R * math.sin(2 * math.pi * j / n), z)
                              for j in range(n)])
                cents.append((c[0], c[1], z))
            out.append(ring_tube(rings, cents))
        return out


def clamp2(pu, pv, du, dv, sl):
    """Soft clamp in a ring plane: inward motion toward the core slice is squeezed with
    tanh so the moved point stays more than DELTA outside it (the slice is convex)."""
    f, gu, gv = rrect_sdf(pu, pv, sl)
    dn = du * gu + dv * gv
    room = f - DELTA
    if room <= 0.0:
        if dn < 0.0:
            du, dv = du - dn * gu, dv - dn * gv
        return du, dv
    if dn < 0.0:
        k = -room * math.tanh(-dn / room) - dn
        du, dv = du + k * gu, dv + k * gv
    for _ in range(6):
        if rrect_sdf(pu + du, pv + dv, sl)[0] >= DELTA - 0.5:
            break
        du, dv = du * 0.6, dv * 0.6
    return du, dv


def clamp3(p, d, core):
    """The same soft clamp in 3D against the nearest core primitive."""
    f, g = core.sdf(p)
    if f > 260.0:
        return d
    dn = d[0] * g[0] + d[1] * g[1] + d[2] * g[2]
    room = f - DELTA
    if room <= 0.0:
        if dn < 0.0:
            d = (d[0] - dn * g[0], d[1] - dn * g[1], d[2] - dn * g[2])
        return d
    if dn < 0.0:
        k = -room * math.tanh(-dn / room) - dn
        d = (d[0] + k * g[0], d[1] + k * g[1], d[2] + k * g[2])
    for _ in range(6):
        if core.sdf((p[0] + d[0], p[1] + d[1], p[2] + d[2]))[0] >= DELTA - 0.5:
            break
        d = (d[0] * 0.6, d[1] * 0.6, d[2] * 0.6)
    return d


# ---------------------------------------------------------------- shape buffers
class Shapes:
    """Positive (water), negative (rock) and core (water, added last) closed shapes."""

    def __init__(self, seed):
        self.pos = []   # list of (verts, tris)
        self.neg = []
        self.core = []
        self.seed = seed

    def add(self, vt, negative=False):
        (self.neg if negative else self.pos).append(vt)


def ring_tube(rings, cents):
    """Closed tube through rings of equal size, capped with a fan at each end."""
    n = len(rings[0])
    verts = []
    for r in rings:
        verts.extend(r)
    tris = []
    for i in range(len(rings) - 1):
        a0, b0 = i * n, (i + 1) * n
        for j in range(n):
            j1 = (j + 1) % n
            tris.append(U.IntVector(a0 + j, a0 + j1, b0 + j))
            tris.append(U.IntVector(a0 + j1, b0 + j1, b0 + j))
    for end in (0, len(rings) - 1):
        ci = len(verts)
        verts.append(U.Vector(*cents[end]))
        idx = end * n
        for j in range(n):
            j1 = (j + 1) % n
            if end == 0:
                tris.append(U.IntVector(ci, idx + j1, idx + j))
            else:
                tris.append(U.IntVector(ci, idx + j, idx + j1))
    return verts, tris


def resample(pts, n):
    """n points equally spaced by arc length round a closed polygon, from pts[0]."""
    m = len(pts)
    cum = [0.0]
    for k in range(m):
        a, b = pts[k], pts[(k + 1) % m]
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    tot = cum[-1]
    out = []
    k = 0
    for j in range(n):
        t = tot * j / n
        while cum[k + 1] < t:
            k += 1
        seg = cum[k + 1] - cum[k]
        a = (t - cum[k]) / seg if seg > 1e-9 else 0.0
        p, q = pts[k], pts[(k + 1) % m]
        out.append((p[0] + (q[0] - p[0]) * a, p[1] + (q[1] - p[1]) * a))
    return out


def tunnel(path, secs, rock, s_list, slices=None, core=None, fade=None, n=RING_N):
    """Displaced ring tube along a path. Rings stand vertical (yaw-only frames), so a
    level hull meets level joints. Returns (verts, tris) and per-ring info:
    (s, (x, y, z, yaw), [(u, v)] final ring points, section name)."""
    rings, cents, info = [], [], []
    for i, s in enumerate(s_list):
        x, y, z, yaw = path.at(s)
        _, (rx, ry) = yaw_dirs(yaw)
        rd, kind, samp, name = secs.at(s)
        sl = None
        if slices:
            # clamp against this ring's slice and its neighbours', so the core's loft
            # between rings (where the margin steps) stays clear of the wall too
            nb = slices[max(0, i - 1):i + 2]
            sl = (min(q[0] for q in nb), max(q[1] for q in nb), min(q[2] for q in nb), max(q[3] for q in nb))
        pts = []
        for k, (c, sn) in enumerate(ANG):
            r = rd[k]
            if sl:
                r = smax(r, ray_rrect(c, sn, sl) + BASE_GAP, SMAX_K)
            pts.append((r * c, r * sn))
        pts = resample(pts, n)
        ring, uv = [], []
        for j in range(n):
            pu, pv = pts[j]
            a, b = pts[(j - 2) % n], pts[(j + 2) % n]
            tu, tv = b[0] - a[0], b[1] - a[1]
            tl = math.hypot(tu, tv) or 1.0
            nu, nv = tv / tl, -tu / tl
            p0 = (x + rx * pu, y + ry * pu, z + pv)
            amp = samp * (fade(p0) if fade else 1.0)
            du = dv = 0.0
            if amp > 0.0:
                dx, dy, dz = rock.displace(p0, (nu * rx, nu * ry, nv), kind)
                du, dv = (dx * rx + dy * ry) * amp, dz * amp
                if sl:
                    du, dv = clamp2(pu, pv, du, dv, sl)
                elif core:
                    d3 = clamp3(p0, (rx * du, ry * du, dv), core)
                    du, dv = d3[0] * rx + d3[1] * ry, d3[2]
            ring.append(U.Vector(x + rx * (pu + du), y + ry * (pu + du), z + pv + dv))
            uv.append((pu + du, pv + dv))
        rings.append(ring)
        cents.append((x, y, z))
        info.append((s, (x, y, z, yaw), uv, name))
    return ring_tube(rings, cents), info


def rock_ellipsoid(c, r, rock, kind='silt', res=45.0, rot_yaw=0.0, amp=1.0, fade=None, core=None, outward=False):
    """Ellipsoid of water centred at c with radii r = (rx, ry, rz), its surface
    displaced by the rock structure (and clamped against the core if given)."""
    cx, cy, cz = c
    ax, ay, az = r
    nth = max(16, int(2 * math.pi * max(ax, ay) / res))
    nph = max(8, int(math.pi * az / res) + 2)
    (fx, fy), (rx, ry) = yaw_dirs(rot_yaw)

    def vert(ex, ey, ez):
        lx, ly, lz = ax * ex, ay * ey, az * ez
        p0 = (cx + fx * lx + rx * ly, cy + fy * lx + ry * ly, cz + lz)
        nx, ny, nz = ex / ax, ey / ay, ez / az
        nl = math.sqrt(nx * nx + ny * ny + nz * nz)
        nx, ny, nz = nx / nl, ny / nl, nz / nl
        n3 = (fx * nx + rx * ny, fy * nx + ry * ny, nz)
        a = amp * (fade(p0) if fade else 1.0)
        d = (0.0, 0.0, 0.0)
        if a > 0.0:
            d = rock.displace(p0, n3, kind)
            d = (d[0] * a, d[1] * a, d[2] * a)
            if outward:
                dn = d[0] * n3[0] + d[1] * n3[1] + d[2] * n3[2]
                if dn < 0.0:
                    d = (d[0] - dn * n3[0], d[1] - dn * n3[1], d[2] - dn * n3[2])
            if core:
                d = clamp3(p0, d, core)
        return U.Vector(p0[0] + d[0], p0[1] + d[1], p0[2] + d[2])

    verts, tris = [], []
    for i in range(1, nph):
        ph = math.pi * i / nph
        sp, cp = math.sin(ph), math.cos(ph)
        for j in range(nth):
            th = 2 * math.pi * j / nth
            verts.append(vert(sp * math.cos(th), sp * math.sin(th), cp))
    top = len(verts); verts.append(vert(0.0, 0.0, 1.0))
    bot = len(verts); verts.append(vert(0.0, 0.0, -1.0))
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


def shaft(cx, cy, z0, z1, R, rock, res=50.0, flute=28.0, seed=0):
    """Vertical shaft of water: radius R plus outward-only relief (bed ledges and
    vertical flutes), so its clear diameter never drops below 2R."""
    n = max(24, int(2 * math.pi * R / res))
    nz = max(2, int((z1 - z0) / res))
    kf = max(6, int(round(2 * math.pi * R / 190.0)))
    verts, tris = [], []
    for i in range(nz + 1):
        z = z0 + (z1 - z0) * i / nz
        for j in range(n):
            th = 2 * math.pi * j / n
            x, y = cx + R * math.cos(th), cy + R * math.sin(th)
            zb = rock.zb(x, y, z)
            d = rock.wall(x, y, zb, rock.bed(zb)) + 45.0
            fl = 0.5 + 0.5 * math.cos(kf * th + 2.5 * fbm(x, y, z * 0.2, seed + 1, 300.0))
            d = max(0.0, d) + flute * fl
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


def joint_dome(cx, cy, z_base, a, b, strike, rock, rnd, levels=3, res=50.0, shrink=0.22, n=3.5):
    """Stepped breakdown dome of water, as a list of closed slabs. Each slab spans two or
    three beds and ends in a flat bedding-plane ceiling; each is smaller than the one below
    and a little offset; its outline is squared to a joint strike and its walls carry the
    bedded, blocky relief (outward only). Reads as roof beds that fell along joints, not as
    a round bubble (a single ellipsoid seen from below showed as an eye: a round rim with
    its bedding-snapped crown as the pupil)."""
    (fx, fy), (rx, ry) = yaw_dirs(strike)
    out = []
    z0 = z_base
    ox = oy = 0.0
    for lev in range(levels):
        kb = rock.bed(rock.zb(cx, cy, z0 + 80.0))
        kt = min(len(rock.beds) - 1, kb + 1 + rnd.randint(1, 2))
        top_b = rock.beds[kt]
        f = (1.0 - shrink * lev) * rnd.uniform(0.92, 1.05)
        al, bl = a * f, b * f
        if lev:
            ox += rnd.uniform(-0.12, 0.12) * a
            oy += rnd.uniform(-0.10, 0.10) * b
        ccx, ccy = cx + fx * ox + rx * oy, cy + fy * ox + ry * oy

        def zc(x, y):
            return top_b + x * rock.dip[0] + y * rock.dip[1]
        zlo = z0 - 120.0
        nth = max(24, int(2 * math.pi * max(al, bl) / res))
        ring = []
        for j in range(nth):
            th = 2 * math.pi * j / nth
            c, s = math.cos(th), math.sin(th)
            rho = ((abs(c) / al) ** n + (abs(s) / bl) ** n) ** (-1.0 / n)
            dx, dy = fx * c + rx * s, fy * c + ry * s
            ring.append((ccx + dx * rho, ccy + dy * rho, dx, dy))
        ztop = zc(ccx, ccy)
        nz = max(2, int((ztop - zlo) / res))
        verts, tris = [], []
        for i in range(nz + 1):
            t = i / nz
            for x0, y0, dx, dy in ring:
                z = zlo + (zc(x0, y0) - zlo) * t
                zb = rock.zb(x0, y0, z)
                d = max(0.0, rock.wall(x0, y0, zb, rock.bed(zb)) + 45.0)
                x, y = x0 + dx * d, y0 + dy * d
                if i == nz:
                    z = zc(x, y)
                verts.append(U.Vector(x, y, z))
        for i in range(nz):
            a0, b0 = i * nth, (i + 1) * nth
            for j in range(nth):
                j1 = (j + 1) % nth
                tris.append(U.IntVector(a0 + j, a0 + j1, b0 + j))
                tris.append(U.IntVector(a0 + j1, b0 + j1, b0 + j))
        ci = len(verts); verts.append(U.Vector(ccx, ccy, zlo))
        ti = len(verts); verts.append(U.Vector(ccx, ccy, ztop))
        top0 = nz * nth
        for j in range(nth):
            j1 = (j + 1) % nth
            tris.append(U.IntVector(ci, j1, j))
            tris.append(U.IntVector(ti, top0 + j, top0 + j1))
        out.append((verts, tris))
        z0 = ztop
    return out


def obox(c, size, yaw=0.0, pitch=0.0, roll=0.0, rnd=None, jit=0.0):
    """Oriented box (8 corners); with rnd and jit the corners wander (angular blocks)."""
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                x, y, z = sx * hx, sy * hy, sz * hz
                if rnd and jit:
                    x += rnd.uniform(-jit, jit) * hx
                    y += rnd.uniform(-jit, jit) * hy
                    z += rnd.uniform(-jit, jit) * hz
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


def slot(cx, cy, strike, half_len, t_bot, t_top, z0, z1):
    """Joint slot: a thin vertical slab along a joint plane, thinner at the top."""
    (fx, fy), (rx, ry) = yaw_dirs(strike)
    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                t = (t_bot if sz < 0 else t_top) * 0.5
                z = z0 if sz < 0 else z1
                corners.append(U.Vector(cx + fx * sx * half_len + rx * sy * t, cy + fy * sx * half_len + ry * sy * t, z))
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


# ---------------------------------------------------------------- tunnel features
def _ring_floor(uv, u=0.0, w=45.0):
    vs = [q[1] for q in uv if abs(q[0] - u) < w and q[1] < 0.0]
    return min(vs) if vs else None


def _ring_roof(uv, u=0.0, w=45.0):
    vs = [q[1] for q in uv if abs(q[0] - u) < w and q[1] > 0.0]
    return max(vs) if vs else None


def _ring_wall(uv, side, v, w=45.0):
    us = [q[0] for q in uv if abs(q[1] - v) < w and q[0] * side > 0.0]
    return (max(us) if side > 0 else min(us)) if us else None


def tunnel_features(sh, info, rock, rnd, L, core, fade, slot_p=0.45, alcove_every=(600.0, 1000.0),
                    dome_secs=(), dome_every=1500.0, block_secs=(), blocks=0, edge=800.0):
    """Joint slots, alcoves, domes and breakdown along a tunnel (all but blocks add water)."""
    notes = {'slots': 0, 'alcoves': 0, 'domes': 0, 'blocks': 0}
    # joint slots where a joint plane crosses the passage
    last = -1e9
    for f in range(len(rock.fam)):
        ang = rock.fam[f][0]
        prev = None
        for s, (x, y, z, yaw), uv, name in info:
            c = rock.cell(x, y, f)
            if prev is not None and c != prev and edge < s < L - edge and abs(s - last) > 450.0 \
                    and rnd.random() < slot_p:
                fl, rf = _ring_floor(uv), _ring_roof(uv)
                if fl is None or rf is None:
                    prev = c
                    continue
                (fx, fy), _ = yaw_dirs(yaw)
                (gx, gy), _ = yaw_dirs(ang)
                sn = max(0.3, abs(fx * gy - fy * gx))
                halfw = max(abs(q[0]) for q in uv)
                depth = rnd.uniform(150.0, 400.0)
                t = rnd.uniform(45.0, 90.0)
                sh.add(slot(x, y, ang, min(900.0, halfw / sn + depth), t, 0.55 * t, z + fl + 40.0, z + rf + 400.0))
                notes['slots'] += 1
                last = s
            prev = c
    # bedding-plane notches: long, low recesses in the thickest bed near a random height,
    # roof and floor snapped to that bed's planes (a weak bed dissolves back further than
    # its neighbours). Round-2 alcoves were full-height ellipsoids whose roof and floor both
    # snapped flat, so they read as rounded-rectangle windows.
    s_next = edge + rnd.uniform(*alcove_every) * 0.5
    side = 1 if rnd.random() < 0.5 else -1
    nb = len(rock.beds) - 2
    for s, (x, y, z, yaw), uv, name in info:
        if s < s_next or s > L - edge:
            continue
        s_next = s + rnd.uniform(*alcove_every)
        side = -side
        v = rnd.uniform(-180.0, 380.0)
        uw = _ring_wall(uv, side, v)
        if uw is None:
            continue
        (fx, fy), (rx, ry) = yaw_dirs(yaw)
        zb = rock.zb(x + rx * uw, y + ry * uw, z + v)
        k0 = rock.bed(zb)
        k = max((max(0, min(nb, k0 + d)) for d in (-1, 0, 1)), key=lambda q: rock.beds[q + 1] - rock.beds[q])
        b0, b1 = rock.beds[k], rock.beds[k + 1]
        v += 0.5 * (b0 + b1) - zb
        uw = _ring_wall(uv, side, v)
        if uw is None:
            continue
        ra, rd, rv = rnd.uniform(380.0, 700.0), rnd.uniform(160.0, 280.0), 0.42 * (b1 - b0)
        u = uw + side * 0.4 * rd
        sh.add(rock_ellipsoid((x + rx * u, y + ry * u, z + v), (ra, rd, rv), rock, 'rock', res=35.0,
                              rot_yaw=yaw + rnd.uniform(-8.0, 8.0), amp=0.85, fade=fade))
        notes['alcoves'] += 1
    # domes: stepped breakdown domes rising out of the roof
    s_next = edge + 400.0
    for s, (x, y, z, yaw), uv, name in info:
        if s < s_next or s > L - edge or name not in dome_secs:
            continue
        s_next = s + dome_every * rnd.uniform(0.8, 1.2)
        rf = _ring_roof(uv)
        if rf is None:
            continue
        _, (rx, ry) = yaw_dirs(yaw)
        u = rnd.uniform(-120.0, 120.0)
        strike = rock.fam[rnd.randrange(len(rock.fam))][0] + rnd.uniform(-5.0, 5.0)
        for vt in joint_dome(x + rx * u, y + ry * u, z + rf - 150.0, rnd.uniform(320.0, 460.0), rnd.uniform(260.0, 380.0),
                             strike, rock, rnd, levels=rnd.choice((2, 3))):
            sh.add(vt)
        notes['domes'] += 1
    # breakdown: rock blocks on the floor, kept off the core
    cand = [q for q in info if q[3] in block_secs and edge < q[0] < L - edge]
    tries = 0
    while cand and notes['blocks'] < blocks and tries < blocks * 8:
        tries += 1
        s, (x, y, z, yaw), uv, name = cand[rnd.randrange(len(cand))]
        side = 1 if rnd.random() < 0.5 else -1
        halfw = max(abs(q[0]) for q in uv if q[0] * side > 0)
        u = side * rnd.uniform(250.0, max(260.0, halfw - 80.0))
        fl = _ring_floor(uv, u)
        if fl is None:
            continue
        sz = (rnd.uniform(110.0, 360.0), rnd.uniform(100.0, 300.0), rnd.uniform(70.0, 200.0))
        _, (rx, ry) = yaw_dirs(yaw)
        c = (x + rx * u, y + ry * u, z + fl + sz[2] * rnd.uniform(0.05, 0.3))
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, rnd.uniform(0.0, 180.0), rnd.uniform(-22.0, 22.0), rnd.uniform(-22.0, 22.0), rnd, 0.15), True)
        notes['blocks'] += 1
    return notes


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


JOINT_N = 720         # rim vertices at every joint, evenly spaced: both chunks get the same polygon


def conform_rims(mesh, exit_t, n=JOINT_N, tol=0.5):
    """Give every joint rim the same canonical polygon: n vertices evenly spaced on the
    joint circle, counted from the joint's side axis.

    Each chunk is cut at its joints on its own, so its rim was its own polygon inscribed in
    the 9 m circle. Where one rim's edge cut inside the other's, a sliver up to ~0.3 cm wide
    opened, and the background (or, with a patch behind it, an unlit crack) showed through
    as a dashed hairline round the joint. Here each rim vertex moves along the circle to the
    nearest canonical angle (an order-keeping map, so no triangle flips), and each rim edge
    that spans several canonical angles gets the missing vertices, its triangle becoming a
    fan. Both sides of a joint then share their rim edge for edge. Returns the new mesh and
    per-joint numbers; raises if a rim does not go once round the circle, one way."""
    GS_R.compact_mesh(mesh)
    pts = [(q.x, q.y, q.z) for q in GS_L.convert_vector_list_to_array(GS_Q.get_all_vertex_positions(mesh, False)[1])]
    tris = [(t.x, t.y, t.z) for t in GS_L.convert_triangle_list_to_array(GS_Q.get_all_triangle_indices(mesh, False)[1])]
    frames = joint_frames(exit_t)
    step = 2.0 * math.pi / n

    def canon(j, k):
        (cx, cy, cz), (fx, fy) = frames[j]
        ang = k * step
        c, s = JOINT_R * math.cos(ang), JOINT_R * math.sin(ang)
        return (cx - fy * c, cy + fx * c, cz + s)

    ring = {}
    for i, (x, y, z) in enumerate(pts):
        for j, ((cx, cy, cz), (fx, fy)) in enumerate(frames):
            dx, dy, dz = x - cx, y - cy, z - cz
            if abs(dx * fx + dy * fy) > tol:
                continue
            lat = -dx * fy + dy * fx
            if abs(math.hypot(lat, dz) - JOINT_R) > 1.0:
                continue
            ring[i] = (j, int(round(math.atan2(dz, lat) / step)) % n)
            break
    for i, (j, k) in ring.items():
        pts[i] = canon(j, k)
    half = set()
    for a, b, c in tris:
        half.update(((a, b), (b, c), (c, a)))
    rim = []
    for a, b, c in tris:
        for u, v in ((a, b), (b, c), (c, a)):
            if u in ring and v in ring and ring[u][0] == ring[v][0] and (v, u) not in half:
                d = (ring[v][1] - ring[u][1] + n // 2) % n - n // 2
                rim.append((ring[u][0], u, v, d))
    stats = []
    for j in range(len(frames)):
        ds = [d for (jj, u, v, d) in rim if jj == j]
        turn = sum(ds)
        back = sum(1 for d in ds if d * turn < 0)
        st = {'ring_verts': sum(1 for (jj, k) in ring.values() if jj == j), 'rim_edges': len(ds),
              'collapsed': ds.count(0), 'back': back, 'turn': turn}
        if abs(turn) != n or back:
            raise RuntimeError('joint %d rim is not one clean turn: %s' % (j, st))
        stats.append(st)
    ins = {}
    for j, u, v, d in rim:
        sg = 1 if d > 0 else -1
        new = []
        for m in range(1, abs(d)):
            new.append(len(pts))
            pts.append(canon(j, (ring[u][1] + sg * m) % n))
        ins[(u, v)] = new
        stats[j]['added'] = stats[j].get('added', 0) + len(new)
    out = []
    for t in tris:
        hit = [e for e in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])) if ins.get(e)]
        if not hit:
            out.append(t)
            continue
        apex = [x for x in t if x not in hit[0]][0]
        s0 = t.index(apex)
        poly = []
        for q in range(3):
            p0, p1 = t[(s0 + q) % 3], t[(s0 + q + 1) % 3]
            poly.append(p0)
            poly.extend(ins.get((p0, p1), []))
        for q in range(1, len(poly) - 1):
            out.append((poly[0], poly[q], poly[q + 1]))
    buf = U.GeometryScriptSimpleMeshBuffers()
    buf.vertices = [U.Vector(*q) for q in pts]
    buf.triangles = [U.IntVector(*t) for t in out]
    res = U.DynamicMesh()
    GS_E.append_buffers_to_mesh(res, buf, 0)
    if GS_Q.get_num_triangle_i_ds(res) != len(out):
        raise RuntimeError('rim rebuild dropped triangles: %d of %d' % (GS_Q.get_num_triangle_i_ds(res), len(out)))
    return res, stats


def finish(mesh):
    GS_N.flip_normals(mesh)
    o = U.GeometryScriptCalculateNormalsOptions()
    GS_N.recompute_normals(mesh, o)
    if GS_Q.get_num_uv_sets(mesh) < 1:
        GS_UV.set_num_uv_sets(mesh, 1)
    GS_UV.set_mesh_u_vs_from_box_projection(mesh, 0, U.Transform(U.Vector(0, 0, 0), U.Rotator(0, 0, 0), U.Vector(4, 4, 4)), U.GeometryScriptMeshSelection(), 2)
    if GS_Q.get_num_uv_sets(mesh) < 1:
        # a static mesh without UVs asserts in its build and takes the editor down
        raise RuntimeError('mesh has no UV set')


def build_mesh(spec, cell):
    """Voxelise the spec's shapes into a cut, snapped, inward-facing shell."""
    t0 = time.time()
    sh = spec['shapes']
    dbg = spec.setdefault('debug', [])

    def note(tag):
        bb = GS_Q.get_mesh_bounding_box(m)
        dbg.append('%s tris=%d min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f) t=%.1fs' % (
            tag, GS_Q.get_num_triangle_i_ds(m), bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z,
            time.time() - t0))

    m = U.DynamicMesh()
    for vt in sh.pos:
        _append(m, vt, False)
    note('pos')
    solidify(m, cell)
    note('water')
    if sh.neg:
        for vt in sh.neg:
            _append(m, vt, True)
        solidify(m, cell)
        note('rock')
    if sh.core:
        for vt in sh.core:
            _append(m, vt, False)
        solidify(m, cell)
        note('core')
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
    m, spec['rim'] = conform_rims(m, spec['exit'])
    finish(m)
    return m, time.time() - t0, moved


def core_exposure(mesh, spec, tol=4.0):
    """Final-mesh vertices on (or inside) the clear core, away from the joint necks.
    A rock surface that lies on the core would be a flat patch; 0 is the target."""
    core = spec['core']
    res = GS_Q.get_all_vertex_positions(mesh, False)
    pts = GS_L.convert_vector_list_to_array(res[1])
    frames = joint_frames(spec['exit'])
    n = bad = 0
    mn = (1e9, None)
    for q in pts:
        near = False
        for (cx, cy, cz), (fx, fy) in frames:
            if abs((q.x - cx) * fx + (q.y - cy) * fy) < SNAP + 100.0:
                near = True
        if near:
            continue
        f = core.sdf((q.x, q.y, q.z))[0]
        n += 1
        if f < tol:
            bad += 1
        if f < mn[0]:
            mn = (f, (round(q.x), round(q.y), round(q.z)))
    return {'checked': n, 'on_core': bad, 'min_cm': round(mn[0], 1), 'at': mn[1]}


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


def section_defs(seed):
    """The section shapes for one chunk; meanders and widths vary along s."""
    rnd = random.Random(seed)
    ph = [rnd.uniform(0.0, 2 * math.pi) for _ in range(8)]
    return {
        'neck': (lambda s: sec_neck(), 'silt', 1.0, False),
        'round': (lambda s: sec_round(), 'silt', 0.9, False),
        'rift': (lambda s: sec_rift(), 'rock', 0.8, False),
        'canyon': (lambda s: sec_canyon(400.0, 70.0 * math.sin(s / 700.0 + ph[0]), 55.0 * math.sin(s / 430.0 + ph[1])),
                   'rock', 1.0, True),
        'keyhole': (lambda s: sec_keyhole(45.0 * math.sin(s / 520.0 + ph[2])), 'rock', 1.0, True),
        'widelow': (lambda s: sec_widelow(790.0 + 90.0 * math.sin(s / 900.0 + ph[3]), 770.0 + 90.0 * math.sin(s / 760.0 + ph[4])),
                    'silt', 1.0, True),
        'hall': (lambda s: sec_widelow(1150.0 + 120.0 * math.sin(s / 700.0 + ph[5]), 1050.0 + 120.0 * math.sin(s / 650.0 + ph[6]),
                                       560.0, 470.0, 3.0), 'silt', 1.0, True),
    }


def straight_core(L, m=M_BODY):
    """Swept core of a straight, level run from -EXT to L + EXT."""
    p = Path().straight(L)
    s_list = ring_s(-EXT, L + EXT)
    sl = core_slice(p, 0.0, lambda s: m)
    slices = [sl] * len(s_list)
    core = Core()
    core.add_tube(p, s_list, slices)
    return p, s_list, slices, core


def spec_A(shapes=True):
    """Straight 30 m tunnel: neck, canyon, keyhole, neck."""
    L = 3000.0
    ex = (L, 0.0, 0.0, 0.0)
    spec = {'name': 'A', 'type': 'tunnel', 'exit': ex}
    if not shapes:
        return spec
    p, s_list, slices, core = straight_core(L)
    secs = Sections([(-EXT, 'neck'), (250.0, 'neck'), (650.0, 'canyon'), (1350.0, 'canyon'), (1750.0, 'keyhole'),
                     (2300.0, 'keyhole'), (2750.0, 'neck'), (L + EXT, 'neck')], section_defs(11))
    rock = Rock(11)
    fade = lambda q: joint_fade(q, ex)
    sh = Shapes(11)
    tv, info = tunnel(p, secs, rock, s_list, slices=slices, fade=fade)
    sh.add(tv)
    notes = tunnel_features(sh, info, rock, random.Random(12), L, core, fade, dome_secs=('keyhole',), dome_every=900.0)
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'path': p, 'notes': notes})
    return spec


def spec_B(shapes=True):
    """Straight 30 m wide, low chamber between two necks: breakdown on a silt floor, domes."""
    L = 3000.0
    ex = (L, 0.0, 0.0, 0.0)
    spec = {'name': 'B', 'type': 'cavern', 'exit': ex}
    if not shapes:
        return spec
    p, s_list, slices, core = straight_core(L)
    secs = Sections([(-EXT, 'neck'), (250.0, 'neck'), (800.0, 'hall'), (2200.0, 'hall'), (2750.0, 'neck'),
                     (L + EXT, 'neck')], section_defs(22))
    rock = Rock(22)
    fade = lambda q: joint_fade(q, ex)
    rnd = random.Random(22)
    sh = Shapes(22)
    tv, info = tunnel(p, secs, rock, s_list, slices=slices, fade=fade)
    sh.add(tv)
    # lobes that break the hall's outline, and domes beyond the lamp
    for c, r, yaw in (((950.0, -950.0, -80.0), (700.0, 600.0, 430.0), 20.0), ((1550.0, 1000.0, 20.0), (820.0, 650.0, 480.0), -15.0),
                      ((2150.0, -850.0, 120.0), (650.0, 620.0, 430.0), 35.0), ((1250.0, 250.0, 380.0), (900.0, 700.0, 380.0), 5.0),
                      ((2000.0, 650.0, 260.0), (780.0, 660.0, 430.0), -30.0)):
        sh.add(rock_ellipsoid(c, r, rock, 'silt', res=45.0, rot_yaw=yaw, fade=fade, core=core))
    for c, r in (((1150.0, -320.0, 820.0), (450.0, 400.0, 900.0)), ((2050.0, 420.0, 700.0), (400.0, 380.0, 820.0))):
        sh.add(rock_ellipsoid(c, r, rock, 'rock', res=45.0, rot_yaw=rnd.uniform(0, 180), fade=fade))
    notes = tunnel_features(sh, info, rock, random.Random(23), L, core, fade, slot_p=0.3, alcove_every=(900.0, 1300.0))
    # silt floor: rock below about z = -500, rising to -470 over the last 4+ m before each neck
    def top(x, y):
        rise = 1.0 - min(sstep(250, 750, x), 1.0 - sstep(2250, 2750, x))
        return -505.0 + 35.0 * rise + 22.0 * fbm(x, y, 0, 23, 300.0)
    sh.add(heightfield(150, 2850, -1700, 1700, -1400, top), True)
    # breakdown: angular blocks, a few big ones at the sides, all clear of the core
    nb = 0
    for i in range(160):
        if nb >= 34:
            break
        big = i % 4 == 0
        x = rnd.uniform(750, 2250)
        y = rnd.uniform(-1150, 1150)
        sz = ((rnd.uniform(260, 420), rnd.uniform(220, 380), rnd.uniform(150, 280)) if big else
              (rnd.uniform(100, 260), rnd.uniform(90, 230), rnd.uniform(60, 170)))
        c = (x, y, top(x, y) + sz[2] * rnd.uniform(0.05, 0.3))
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, rnd.uniform(0, 180), rnd.uniform(-25, 25), rnd.uniform(-25, 25), rnd, 0.15), True)
        nb += 1
    notes['blocks'] = nb
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'path': p, 'notes': notes})
    return spec


T1_R = 1400.0
T1_SQUEEZE = (1260.0, 2640.0)   # level, straight rift
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


def t1_exit():
    p = t1_path()
    x, y, z, yaw = p.at(p.length)
    return (round(x, 3), round(y, 3), round(z, 3), round(yaw, 6))


def t1_margin(s):
    """0.75 m for hull positions whose grown box reaches the squeeze, 1.0 m elsewhere."""
    q0, q1 = T1_SQUEEZE
    reach = HULL_HALF[0] + M_BODY
    return M_SQUEEZE if q0 - reach <= s <= q1 + reach else M_BODY


def spec_T1(shapes=True):
    """Tunnel: canyon, a rift squeeze, keyhole, a canyon bend of 90 degrees, a wide low
    passage down a 6 m drop, a short round tube, level neck."""
    p = t1_path()
    L = p.length
    ex = t1_exit()
    spec = {'name': 'T1', 'type': 'tunnel', 'exit': ex, 'path': p, 'squeeze': T1_SQUEEZE}
    if not shapes:
        return spec
    q0, q1 = T1_SQUEEZE
    s_list = ring_s(-EXT, L + EXT)
    slices = [core_slice(p, s, t1_margin) for s in s_list]
    core = Core()
    core.add_tube(p, s_list, slices)
    secs = Sections([(-EXT, 'neck'), (250.0, 'neck'), (700.0, 'canyon'), (1000.0, 'canyon'), (q0, 'rift'), (q1, 'rift'),
                     (2900.0, 'keyhole'), (3650.0, 'keyhole'), (4050.0, 'canyon'), (6050.0, 'canyon'), (6350.0, 'widelow'),
                     (8200.0, 'widelow'), (8700.0, 'round'), (9000.0, 'round'), (L - 250.0, 'neck'), (L + EXT, 'neck')],
                    section_defs(33))
    rock = Rock(33)
    fade = lambda q: joint_fade(q, ex)
    sh = Shapes(33)
    tv, info = tunnel(p, secs, rock, s_list, slices=slices, fade=fade)
    sh.add(tv)
    notes = tunnel_features(sh, info, rock, random.Random(34), L, core, fade,
                            dome_secs=('keyhole', 'widelow', 'round'), block_secs=('widelow',), blocks=12)
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'notes': notes})
    return spec


C1_COL = (3000.0, 0.0)      # sink column axis
C1_PARK_Z = -2400.0
C1_FLOOR = -2950.0
C1_EXIT_YAW = -60.0
C1_EXIT_RUN = 3200.0
C1_COL_R = 950.0            # the shaft's clear radius before its outward relief


def c1_exit():
    (fx, fy), _ = yaw_dirs(C1_EXIT_YAW)
    return (round(C1_COL[0] + fx * C1_EXIT_RUN, 3), round(C1_COL[1] + fy * C1_EXIT_RUN, 3), C1_PARK_Z, C1_EXIT_YAW)


C1_LEDGES = [(120.0, -1750.0, 900.0), (205.0, -2150.0, 800.0), (60.0, -950.0, 700.0), (160.0, -600.0, 600.0)]
C1_LEDGE_R0 = 1250.0        # ledges start this far from the column axis
C1_PILE = (2700.0, 1650.0)


def c1_core():
    """Entry run, sink, in-place turn at Park and exit run."""
    cx, cy = C1_COL
    hx, hy, hz = core_half(M_BODY)
    core = Core()
    core.boxes.append((((cx + hx - EXT) * 0.5, 0.0, 0.0), ((cx + hx + EXT) * 0.5, hy, hz), 0.0))
    core.boxes.append(((cx, cy, C1_PARK_Z * 0.5), (hx, hy, (hz - (C1_PARK_Z - hz)) * 0.5), 0.0))
    rturn = math.hypot(HULL_HALF[0] + M_BODY, HULL_HALF[1] + M_BODY) + CORE_E
    core.cyls.append(((cx, cy, C1_PARK_Z), rturn, hz))
    (fx, fy), _ = yaw_dirs(C1_EXIT_YAW)
    a0, a1 = -hx, C1_EXIT_RUN + EXT
    mid = 0.5 * (a0 + a1)
    core.boxes.append(((cx + fx * mid, cy + fy * mid, C1_PARK_Z), (0.5 * (a1 - a0), hy, hz), C1_EXIT_YAW))
    return core


def spec_C1(shapes=True):
    """Stop cavern: high entry, a clear sink shaft, Park, a lower exit turned -60 degrees."""
    ex = c1_exit()
    cx, cy = C1_COL
    salvage = []
    for az, ztop, wid in C1_LEDGES:
        (ax, ay), _ = yaw_dirs(az)
        r = C1_LEDGE_R0 + 520.0
        salvage.append(('ledge%d' % int(az), (cx + ax * r, cy + ay * r), ztop + 250.0))
    salvage.append(('pile', C1_PILE, C1_PARK_Z + 300.0))
    salvage.append(('floor', (cx + 700.0, cy + 600.0), C1_PARK_Z))
    salvage.append(('pocket', (cx - 900.0, 1000.0), C1_PARK_Z))
    spec = {'name': 'C1', 'type': 'stop', 'exit': ex,
            'sockets': {'Park': (cx, cy, C1_PARK_Z, 0.0), 'Serpent': (cx + 400.0, -1750.0, -1500.0, 90.0)},
            'salvage': salvage}
    if not shapes:
        return spec
    rnd = random.Random(44)
    rock = Rock(44, a_res=55.0, a_blk=30.0)
    core = c1_core()
    fade = lambda q: joint_fade(q, ex)
    sh = Shapes(44)
    defs = section_defs(44)
    # entry tunnel, level, opening into the chamber
    pin = Path().straight(cx)
    secs_in = Sections([(-EXT, 'neck'), (250.0, 'neck'), (750.0, 'keyhole'), (cx, 'keyhole')], defs)
    tv, info_in = tunnel(pin, secs_in, rock, ring_s(-EXT, cx), core=core, fade=fade)
    sh.add(tv)
    # exit tunnel from the column axis, level at Park height
    pout = Path(cx, cy, C1_EXIT_YAW).straight(C1_EXIT_RUN)
    pout.zf = lambda s: C1_PARK_Z
    secs_out = Sections([(500.0, 'round'), (1500.0, 'round'), (2000.0, 'widelow'), (2500.0, 'widelow'),
                         (C1_EXIT_RUN - 250.0, 'neck'), (C1_EXIT_RUN + EXT, 'neck')], defs)
    tv, info_out = tunnel(pout, secs_out, rock, ring_s(500.0, C1_EXIT_RUN + EXT), core=core, fade=fade)
    sh.add(tv)
    # chamber: stacked ellipsoids with bedded walls
    for c, r, yaw in (((cx, cy, -2400.0), (2200.0, 1800.0, 900.0), 10.0), ((cx, cy, -1250.0), (1550.0, 1400.0, 1150.0), -15.0),
                      ((cx - 200.0, cy, 0.0), (1600.0, 1300.0, 900.0), 5.0),
                      ((cx + 1300.0, 900.0, -1700.0), (800.0, 700.0, 600.0), 30.0), ((cx - 1200.0, -800.0, -2000.0), (900.0, 700.0, 700.0), -20.0),
                      ((cx + 600.0, 1100.0, -400.0), (700.0, 600.0, 500.0), 0.0), ((cx - 800.0, 1000.0, -2500.0), (800.0, 700.0, 500.0), 40.0)):
        sh.add(rock_ellipsoid(c, r, rock, 'silt', res=45.0, rot_yaw=yaw, fade=fade, core=core))
    # domes above the shaft and the chamber roof, beyond the lamp
    sh.add(rock_ellipsoid((cx + 100.0, cy - 100.0, 1000.0), (800.0, 700.0, 1300.0), rock, 'rock', res=45.0, rot_yaw=25.0, fade=fade))
    for vt in joint_dome(cx + 900.0, -600.0, 350.0, 680.0, 520.0, rock.fam[0][0] + 4.0, rock, random.Random(45), levels=3):
        sh.add(vt)
    # serpent pocket off the -Y wall, mid height, with a narrow mouth
    sh.add(rock_ellipsoid((cx + 400.0, -1750.0, -1500.0), (520.0, 450.0, 380.0), rock, 'rock', res=40.0, rot_yaw=10.0, amp=0.5))
    sh.add(rock_ellipsoid((cx + 400.0, -1420.0, -1500.0), (300.0, 360.0, 260.0), rock, 'rock', res=40.0, amp=0.4))
    # the sink shaft: clear radius C1_COL_R, relief only outward
    sh.add(shaft(cx, cy, C1_FLOOR + 150.0, 250.0, C1_COL_R, rock, seed=57))
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
    # ledges: blocky rock shelves from the walls, tops on bedding planes, outside the shaft
    for az, ztop, wid in C1_LEDGES:
        (ax, ay), _ = yaw_dirs(az)
        mid = C1_LEDGE_R0 + 1200.0
        k = rock.bed(rock.zb(cx + ax * mid, cy + ay * mid, ztop))
        zt = rock.beds[k + 1] + (cx + ax * mid) * rock.dip[0] + (cy + ay * mid) * rock.dip[1]
        th = rnd.uniform(200.0, 300.0)
        sh.add(obox((cx + ax * mid, cy + ay * mid, zt - 0.5 * th), (2400.0, wid, th), az, rnd.uniform(-3, 3), rnd.uniform(-3, 3),
                    rnd, 0.06), True)
    # breakdown: a pile, and blocks scattered on the floor, all clear of the shaft and the core
    nb = 0
    for i in range(200):
        if nb >= 34:
            break
        if i < 90:
            a = rnd.uniform(0, 2 * math.pi); d = rnd.uniform(0, 560)
            x, y = C1_PILE[0] + d * math.cos(a), C1_PILE[1] + d * math.sin(a)
            zf = C1_FLOOR + rnd.uniform(0, 330) * (1.0 - d / 600.0)
            sz = (rnd.uniform(160, 420), rnd.uniform(140, 360), rnd.uniform(100, 260))
        else:
            a = rnd.uniform(0, 2 * math.pi); d = rnd.uniform(1300, 2000)
            x, y = cx + d * math.cos(a), cy + d * math.sin(a)
            zf = floor_top(x, y) + rnd.uniform(0, 60)
            sz = (rnd.uniform(110, 380), rnd.uniform(100, 320), rnd.uniform(70, 220))
        hd = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2)
        if math.hypot(x - cx, y - cy) - hd < C1_COL_R + 200.0:
            continue
        c = (x, y, zf)
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, rnd.uniform(0, 180), rnd.uniform(-30, 30), rnd.uniform(-30, 30), rnd, 0.15), True)
        nb += 1
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'notes': {'blocks': nb}})
    return spec


SPECS = {'A': spec_A, 'B': spec_B, 'T1': spec_T1, 'C1': spec_C1}


def meta(name):
    """Type, exit and sockets without building any shapes."""
    return SPECS[name](shapes=False)


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
    expo = core_exposure(mesh, spec)
    t2 = time.time()
    write_mesh(name, mesh, nanite)
    t3 = time.time()
    bb = GS_Q.get_mesh_bounding_box(mesh)
    sh = spec['shapes']
    return {'name': name, 'type': spec['type'], 'exit': spec['exit'], 'tris': ntri,
            'shapes': (len(sh.pos), len(sh.neg), len(sh.core)), 'notes': spec.get('notes'),
            'shapes_s': round(t1 - t0, 2), 'voxel_s': round(tbuild, 2), 'save_s': round(t3 - t2, 2),
            'total_s': round(t3 - t0, 2), 'snapped': moved, 'debug': spec.get('debug'), 'core_exposure': expo,
            'rim': spec.get('rim'),
            'min': (round(bb.min.x, 1), round(bb.min.y, 1), round(bb.min.z, 1)),
            'max': (round(bb.max.x, 1), round(bb.max.y, 1), round(bb.max.z, 1))}
