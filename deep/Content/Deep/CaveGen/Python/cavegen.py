"""DEEP cave chunk generator (editor Python, Geometry Script).

Each chunk is a closed "water" solid, voxelised by Geometry Script in three
passes:
  1. water shapes: passage rings, alcoves, domes, chambers, joint slots;
  2. minus rock shapes: breakdown blocks, ledges, floors;
  3. plus the clear core: the hull's swept box along the flight path, grown by
     1.0 m (0.75 m in squeezes), so rock can come close but never into it.
Then pinholes are closed, crumbs dropped, the ends cut on the joint planes and
snapped onto the joint type's outline, the normals turned inward, and a static
mesh baked in /Game/Deep/CaveGen/Meshes.

Round 3 (rock that catches the lamp): passages change section along each
tunnel (canyon, keyhole, wide-low, round, rift) and the walls are displaced by
a rock structure: horizontal beds (ledges, overhangs, stepped roofs and floors)
and two families of vertical joints (blocky faces, straight slots). Rock only
moves toward the core as far as a soft clamp allows, so the core never shows.

Round 4 (nothing should read as man-made): relief runs to 0.4 m from each joint; section
changes are ragged per direction; slots taper and lean instead of being boxes; beds'
ledges and grooves die out along the bed; bed undercuts replace the ellipsoid alcoves;
joints into big spaces sit back down a rough throat; the string ends in rock (chunk E).

Round 5 (typed joints, D-059): the 9 m round neck is gone. A joint has a type (bed, rift
or key), each an irregular outline that holds the hull with 1 m clear; a chunk's entry
and exit carry types, and chunks join only where they match. Runs start in chunk S, a
calm cavern with the sub's berth at its origin and no entry joint.

Frame of every chunk: the entry joint's centre is the origin (S: the berth, Park), +X
runs into the chunk, Z is up. Units are cm and degrees. Each chunk ends on a level
joint whose transform (position + yaw) is recorded as the chunk's exit.

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
                 dip=(1.2, 30.0), a_res=45.0, a_var=30.0, a_blk=25.0, a_notch=18.0, a_fine=10.0,
                 a_sharp=12.0, a_warp=50.0, roof_snap=1.0):
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
        # round 6: a sharp, ridged term (round 5's walls were all soft fbm and read as clay),
        # a slow warp of the bed frame (round 5's beds were planes, so every roof band and
        # ledge ran dead straight) and how much of a roof snaps up to its bedding plane
        self.a_sharp, self.a_warp, self.roof_snap = a_sharp, a_warp, roof_snap

    def zb(self, x, y, z):
        """Height in the bed frame (beds dip a little and sag and rise over tens of metres)."""
        w = self.a_warp * fbm(x, y, 0.0, self.seed + 37, 1600.0) if self.a_warp else 0.0
        return z - (x * self.dip[0] + y * self.dip[1]) - w

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
        # a bed's ledge and its bedding-plane groove come and go along the bed (round 3's
        # were uniform, so they ran as straight, machined lines across every face)
        gr = max(0.0, min(1.3, 0.45 + 1.2 * fbm(x / 2.0, y / 2.0, k * 37.0, self.seed + 13, 900.0)))
        gl = 0.5 + 0.7 * fbm(x / 2.5, y / 2.5, k * 53.0, self.seed + 19, 1100.0)
        d = self.a_res * self.res[k] * gl
        d += self.a_var * fbm(x / 3.5, y / 3.5, 0.5 * (z0 + z1), self.seed, 300.0)
        d += self.a_blk * _h(k, ja, jb, self.seed + 7)
        d += self.a_notch * gr * (1.0 - math.sqrt(max(0.0, math.sin(math.pi * t))))
        d += self.a_fine * fbm(x, y, zb, self.seed + 3, 120.0)
        if self.a_sharp:
            # ridged noise: crisp arrises where the noise crosses zero, broad faces between
            d += self.a_sharp * (1.0 - 2.4 * abs(fbm(x * 1.3, y * 1.3, zb, self.seed + 47, 260.0)))
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
            if nz > 0.0:      # roof: up to the bedding plane above, in patches
                up = self.beds[k + 1] - zb + 0.5 * self.a_fine * fbm(x, y, zb, self.seed + 5, 150.0)
                # round 5 snapped every roof; long flat bands read as cut. Now the snap comes
                # and goes in patches, and between them the roof undulates
                g = self.roof_snap * sstep(-0.45, 0.35, fbm(x, y, k * 29.0, self.seed + 41, 900.0))
                und = 24.0 * (0.5 + 0.5 * fbm(x, y, zb, self.seed + 43, 420.0))
                dz = rw * max(0.0, g * up + (1.0 - g) * und)
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


def r_wedge(c, s, w0, H, lean=0.0):
    """Polar radius of a slot that tapers from half width w0 at v = 0 to nothing at v = H,
    its centreline leaning by `lean` (u per v): |u - lean v| < w0 (1 - v / H). Round 3's
    slots were boxes, so every opening above or below a passage was a clean rectangle.
    Swap c and s for a sideways wing, negate s for a trench below."""
    if s <= 0.0:
        return w0
    return w0 / (abs(c - lean * s) + w0 * s / H)


ANG = [(math.cos(-0.5 * math.pi + 2 * math.pi * k / NA), math.sin(-0.5 * math.pi + 2 * math.pi * k / NA))
       for k in range(NA)]


def sec_neck():
    return [JOINT_R] * NA


def sec_round():
    return [r_sel(c, s, 480.0, 480.0, 500.0, 470.0, 2.2) for c, s in ANG]


def sec_phreatic(lean=0.0, wing=1.0):
    """Tube dissolved along a bedding plane: a lopsided lens, a tapering wing out each side
    (longer on the right), and a narrow trench cut down its floor. Replaces round 3's
    round section, which read as a pipe."""
    out = []
    for c, s in ANG:
        r = r_sel(c, s, 520.0, 440.0, 420.0, 390.0, 2.3)
        r = max(r, r_wedge(s, c, 150.0, 950.0 * wing, 0.12 + lean))
        r = max(r, r_wedge(s, -c, 105.0, 720.0 * wing, -0.08 + lean))
        r = max(r, r_wedge(c, -s, 170.0, 780.0, 0.25 - lean))
        out.append(r)
    return out


def sec_canyon(w=400.0, off=0.0, off2=0.0):
    """Tall narrow canyon: 8 m wide at the hull, narrowing up into a leaning, tapering slot
    up to 17 m tall."""
    out = []
    for c, s in ANG:
        r = r_sel(c, s, w, 0.92 * w, 500.0, 470.0, 3.0)
        r = max(r, r_wedge(c, s, 330.0, 1150.0, off / 380.0))
        r = max(r, r_wedge(c, s, 200.0, 1700.0, off2 / 300.0))
        out.append(r)
    return out


def sec_keyhole(off=0.0):
    """Rounded tube over a narrow, tapering, leaning slot."""
    return [max(r_sel(c, s, 450.0, 430.0, 480.0, 420.0, 2.4), r_wedge(c, -s, 175.0, 1050.0, off / 350.0))
            for c, s in ANG]


def sec_widelow(aR, aL, bT=450.0, bB=440.0, n=3.5):
    """Wide low passage; its roof snaps to a bedding plane."""
    return [r_sel(c, s, aR, aL, bT, bB, n) for c, s in ANG]


def sec_rift(lean=0.22, w_top=175.0, lean_bot=-0.15):
    """Squeeze: a narrow, leaning rift that tapers up into a roof slot and down into a floor
    slot; the core (plus an uneven gap) sets its width. Round 8b: the slots' lean and width
    vary along s (T1's section_defs), so seen end-on they do not stack into one notch."""
    return [max(r_sel(c, s, 290.0, 265.0, 430.0, 390.0, 2.6), r_wedge(c, s, w_top, 1100.0, lean),
                r_wedge(c, -s, 140.0, 650.0, lean_bot)) for c, s in ANG]


def sec_throat():
    """Where a joint meets a big space: lips of rock that stand inside the 9 m circle at
    the sides (the core still clears them), a slot up into the roof and a lower one down,
    so from the big space the circle sits back behind a ragged mouth."""
    return [max(r_sel(c, s, 380.0, 350.0, 480.0, 450.0, 2.4), r_wedge(c, s, 150.0, 820.0, -0.2),
                r_wedge(c, -s, 110.0, 540.0, 0.25)) for c, s in ANG]


def sec_pinch(q):
    """A bedding-plane passage that closes down: q from 0 (open, 14 m by 7 m) to 1 (shut).
    (Round 7b: it holds its height longer and narrows more, so it closes in the last
    metres, where the breakdown choke fills it, not in one long smooth wedge.)"""
    h = max(0.0, 1.0 - q) ** 0.7
    w = 1.0 - 0.7 * q
    return [r_sel(c, s, 720.0 * w, 640.0 * w, max(8.0, 380.0 * h), max(8.0, 340.0 * h), 2.6) for c, s in ANG]


def jag(rnd, lobes=(2, 3, 4, 5), amp=0.3):
    """Smooth angular noise in [-amp, amp] over the NA polar samples."""
    terms = [(k, rnd.uniform(0.0, 2 * math.pi), rnd.uniform(0.4, 1.0)) for k in lobes]
    tot = sum(a for _, _, a in terms)
    out = []
    for i in range(NA):
        th = 2 * math.pi * i / NA
        out.append(amp * sum(a * math.sin(k * th + p) for k, p, a in terms) / tot)
    return out


# ---------------------------------------------------------------- joint types (D-059)
# Round 4 joined every chunk on the same 9 m circle; from the helm each joint read as a
# round tube. Now a joint has a type, and each type is its own irregular outline, taken
# from real passage cross-sections (Palmer, Cave Geology; Ford and Williams, Karst
# Hydrogeology and Geomorphology): a bedding-plane slot, a rift and a keyhole. Both sides
# of a joint get the same outline vertex for vertex, so chunks join only where the types
# match. Each outline is a polar radius about the joint centre (the hull's axis) and holds
# the hull's 3.3 x 5.0 m section with 1.0 m clear on every side, plus the core's extras.
JOINT_TYPES = ('bed', 'rift', 'key')
JOINT_SLICE = (-(HULL_HALF[1] + M_BODY + CORE_E), HULL_HALF[1] + M_BODY + CORE_E,
               -(HULL_HALF[2] + M_BODY + CORE_E), HULL_HALF[2] + M_BODY + CORE_E)
JOINT_MIN_GAP = 40.0  # each outline stays at least this far (cm) outside the core's rounded rectangle


def _rot(c, s, deg):
    """Direction (c, s) seen in a shape turned by deg (counter-clockwise, u right, v up)."""
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    return c * ca + s * sa, s * ca - c * sa


def _bed_wing(u, v, sg, h0, h1, ue, lean, cap):
    """Inside test for one wing of the bed slot: a band along the bed on side sg (+1 right,
    -1 left), centre line leaning `lean` (v per |u|), half height h0 at the lens tapering to
    h1 at |u| = ue, where it ends in a blunt, rounded cap of half length `cap`."""
    a = sg * u
    if a < 0.0:
        return False
    vc = lean * a
    if a <= ue:
        return abs(v - vc) < h0 + (h1 - h0) * a / ue
    ve = lean * ue
    return ((a - ue) / cap) ** 2 + ((v - ve) / h1) ** 2 < 1.0


def _jt_bed(c, s):
    """Bedding-plane slot: wide and low, its flat roof and floor dipping 7 degrees to the
    right with the beds, with a wing out along the bed each side. Round 5's wings tapered
    to points, so with the lens the slot read as an eye; now each wing keeps most of its
    height and ends blunt and round, the right one stepping up the bed and the left down."""
    c1, s1 = _rot(c, s, -7.0)
    r = r_sel(c1, s1, 700.0, 640.0, 440.0, 438.0, 3.4)
    # the wings are not star-shaped as formulas, so march out along the ray and keep the
    # last point inside either wing
    last = 0.0
    q = r
    while q < 1500.0:
        u, v = q * c1, q * s1
        if _bed_wing(u, v, 1, 205.0, 135.0, 900.0, 0.075, 150.0) or _bed_wing(u, v, -1, 165.0, 110.0, 700.0, -0.07, 125.0):
            last = q
        q += 2.0
    return max(r, last)


def _jt_rift(c, s):
    """Rift: tall and narrow, leaning 11 degrees to the right, tapering up into a roof
    slot and down into a floor slot."""
    c1, s1 = _rot(c, s, -9.0)
    r = r_sel(c1, s1, 385.0, 395.0, 600.0, 545.0, 3.2)
    r = max(r, r_wedge(c1, s1, 205.0, 1380.0, 0.10))
    r = max(r, r_wedge(c1, -s1, 150.0, 830.0, -0.12))
    return r


# Round 8: the keyhole's trench was a straight wedge, so from the helm it read as a doorway
# cut in the rock. Real keyholes (Palmer, Cave Geology ch. 7; Ford and Williams ch. 7;
# Jameson, GSA 2004, on entrenchment in the Greenbrier karst): an older phreatic tube whose
# rounded floor survives as a shoulder each side, a vadose canyon cut down through it,
# narrower than the tube, meandering, its walls broken by undercuts, notches and ledges in
# groups at each stillstand of the stream, the outside of a bend overhung and the inside
# stepped, the floor a narrow rounded slot. A spring-line notch marks the bedding plane the
# tube grew along. Each trench wall here is a profile down from the tube floor: it narrows
# with depth, and in each stage it recedes as it goes down (an undercut under a ledge) and
# then steps in at the next ledge. Undercuts stay within what a ray from the joint centre
# can see (a wall's outward slope under its distance over depth), so the outline stays
# star-shaped for the rims.
KEY_LINE = (20.0, 0.06, 20.0, 220.0)     # trench line: offset, lean (u per cm depth), meander amp, wavelength
# each wall's stages, top down: (depth of the ledge that starts it, how far the wall steps
# in at that ledge, how far it recedes below it). The outside of the bend (right) is
# overhung, the inside (left) stepped
KEY_RIGHT = (175.0, ((470.0, 0.0, 22.0), (585.0, 15.0, 24.0), (715.0, 20.0, 20.0), (835.0, 20.0, 16.0),
                     (950.0, 25.0, 10.0), (1050.0, 25.0, 6.0)))
KEY_LEFT = (150.0, ((470.0, 0.0, 14.0), (600.0, 25.0, 16.0), (720.0, 25.0, 14.0), (840.0, 20.0, 10.0),
                    (960.0, 20.0, 8.0), (1060.0, 15.0, 4.0)))
KEY_TOP, KEY_BOT = 380.0, 1150.0


def _key_half(d, wall):
    """Half width of one trench wall at depth d. Down the wall it steps in at each ledge;
    below each ledge it recedes (an undercut) until, just above the next, it comes back in
    over that ledge's sloping top. The recess stays within what a ray from the joint
    centre sees, so the outline stays star-shaped."""
    h, stages = wall
    k = 0
    while k + 1 < len(stages) and d >= stages[k + 1][0]:
        k += 1
    h -= sum(st[1] for st in stages[:k + 1])
    d0, _, under = stages[k]
    if d < d0:
        return h
    d1 = stages[k + 1][0] if k + 1 < len(stages) else KEY_BOT
    t = (d - d0) / max(1.0, d1 - d0)
    return h + under * (t / 0.8 if t < 0.8 else 1.0 - smooth((t - 0.8) / 0.2))


def _key_notch(u, v, sg, vc, lean, h, ue, cap):
    """A spring-line notch on side sg: a band of half height h along the bed (centre vc,
    rising `lean` per cm out) to |u| = ue, ending in a round cap of half length cap."""
    a = sg * u
    if a < 0.0:
        return False
    w = vc + lean * a
    if a <= ue:
        return abs(v - w) < h
    return ((a - ue) / cap) ** 2 + ((v - vc - lean * ue) / h) ** 2 < 1.0


def _key_inside(u, v):
    d = -v
    # the tube: rounder on top, its floor wider and lower on the left
    a = 600.0 if u >= 0 else 565.0
    b = 520.0 if v >= 0 else (470.0 if u >= 0 else 485.0)
    if (abs(u) / a) ** 2.5 + (abs(v) / b) ** 2.5 < 1.0:
        return True
    # notches along the bedding plane the tube grew on, deeper on the right
    if _key_notch(u, v, 1, 40.0, 0.05, 48.0, 560.0, 110.0) or _key_notch(u, v, -1, -75.0, -0.03, 36.0, 500.0, 90.0):
        return True
    if d < KEY_TOP or d > KEY_BOT + 110.0:
        return False
    off, lean, amp, wl = KEY_LINE
    cl = off + lean * (d - KEY_TOP) + amp * math.sin((d - KEY_TOP) / wl)
    hr = _key_half(d, KEY_RIGHT)
    hl = _key_half(d, KEY_LEFT)
    if d > KEY_BOT:
        # a rounded slot floor
        e = (d - KEY_BOT) / 110.0
        w = math.sqrt(max(0.0, 1.0 - e * e))
        return cl - hl * w < u < cl + hr * w
    return cl - hl < u < cl + hr


def _jt_key(c, s):
    """Keyhole: a wide phreatic tube, lower on the left, with a notch along its spring line,
    over a vadose trench cut down through its floor: a meandering canyon whose walls step
    and undercut, and a rounded slot at the bottom."""
    last = 0.0
    q = 2.0
    while q < 1500.0:
        if _key_inside(q * c, q * s):
            last = q
        q += 2.0
    return last


_JT_FN = {'bed': (_jt_bed, 71), 'rift': (_jt_rift, 72), 'key': (_jt_key, 73)}
_JT_CACHE = {}


def joint_outline(t):
    """Radii over ANG for joint type t: its base shape with a fixed, low-frequency jag, so
    the outline is irregular but identical in every chunk."""
    if t not in _JT_CACHE:
        fn, seed = _JT_FN[t]
        j = jag(random.Random(seed), lobes=(3, 4, 6, 7), amp=0.06)
        r = [fn(c, s) * (1.0 + j[k]) for k, (c, s) in enumerate(ANG)]
        # round the slot tips over about 2 degrees, so no tip is thinner than a voxel (round 8:
        # the keyhole's trench walls run nearly along the rays, where 2 degrees is a metre or
        # more of wall and would blur its ledges, so it is smoothed over 0.4 degrees)
        hw = 1 if t == 'key' else 5
        for _ in range(2):
            r = [sum(r[(k + d) % NA] for d in range(-hw, hw + 1)) / (2.0 * hw + 1.0) for k in range(NA)]
        _JT_CACHE[t] = r
    return _JT_CACHE[t]


def _tab_at(tab, th):
    f = ((th + 0.5 * math.pi) / (2.0 * math.pi) * NA) % NA
    i = int(f)
    a = f - i
    return tab[i % NA] * (1.0 - a) + tab[(i + 1) % NA] * a


def joint_base_r(t, th):
    """Type t's outline radius at angle th before its rock relief."""
    return _tab_at(joint_outline(t), th)


# Round 8: the joint's own rock. Rounds 6 to 7b faded every chunk's relief to nothing at
# each joint plane (the rock over 6.5 m, the fine relief over 3.5 m), so both sides met
# on the bare outline: from the helm each joint showed as a smooth band and a clean cut
# ring. Now each joint type carries a relief of its own, fixed in the joint's frame (a
# along the direction of travel, lat across it, z up), so the chunks either side of a
# joint evaluate the same rock and it runs through the plane. It is a radial offset from
# the joint centre, a function of angle and a only, so the rim (a = 0) is still one polar
# outline, the same vertex for vertex on both sides. Bedded and jointed like the chunks'
# rock; it hands over to the chunk's own relief over FADE_OUTER.
JREL_OUT = 75.0       # the relief's soft limit outward (cm)
JREL_IN = 40.0        # and inward, never closer than JREL_KEEP to the core
JREL_KEEP = 45.0
JREL_COS = 0.5        # a wall's offset along its normal becomes a radial offset of at most 1 / JREL_COS times it
_JREL = {}


def _jrel_geo(t):
    """Per sample of type t's outline: the base point (lat, z), the cosine between the
    ray and the outline's normal, the gap to the core, and the joint's rock."""
    if t not in _JREL:
        tab = joint_outline(t)
        pts = [(r * c, r * s) for r, (c, s) in zip(tab, ANG)]
        cs, gaps = [], []
        for k, (c, s) in enumerate(ANG):
            a, b = pts[k - 3], pts[(k + 3) % NA]
            tu, tv = b[0] - a[0], b[1] - a[1]
            tl = math.hypot(tu, tv) or 1.0
            cs.append(abs(c * tv / tl - s * tu / tl))
            gaps.append(tab[k] - ray_rrect(c, s, JOINT_SLICE))
        rock = Rock(_JT_FN[t][1] + 100, a_res=42.0, a_var=28.0, a_blk=14.0, a_notch=8.0, a_fine=5.0, a_sharp=5.0,
                    a_warp=40.0)
        _JREL[t] = (pts, cs, gaps, rock)
    return _JREL[t]


def _jrel_raw(t, k, a):
    pts, cs, gaps, rock = _jrel_geo(t)
    u, v = pts[k]
    zb = rock.zb(a, u, v)
    d = rock.wall(a, u, zb, rock.bed(zb)) / max(cs[k], JREL_COS)
    if d >= 0.0:
        return JREL_OUT * math.tanh(d / JREL_OUT)
    lim = max(0.0, min(JREL_IN, gaps[k] - JREL_KEEP))
    return -lim * math.tanh(-d / lim) if lim > 0.0 else 0.0


# Round 8b: the rock above is evaluated at each outline sample on its own. Where the
# outline runs nearly along the rays (a slot wall), 1 / cos makes neighbouring samples
# disagree by tens of cm, so the rim zigzagged (rift at 80 degrees: 914, 878, 918, 876 cm
# over 1.5 degrees) and the snap folded the surface onto it: bright lips round the rims.
# The relief is now smoothed over nine samples (binomial, 3.6 degrees) and cached in 5 cm steps of a,
# linear between them, so the rim keeps its rock but has no sawtooth.
JREL_SMOOTH = (1.0, 8.0, 28.0, 56.0, 70.0, 56.0, 28.0, 8.0, 1.0)
JREL_ABIN = 5.0
_JREL_BIN = {}


def _jrel_bin(t, ia):
    key = (t, ia)
    row = _JREL_BIN.get(key)
    if row is None:
        row = _JREL_BIN[key] = [None] * NA
    return row


def _jrel_k(t, k, a):
    """Type t's rock at outline sample k, a cm along travel from the plane: the smoothed,
    cached form of _jrel_raw."""
    f = a / JREL_ABIN
    ia = math.floor(f)
    w = f - ia
    r0, r1 = _jrel_bin(t, ia), _jrel_bin(t, ia + 1)
    h = len(JREL_SMOOTH) // 2
    tot = 0.0
    for d, wt in zip(range(-h, h + 1), JREL_SMOOTH):
        kk = (k + d) % NA
        v0 = r0[kk]
        if v0 is None:
            v0 = r0[kk] = _jrel_raw(t, kk, ia * JREL_ABIN)
        if w > 0.0:
            v1 = r1[kk]
            if v1 is None:
                v1 = r1[kk] = _jrel_raw(t, kk, (ia + 1) * JREL_ABIN)
            v0 += (v1 - v0) * w
        tot += wt * v0
    return tot / sum(JREL_SMOOTH)


def joint_relief(t, th, a):
    """Radial offset (cm, + away from the water) of type t's rock at angle th, a cm along
    the direction of travel from the joint plane."""
    f = ((th + 0.5 * math.pi) / (2.0 * math.pi) * NA) % NA
    i = int(f)
    w = f - i
    return _jrel_k(t, i % NA, a) * (1.0 - w) + _jrel_k(t, (i + 1) % NA, a) * w


_RIM_CACHE = {}


RIM_TURN = 30.0       # round 8b: the most the rim polygon turns at one sample (degrees)
RIM_RHO = 30.0        # round 8b: the rim's smallest feature, as a disc radius (cm)


def _rim_morph(rs, rho=None):
    """Round 8b: no feature of the rim narrower than a 2*rho disc. Opening (the union of
    every rho disc that fits in the water) takes off the outline's narrow slot tips;
    closing (the same in the rock) takes off narrow rock noses. The rift's slot tips were
    50 cm wide and over a metre deep at the plane; neither chunk's 25 cm voxel surface
    followed them, so the rim alone went into them and the two fans lay flat in the plane
    there, a fin that caught the light. Returns radii along the same rays."""
    rho = RIM_RHO if rho is None else rho
    n = len(rs)
    P = [(r * c, r * sn) for r, (c, sn) in zip(rs, ANG)]
    dth = 2.0 * math.pi / n

    def seg_d2(c, a, b):
        ex, ey = b[0] - a[0], b[1] - a[1]
        L2 = ex * ex + ey * ey
        q = 0.0 if L2 < 1e-12 else max(0.0, min(1.0, ((c[0] - a[0]) * ex + (c[1] - a[1]) * ey) / L2))
        return (a[0] + ex * q - c[0]) ** 2 + (a[1] + ey * q - c[1]) ** 2

    def discs(sign):
        out = []
        for k in range(n):
            a, b = P[k - 1], P[(k + 1) % n]
            tx, ty = b[0] - a[0], b[1] - a[1]
            L = math.hypot(tx, ty)
            if L < 1e-9:
                continue
            nx, ny = ty / L, -tx / L                      # out of the water (ANG runs anticlockwise)
            c = (P[k][0] + sign * rho * nx, P[k][1] + sign * rho * ny)
            rc = math.hypot(*c)
            W = n if rc < 3.0 * rho else int(math.asin(min(1.0, (rho + 10.0) / rc)) / dth) + 4
            k0 = int(round((math.atan2(c[1], c[0]) + 0.5 * math.pi) / dth)) % n
            js = range(n) if W >= n // 2 else range(k0 - W, k0 + W + 1)
            if all(seg_d2(c, P[j % n], P[(j + 1) % n]) >= (rho - 0.5) ** 2 for j in js):
                out.append((c, rc, k0))
        return out

    def rays(dl, keep):
        res = [None] * n
        for c, rc, k0 in dl:
            W = n // 2 if rc <= rho else int(math.asin(rho / rc) / dth) + 2
            for j in range(k0 - W, k0 + W + 1):
                cj, sj = ANG[j % n]
                cd = c[0] * cj + c[1] * sj
                disc = rho * rho - rc * rc + cd * cd
                if disc < 0.0:
                    continue
                t = cd + keep * math.sqrt(disc)
                if t <= 0.0:
                    continue
                q = j % n
                if res[q] is None or keep * (t - res[q]) > 0.0:
                    res[q] = t
        return res
    far = rays(discs(-1.0), 1.0)                          # opening: the farthest water disc point
    rs = [min(r, f) if f is not None else r for r, f in zip(rs, far)]
    P = [(r * c, r * sn) for r, (c, sn) in zip(rs, ANG)]
    near = rays(discs(1.0), -1.0)                         # closing: the nearest rock disc point
    return [max(r, f) if f is not None else r for r, f in zip(rs, near)]


def _round_rim(rs):
    """Round 8b: round the rim's sharp corners. The key's ledges and the rift's relief steps
    turned the rim up to 92 degrees within one 0.4 degree sample (a 4 cm notch at a ledge's
    nose); the voxel surface either side of the plane is rounded at 25 cm, so the fan
    between them folded there whatever the snap did. Each pass moves every sample whose
    polygon turns more than RIM_TURN (and its neighbours, less) toward the midpoint of its
    neighbours, until none does; the result is read back as radii along the same rays.
    Only sharp corners move (by a few cm); the rest of the rim is unchanged."""
    n = len(rs)
    P = [(r * c, r * sn) for r, (c, sn) in zip(rs, ANG)]
    lim = math.cos(math.radians(RIM_TURN))
    for _ in range(600):
        hot = set()
        for k in range(n):
            a, b, c = P[k - 1], P[k], P[(k + 1) % n]
            e1, e2 = (b[0] - a[0], b[1] - a[1]), (c[0] - b[0], c[1] - b[1])
            l1, l2 = math.hypot(*e1), math.hypot(*e2)
            if l1 > 1e-9 and l2 > 1e-9 and (e1[0] * e2[0] + e1[1] * e2[1]) / (l1 * l2) < lim:
                hot.add(k)
        if not hot:
            break
        new = {}
        for k in hot | {(k + d) % n for k in hot for d in (-1, 1)}:
            w = 0.5 if k in hot else 0.25
            a, b, c = P[k - 1], P[k], P[(k + 1) % n]
            new[k] = (b[0] + w * (0.5 * (a[0] + c[0]) - b[0]), b[1] + w * (0.5 * (a[1] + c[1]) - b[1]))
        for k, q in new.items():
            P[k] = q
    out = []
    for k, (c, sn) in enumerate(ANG):
        best = None
        for j in range(k - 15, k + 15):
            p, q = P[j % n], P[(j + 1) % n]
            dx, dy = q[0] - p[0], q[1] - p[1]
            den = c * dy - sn * dx
            if abs(den) < 1e-12:
                continue
            u = (p[0] * sn - p[1] * c) / den
            tt = (p[0] * dy - p[1] * dx) / den
            if -1e-9 <= u <= 1.0 + 1e-9 and tt > 0.0 and (best is None or abs(tt - rs[k]) < abs(best - rs[k])):
                best = tt
        out.append(rs[k] if best is None else best)
    return out


def joint_rim(t):
    """Type t's rim over ANG: its outline plus its rock relief at the plane, with nothing
    narrower than RIM_RHO's disc and its sharp corners rounded (round 8b)."""
    if t not in _RIM_CACHE:
        _RIM_CACHE[t] = _round_rim(_rim_morph([r + _jrel_k(t, k, 0.0) for k, r in enumerate(joint_outline(t))]))
    return _RIM_CACHE[t]


def _rim_delta(t, k):
    """How far the rounding moved rim sample k (cm, along its ray)."""
    if t not in _RIM_D_CACHE:
        rim = joint_rim(t)
        _RIM_D_CACHE[t] = [rim[q] - (r + _jrel_k(t, q, 0.0)) for q, r in enumerate(joint_outline(t))]
    return _RIM_D_CACHE[t][k]


_RIM_D_CACHE = {}


def joint_r(t, th):
    """Rim radius of joint type t at angle th (radians from the joint's side axis,
    counter-clockwise toward up), interpolated between the ANG samples."""
    return _tab_at(joint_rim(t), th)


def joint_margins(t):
    """Smallest gap (cm) between type t's rim (round 8: with its rock relief) and the core's
    rounded rectangle at a level joint, and the rim's extent (u0, u1, v0, v1)."""
    tab = joint_rim(t)
    gap = min(r - ray_rrect(c, s, JOINT_SLICE) for r, (c, s) in zip(tab, ANG))
    us = [r * c for r, (c, s) in zip(tab, ANG)]
    vs = [r * s for r, (c, s) in zip(tab, ANG)]
    return gap, (min(us), max(us), min(vs), max(vs))


def check_joint_types():
    out = {}
    for t in JOINT_TYPES:
        gap, ext = joint_margins(t)
        if gap < JOINT_MIN_GAP:
            raise RuntimeError('joint type %s comes within %.1f cm of the core' % (t, gap))
        out[t] = {'gap_cm': round(gap, 1), 'width_m': round((ext[1] - ext[0]) / 100.0, 2),
                  'height_m': round((ext[3] - ext[2]) / 100.0, 2)}
    return out


class Sections:
    """Section shapes along a path. anchors: [(s, name)]; between two anchors with
    different names the radii blend smoothly. defs: name -> (fn(s) -> radii, floor kind, amp, varies)."""

    def __init__(self, anchors, defs, seed=0):
        self.anchors = anchors
        self.defs = defs
        self.cache = {}
        self.seed = seed
        self.jags = {}

    def jagged(self, i, na, nb):
        """Per-angle offsets for the blend after anchor i: each direction opens at its own
        place, so a section change is a ragged flare, not a cone. Round 5 opened the roof
        late next to a neck, to leave a lip over the joint; from the helm that lip read as a
        lintel over a door, so round 6 drops it."""
        if i not in self.jags:
            self.jags[i] = jag(random.Random(self.seed * 1000 + i), amp=0.26)
        return self.jags[i]

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
                base = max(0.0, min(1.0, (s - sa) / (sb - sa)))
                t = smooth(base)
                ra, rb = self.radii(na, s), self.radii(nb, s)
                dom = na if t < 0.5 else nb
                amp = self.defs[na][2] + (self.defs[nb][2] - self.defs[na][2]) * t
                # |j| <= 0.3 keeps each direction's blend monotone along s
                sw = math.sin(math.pi * base)
                jj = self.jagged(i, na, nb)
                return ([a + (b - a) * smooth(base + jk * sw) for a, b, jk in zip(ra, rb, jj)],
                        self.defs[dom][1], amp, dom)
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
        self.capped = False

    def add_tube(self, path, s_list, slices):
        # a tube that stops short (None slices, as in the end chunk) has a capped end; a
        # full tube runs past both joints and its ends are never reached, so it stays open
        self.capped = self.capped or any(sl is None for sl in slices)
        for s, sl in zip(s_list, slices):
            if sl:
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
        d = self._ring_sdf(k, p)
        if self.capped:
            for i, sign in ((n - 1, 1.0), (0, -1.0)):
                a = sign * self._along(i, p)          # how far past this end of the tube
                if a > 0.0:
                    d2, g = self._ring_sdf(i, p)
                    (fx, fy), _ = yaw_dirs(rings[i][1][3])
                    if d2 > 0.0:
                        dd = math.hypot(d2, a)
                        return dd, ((g[0] * d2 + sign * fx * a) / dd, (g[1] * d2 + sign * fy * a) / dd, g[2] * d2 / dd)
                    return (a, (sign * fx, sign * fy, 0.0)) if a > d2 else (d2, g)
        return d

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


def tunnel(path, secs, rock, s_list, slices=None, core=None, fade=None, n=RING_N, gap=None):
    """Displaced ring tube along a path. Rings stand vertical (yaw-only frames), so a
    level hull meets level joints. gap (round 8b), if given: gap(s, k, c, sn, rc, sl) -> cm
    more that the wall stands off the core along ray k (outward only, so clearances hold).
    Returns (verts, tris) and per-ring info: (s, (x, y, z, yaw), [(u, v)] final ring
    points, section name)."""
    rings, cents, info = [], [], []
    for i, s in enumerate(s_list):
        x, y, z, yaw = path.at(s)
        _, (rx, ry) = yaw_dirs(yaw)
        rd, kind, samp, name = secs.at(s)
        sl = None
        if slices:
            # clamp against this ring's slice and its neighbours', so the core's loft
            # between rings (where the margin steps) stays clear of the wall too; a ring
            # with no slice (past the end of the core) is not clamped
            nb = [q for q in slices[max(0, i - 1):i + 2] if q]
            if nb:
                sl = (min(q[0] for q in nb), max(q[1] for q in nb), min(q[2] for q in nb), max(q[3] for q in nb))
        fc = fade((x, y, z)) if fade else 1.0
        pts = []
        for k, (c, sn) in enumerate(ANG):
            r = rd[k]
            if sl:
                # the gap kept round the core swells unevenly (outward only), so walls the
                # core sets are not parallel to the hull's box
                rc = ray_rrect(c, sn, sl)
                bulge = 0.0
                if fc > 0.0:
                    bulge = fc * 85.0 * max(0.0, 0.3 + fbm(x + rx * rc * c, y + ry * rc * c, z + rc * sn,
                                                               rock.seed + 29, 380.0))
                if gap:
                    bulge += max(0.0, gap(s, k, c, sn, rc, sl))
                r = smax(r, rc + BASE_GAP + bulge, SMAX_K)
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
            jr = fade.relief(p0) if hasattr(fade, 'relief') else None
            if jr:
                # round 8: the joint's own rock, which both chunks share, as the chunk's fades
                du, dv = du + jr[0] * rx + jr[1] * ry, dv + jr[2]
            jn = fade.near(p0) if hasattr(fade, 'near') else 0.0
            if jn > 0.0:
                # fine relief along the ring's outward normal near a joint. Round 6 pushed it
                # outward only (mean +14 cm, peaking 1.4 m from the plane): every rim got a
                # raised ring and a crease inside it. Round 7: zero mean, both ways
                f = JFINE_AMP * jn * (0.7 * fbm(p0[0], p0[1], p0[2], rock.seed + 51, 110.0)
                                      + 0.45 * (0.3 - abs(fbm(p0[0], p0[1], p0[2], rock.seed + 53, 60.0))))
                du, dv = du + f * nu, dv + f * nv
            if du or dv:
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


def warp_blob(vt, c, amp, base, seed):
    """Push each vertex of a closed shape (verts, tris) along its ray from c by a
    low-frequency noise, so a smooth ellipsoid becomes an uneven, worn chamber whose
    bays and bulges grow out of one surface (overlapping ellipsoids meet in creases that
    read as round holes from inside)."""
    verts, tris = vt
    cx, cy, cz = c
    out = []
    for v in verts:
        dx, dy, dz = v.x - cx, v.y - cy, v.z - cz
        d = math.sqrt(dx * dx + dy * dy + dz * dz) or 1.0
        k = 1.0 + amp * fbm(2000.0 * dx / d, 2000.0 * dy / d, 2000.0 * dz / d, seed, base)
        out.append(U.Vector(cx + dx * k, cy + dy * k, cz + dz * k))
    return out, tris


def bent_tube(pts, radii, seed, res=45.0, step=50.0, amp=0.14, flute=0.0, lobe=0.0):
    """Closed tube of water along a 3D polyline pts with radii at each point (linear
    between), its rings perpendicular to the line (parallel transport) and its radius
    varied by low-frequency noise: a water-worn shaft that bends.

    With flute or lobe set (round 6), the relief is a shaft's, not a lathe's: the noise is
    sampled in the tube's own frame and stretched about 3.5 times along the axis, so it
    runs up the shaft instead of round it; flutes (outward grooves, `flute` cm deep) wander
    as they climb; and low-order lobes (`lobe` of the radius) make the outline uneven.
    Round 5 sampled isotropic noise at each ring, and from below its bands read as rings."""
    import bisect
    acc = [0.0]
    for a, b in zip(pts, pts[1:]):
        acc.append(acc[-1] + math.dist(a, b))
    L = acc[-1]

    def at(s):
        i = min(len(pts) - 2, max(0, bisect.bisect_right(acc, s) - 1))
        t = (s - acc[i]) / ((acc[i + 1] - acc[i]) or 1.0)
        a, b = pts[i], pts[i + 1]
        return tuple(a[k] + (b[k] - a[k]) * t for k in range(3)), radii[i] + (radii[i + 1] - radii[i]) * t

    def norm(v):
        l = math.sqrt(sum(x * x for x in v)) or 1.0
        return tuple(x / l for x in v)

    def cross(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])

    ns = max(2, int(L / step))
    n = max(24, int(2 * math.pi * max(radii) / res))
    rings, cents = [], []
    u = None
    for i in range(ns + 1):
        s = L * i / ns
        c, R = at(s)
        c2, _ = at(min(L, s + 60.0)); c1, _ = at(max(0.0, s - 60.0))
        t = norm(tuple(c2[k] - c1[k] for k in range(3)))
        if u is None:
            u = norm(cross((0.0, 1.0, 0.0) if abs(t[1]) < 0.9 else (1.0, 0.0, 0.0), t))
        else:
            d = sum(u[k] * t[k] for k in range(3))
            u = norm(tuple(u[k] - d * t[k] for k in range(3)))
        v = cross(t, u)
        ring = []
        kf = max(5, int(round(2 * math.pi * R / 230.0)))
        for j in range(n):
            th = 2 * math.pi * j / n
            ox = tuple(u[k] * math.cos(th) + v[k] * math.sin(th) for k in range(3))
            p0 = tuple(c[k] + R * ox[k] for k in range(3))
            if flute or lobe:
                # the tube's own frame: (ca, sa) round the axis, s/3.5 along it
                ca, sa, sz = 900.0 * math.cos(th), 900.0 * math.sin(th), s / 3.5
                rr = R * (1.0 + lobe * fbm(ca, sa, sz, seed + 9, 900.0)
                          + 0.5 * amp * fbm(ca * 1.6, sa * 1.6, sz * 1.4, seed, 500.0)
                          + 0.03 * fbm(p0[0], p0[1], p0[2], seed + 5, 120.0))
                ph = kf * th + 2.2 * fbm(ca * 0.4, sa * 0.4, s / 2.0, seed + 11, 700.0)
                fl = 0.5 + 0.5 * math.cos(ph)
                fl *= 0.6 + 0.4 * fbm(ca, sa, s / 1.5, seed + 13, 600.0)
                r = rr + flute * max(0.0, fl)
            else:
                r = R * (1.0 + amp * fbm(p0[0], p0[1], p0[2], seed, 700.0) + 0.04 * fbm(p0[0], p0[1], p0[2], seed + 5, 180.0))
            ring.append(U.Vector(*(c[k] + r * ox[k] for k in range(3))))
        rings.append(ring)
        cents.append(c)
    return ring_tube(rings, cents)


def shaft(cx, cy, z0, z1, R, rock, res=50.0, flute=40.0, seed=0):
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
            # round 7: round 6 took the bedded wall relief per height, so each bed's ledge
            # ran round the shaft and from Park it read as concentric rings. Now the relief
            # is sampled in the shaft's own frame, stretched up the axis (as bent_tube's)
            ca, sa, sz = 900.0 * math.cos(th), 900.0 * math.sin(th), z / 3.5
            d = (60.0 + 70.0 * fbm(ca, sa, sz, seed + 9, 900.0) + 35.0 * fbm(ca * 1.6, sa * 1.6, sz * 1.4, seed, 500.0)
                 + 8.0 * fbm(x, y, z, seed + 5, 120.0))
            fl = 0.5 + 0.5 * math.cos(kf * th + 2.2 * fbm(ca * 0.4, sa * 0.4, z / 2.0, seed + 1, 700.0))
            fl *= 0.6 + 0.4 * fbm(ca, sa, z / 1.5, seed + 13, 600.0)
            d = max(0.0, d) + flute * max(0.0, fl)
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


def slot_lens(cx, cy, strike, half_len, t, z0, z1, rnd, n=14):
    """Joint slot as a lens along the joint plane: thickest in the middle, pinching out at
    both ends, its top dipping toward the ends and its trace wandering a little. Round 3's
    slots were straight-edged slabs and read as cut notches."""
    (fx, fy), (rx, ry) = yaw_dirs(strike)
    ph = rnd.uniform(0.0, 6.28)
    lop = rnd.uniform(-0.25, 0.25)
    rings, cents = [], []
    for i in range(n + 1):
        a = -1.0 + 2.0 * i / n
        e = max(0.0, 1.0 - a * a)
        th = t * (0.08 + 0.92 * e ** 0.6) * (1.0 + 0.25 * math.sin(3.1 * a + ph))
        top = z0 + (z1 - z0) * (0.35 + 0.65 * e ** 0.5) * (1.0 + lop * a)
        bot = z0 + (top - z0) * 0.25 * (1.0 - e)
        w = 0.35 * t * math.sin(2.3 * a + ph)          # the trace wanders off the plane
        px, py = cx + fx * a * half_len + rx * w, cy + fy * a * half_len + ry * w
        sec = []
        for q, (dt, zz) in enumerate(((-0.5, 0.0), (-0.42, 0.45), (-0.2, 0.85), (0.0, 1.0), (0.24, 0.8),
                                      (0.45, 0.4), (0.5, 0.0))):
            hz = bot + (top - bot) * zz
            sec.append(U.Vector(px + rx * dt * th * (1.0 - 0.6 * zz), py + ry * dt * th * (1.0 - 0.6 * zz), hz))
        sec.append(U.Vector(px, py, bot - 0.15 * th))
        rings.append(sec)
        cents.append((px, py, 0.5 * (top + bot)))
    return ring_tube(rings, cents)


def bed_undercut(sh, info, i0, i1, side, k, rock, rnd, fade=None, depth=(120.0, 260.0)):
    """A weak bed dissolved back under the one above it, along the wall from ring i0 to i1:
    a wedge-shaped recess whose roof is the bedding plane, deepest in the middle and dying
    out at both ends, with its depth wandering. Replaces round 3's ellipsoid alcoves,
    which read as rounded fingers."""
    b0, b1 = rock.beds[k], rock.beds[k + 1]
    hmax = max(50.0, min(150.0, 0.6 * (b1 - b0)))
    D = rnd.uniform(*depth)
    seed = rnd.randrange(1 << 16)
    rings, cents = [], []
    for i in range(i0, i1 + 1):
        s, (x, y, z, yaw), uv, name = info[i]
        t = (i - i0) / max(1, i1 - i0)
        _, (rx, ry) = yaw_dirs(yaw)
        vt = b1 - 12.0 + x * rock.dip[0] + y * rock.dip[1] - z
        uw = _ring_wall(uv, side, vt - 0.5 * hmax, w=60.0)
        if uw is None:
            if len(rings) >= 4:
                break
            rings, cents = [], []
            continue
        wx, wy = x + rx * uw, y + ry * uw
        f = fade((wx, wy, z + vt)) if fade else 1.0
        e = math.sin(math.pi * t) ** 0.7
        dep = f * D * e * (0.7 + 0.45 * max(-0.6, fbm(wx, wy, 0.0, seed, 350.0)))
        h = hmax * (0.3 + 0.7 * e ** 0.5)
        sec = []
        for du, dv in ((-60.0, 0.0), (0.5, -0.04), (1.0, -0.35), (0.55, -0.72), (0.15, -1.0), (-60.0, -1.12)):
            uu = du if du < 0.0 else du * dep
            sec.append((uw + side * uu, vt + dv * h))
        rings.append([U.Vector(x + rx * u, y + ry * u, z + v) for u, v in sec])
        cents.append((x + rx * (uw - side * 30.0), y + ry * (uw - side * 30.0), z + vt - 0.5 * h))
    if len(rings) >= 4:
        sh.add(ring_tube(rings, cents))
        return 1
    return 0


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
    notes = {'slots': 0, 'alcoves': 0, 'domes': 0, 'blocks': 0}   # alcoves: bed undercuts since round 4
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
                t = rnd.uniform(55.0, 110.0)
                sh.add(slot_lens(x, y, ang, min(900.0, halfw / sn + depth), t, z + fl + 40.0,
                                 z + rf + rnd.uniform(220.0, 480.0), rnd))
                notes['slots'] += 1
                last = s
            prev = c
    # bed undercuts: a weak bed dissolved back under a stronger one, 8 to 16 m along the
    # wall, tapering out at both ends (round 3's short ellipsoid notches read as fingers)
    s_next = edge + rnd.uniform(*alcove_every) * 0.5
    side = 1 if rnd.random() < 0.5 else -1
    nb = len(rock.beds) - 2
    ss = [q[0] for q in info]
    for i, (s, (x, y, z, yaw), uv, name) in enumerate(info):
        if s < s_next or s > L - edge:
            continue
        run = rnd.uniform(800.0, 1600.0)
        s_next = s + rnd.uniform(*alcove_every) + 0.5 * run
        side = -side
        v = rnd.uniform(-150.0, 350.0)
        uw = _ring_wall(uv, side, v)
        if uw is None:
            continue
        _, (rx, ry) = yaw_dirs(yaw)
        zb = rock.zb(x + rx * uw, y + ry * uw, z + v)
        k0 = rock.bed(zb)
        k = max((max(0, min(nb, k0 + d)) for d in (-1, 0, 1)), key=lambda q: rock.beds[q + 1] - rock.beds[q])
        i1 = bisect.bisect_left(ss, min(s + run, L - edge))
        notes['alcoves'] += bed_undercut(sh, info, i, min(i1, len(info) - 1), side, k, rock, rnd, fade)
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
        sh.add(obox(c, sz, rnd.uniform(0.0, 180.0), rnd.uniform(-12.0, 12.0), rnd.uniform(-12.0, 12.0), rnd, 0.1), True)
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


def joint_frames(exit_t, entry=True):
    """Entry joint (unless the chunk starts the string), and the exit joint unless the
    chunk ends in rock (exit None)."""
    out = [((0.0, 0.0, 0.0), (1.0, 0.0))] if entry else []
    if exit_t is not None:
        ex, ey, ez, eyaw = exit_t
        (fx, fy), _ = yaw_dirs(eyaw)
        out.append(((ex, ey, ez), (fx, fy)))
    return out


def spec_joints(spec):
    """[(centre, forward, type, sign)] for the chunk's joints. sign is +1 at the entry
    (the chunk lies along +forward) and -1 at the exit."""
    js = []
    if spec.get('in'):
        js.append(((0.0, 0.0, 0.0), (1.0, 0.0), spec['in'], 1.0))
    if spec['exit'] is not None:
        ex, ey, ez, eyaw = spec['exit']
        (fx, fy), _ = yaw_dirs(eyaw)
        js.append(((ex, ey, ez), (fx, fy), spec['out'], -1.0))
    return js


def _plane_al(p, joints):
    """(along, sign * forward) for the joint plane nearest p."""
    best = None
    for (cx, cy, cz), (fx, fy), t, sg in joints:
        d = sg * ((p[0] - cx) * fx + (p[1] - cy) * fy)
        if best is None or abs(d) < abs(best[0]):
            best = (d, (fx * sg, fy * sg))
    return best


JOINT_SETTLE = 22.0   # clean_plane's reach (cm): just under one 25 cm voxel


def clean_plane(pts, tris, joints, eps=JOINT_SETTLE, tol=0.01):
    """Round 8b: tidy each cut before the snap. plane_cut leaves rows of vertices a few
    mm inside the plane next to the rim (a voxel layer 0.5 cm from it at B's exit); the
    snap put them on the plane too, and the thin triangles between folded over: dot -1
    pairs at every rim, the bright lips. Here (1) each vertex within eps of a plane, with
    a neighbour on it, merges into the nearest such neighbour; (2) triangles left lying in
    the plane with two open edges (ears off the rim) go; (3) plane vertices left inside
    the surface (no longer on the rim) step eps into the chunk. Returns (pts, tris, stats)."""
    nb = {}
    for a, b, c in tris:
        for u, v in ((a, b), (b, c), (c, a)):
            nb.setdefault(u, set()).add(v)
            nb.setdefault(v, set()).add(u)
    al = [_plane_al(p, joints)[0] if joints else 1e9 for p in pts]
    parent = list(range(len(pts)))
    nset = 0
    for u in range(len(pts)):
        if tol < al[u] < eps:
            on = [v for v in nb.get(u, ()) if abs(al[v]) <= tol]
            if on:
                parent[u] = min(on, key=lambda v: math.dist(pts[u], pts[v]))
                nset += 1

    def f(i):
        while parent[i] != i:
            i = parent[i]
        return i
    keep = {}
    for t in tris:
        q = (f(t[0]), f(t[1]), f(t[2]))
        if q[0] == q[1] or q[1] == q[2] or q[0] == q[2]:
            continue
        keep.setdefault(frozenset(q), []).append(q)
    tris = []
    for ts in keep.values():
        if len(ts) == 2:
            a, b = ts
            if (a[1], a[2]) not in ((b[0], b[1]), (b[1], b[2]), (b[2], b[0])):
                continue          # two opposite triangles on the same vertices: a fin, both go
        tris.append(ts[0])
    on = [abs(a) <= tol for a in al]
    alive = [True] * len(tris)
    he = {}
    for i, (a, b, c) in enumerate(tris):
        for e in ((a, b), (b, c), (c, a)):
            he[e] = i
    ears = 0
    changed = True
    while changed:
        changed = False
        for i, (a, b, c) in enumerate(tris):
            if not alive[i] or not (on[a] and on[b] and on[c]):
                continue
            if sum(1 for u, v in ((a, b), (b, c), (c, a)) if (v, u) not in he) >= 2:
                alive[i] = False
                for e in ((a, b), (b, c), (c, a)):
                    he.pop(e, None)
                ears += 1
                changed = True
    tris = [t for t, k in zip(tris, alive) if k]
    border = set()
    for (u, v) in he:
        if (v, u) not in he:
            border.update((u, v))
    pts = list(pts)
    lifted = 0
    for i, p in enumerate(pts):
        if on[i] and i not in border:
            d, (dx, dy) = _plane_al(p, joints)
            pts[i] = (p[0] + dx * eps, p[1] + dy * eps, p[2])
            lifted += 1
    used = sorted({x for t in tris for x in t})
    newi = {o: k for k, o in enumerate(used)}
    return [pts[o] for o in used], [tuple(newi[x] for x in t) for t in tris], \
        {'settled': nset, 'ears': ears, 'lifted': lifted}


_ANG_DTH = 2.0 * math.pi / NA


def _joint_target(t, k, a0, along):
    """Outline sample k of joint type t as the snap lays it at a0 (along from the plane):
    the rim on the plane, the outline plus the fading joint rock off it. (u, v) in the
    joint plane."""
    k %= NA
    if along <= 0.5:
        R = joint_rim(t)[k]
    else:
        # the rim's rounding fades out over the snap zone, so it meets the plane exactly
        R = joint_outline(t)[k] + (1.0 - sstep(0.0, FADE_OUTER, along)) * _jrel_k(t, k, a0)             + (1.0 - sstep(0.0, SNAP, along)) * _rim_delta(t, k)
    c, s = ANG[k]
    return R * c, R * s


def _closest_on_outline(t, u, v, r, R, a0, along):
    """Nearest point to (u, v) on type t's outline at a0. The ray's own target is |R - r|
    away, so only samples within that arc length either side need a look."""
    th = math.atan2(v, u)
    f = ((th + 0.5 * math.pi) / (2.0 * math.pi) * NA) % NA
    k0 = int(f)
    w = min(40, 1 + int(abs(R - r) / max(r * _ANG_DTH, 1.0)))
    best = None
    prev = _joint_target(t, k0 - w, a0, along)
    for k in range(k0 - w + 1, k0 + w + 2):
        cur = _joint_target(t, k, a0, along)
        ex, ey = cur[0] - prev[0], cur[1] - prev[1]
        L2 = ex * ex + ey * ey
        q = 0.0 if L2 < 1e-9 else max(0.0, min(1.0, ((u - prev[0]) * ex + (v - prev[1]) * ey) / L2))
        px, py = prev[0] + ex * q, prev[1] + ey * q
        d2 = (px - u) ** 2 + (py - v) ** 2
        if best is None or d2 < best[0]:
            best = (d2, px, py)
        prev = cur
    return best[1], best[2]


def snap_pts(pts, joints):
    """Blend vertices within SNAP of each joint plane onto the joint type's outline; those
    on the plane land exactly on the rim. Round 8b: onto the nearest point of the outline,
    not along the ray from the joint centre. Where a wall runs nearly along the rays (the
    slot walls), the radial snap sent neighbouring vertices to very different radii and
    folded the triangles between them. Returns (pts, moved)."""
    pts = list(pts)
    moved = 0
    for i, p in enumerate(pts):
        for (cx, cy, cz), (fx, fy), t, sg in joints:
            dx, dy, dz = p[0] - cx, p[1] - cy, p[2] - cz
            a0 = dx * fx + dy * fy
            along = sg * a0                     # distance from the plane, + into the chunk
            if along > SNAP or along < -1.0:
                continue
            lat = -dx * fy + dy * fx
            r = math.hypot(lat, dz)
            if r < 1.0:
                continue
            th = math.atan2(dz, lat)
            if along <= 0.5:
                R = joint_r(t, th)
            else:
                R = joint_base_r(t, th) + (1.0 - sstep(0.0, FADE_OUTER, along)) * joint_relief(t, th, a0)
            if r > R + 450.0:
                continue
            tu, tv = _closest_on_outline(t, lat, dz, r, R, a0, along)
            w = 1.0 - smooth(max(along, 0.0) / SNAP)
            nu, nv = lat + (tu - lat) * w, dz + (tv - dz) * w
            if along <= 0.5:
                # exactly on the polar rim, where conform_pts looks for it
                th2 = math.atan2(nv, nu)
                R2 = joint_r(t, th2)
                nu, nv = R2 * math.cos(th2), R2 * math.sin(th2)
                bx, by = cx, cy
            else:
                bx, by = cx + a0 * fx, cy + a0 * fy
            pts[i] = (bx - fy * nu, by + fx * nu, cz + nv)
            moved += 1
            break
    return pts, moved


def _tri_n(pts, t):
    """Unit normal of triangle t and its doubled area."""
    P, Q, R = pts[t[0]], pts[t[1]], pts[t[2]]
    u = (Q[0] - P[0], Q[1] - P[1], Q[2] - P[2])
    v = (R[0] - P[0], R[1] - P[1], R[2] - P[2])
    c = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    L = math.sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2])
    if L < 1e-9:
        return None, 0.0
    return (c[0] / L, c[1] / L, c[2] / L), L


LIP_DEG = 90.0        # a lip: two neighbouring triangles whose normals differ by more than this
LIP_SOFT = 60.0       # also counted, for context: a sharp crease, not yet a fold
LIP_REACH = 300.0     # how far from a joint plane, both sides, the lip check and repair look
LIP_LIFT = 15.0       # least distance (cm) from the plane for a rim fan vertex that folded


def joint_folds(pts, tris, joints, deg=LIP_DEG, reach=LIP_REACH):
    """Round 8b's lip check. For each joint: the interior edges with both triangles within
    reach (cm) of the plane on the chunk's side whose two normals differ by more than deg.
    A smooth rock wall from 25 cm voxels bends a few tens of degrees per edge; a fold back
    on itself (the bright lips, normals 150 to 180 degrees apart) or a knife crease is more.
    Returns per joint {'edges', 'lips' (over deg), 'over_60', 'max_deg', 'at': [(theta deg, a cm, deg)]}."""
    cmax = math.cos(math.radians(deg))
    out = []
    for (cx, cy, cz), (fx, fy), t, sg in joints:
        A = [sg * ((p[0] - cx) * fx + (p[1] - cy) * fy) for p in pts]
        he = {}
        for k, tr in enumerate(tris):
            if all(-1.0 <= A[i] <= reach for i in tr):
                a, b, c = tr
                for e in ((a, b), (b, c), (c, a)):
                    he[e] = k
        nrm = {}
        n = bad = soft = 0
        worst = 0.0
        at = []
        for (u, v), k in he.items():
            o = he.get((v, u))
            if o is None or o < k:
                continue
            for q in (k, o):
                if q not in nrm:
                    nrm[q] = _tri_n(pts, tris[q])[0]
            if nrm[k] is None or nrm[o] is None:
                continue
            n += 1
            d = max(-1.0, min(1.0, sum(x * y for x, y in zip(nrm[k], nrm[o]))))
            ang = math.degrees(math.acos(d))
            worst = max(worst, ang)
            if ang > LIP_SOFT:
                soft += 1
            if d < cmax:
                bad += 1
                x, y, z = [(pts[u][i] + pts[v][i]) * 0.5 for i in range(3)]
                dx, dy, dz = x - cx, y - cy, z - cz
                at.append((round(math.degrees(math.atan2(dz, -dx * fy + dy * fx))), round(sg * (dx * fx + dy * fy)),
                           round(ang)))
        out.append({'type': t, 'edges': n, 'lips': bad, 'over_%d' % int(LIP_SOFT): soft, 'max_deg': round(worst, 1),
                    'at': sorted(at)[:12]})
    return out


def unfold(pts, tris, joints, deg=LIP_SOFT, reach=LIP_REACH, passes=60):
    """Round 8b: repair what joint_folds finds. The snap moves vertices a few cm; where the
    voxel surface has triangles smaller than that, a neighbour can cross over and the pair
    folds. Each pass moves the vertices of every fold (and their neighbours, more gently)
    halfway to the centre of their neighbours, which lays the fold flat; vertices on a
    joint plane stay put, so the rims stay canonical. Repeats until none is left or passes
    run out. Returns (pts, tris, vertex moves)."""
    cmax = math.cos(math.radians(deg))
    pts = list(pts)
    plane = [any(abs((x - cx) * fx + (y - cy) * fy) < 0.01 for (cx, cy, cz), (fx, fy), t, sg in joints)
             for (x, y, z) in pts]
    band = [any(-1.0 <= sg * ((x - cx) * fx + (y - cy) * fy) <= reach for (cx, cy, cz), (fx, fy), t, sg in joints)
            for (x, y, z) in pts]
    he = {}
    nb = {}
    for k, (a, b, c) in enumerate(tris):
        for u, v in ((a, b), (b, c), (c, a)):
            he[(u, v)] = k
            nb.setdefault(u, set()).add(v)
            nb.setdefault(v, set()).add(u)
    edges = [(u, v, k, he[(v, u)]) for (u, v), k in he.items() if u < v and (v, u) in he and band[u] and band[v]]
    total = 0
    for _ in range(passes):
        hot = set()
        for u, v, k, o in edges:
            n1, n2 = _tri_n(pts, tris[k])[0], _tri_n(pts, tris[o])[0]
            if n1 is None or n2 is None or sum(x * y for x, y in zip(n1, n2)) < cmax:
                hot.update(tris[k])
                hot.update(tris[o])
        if not hot:
            break
        warm = set()
        for i in hot:
            warm.update(nb[i])
        warm -= hot
        new = {}
        for grp, wt in ((hot, 0.5), (warm, 0.25)):
            for i in grp:
                if plane[i]:
                    continue
                ns = nb[i]
                c = [sum(pts[j][q] for j in ns) / len(ns) for q in range(3)]
                q = tuple(pts[i][k] + (c[k] - pts[i][k]) * wt for k in range(3))
                if i in hot and any(plane[j] for j in ns):
                    # a fan vertex on the rim: the centre of its neighbours lies near the
                    # plane, which keeps the fan flat; where the rim has a notch (the key's
                    # ledges, the rift's corners) a flat fan folds, so it steps back into
                    # the chunk, where the fan stands up and the notch only bends it
                    d, (dx, dy) = _plane_al(q, joints)
                    if d < LIP_LIFT:
                        q = (q[0] + dx * (LIP_LIFT - d), q[1] + dy * (LIP_LIFT - d), q[2])
                new[i] = q
        for i, q in new.items():
            pts[i] = q
        total += len(new)
    return pts, list(tris), total


def joint_pts(pts, tris, joints, n=None, tol=0.5):
    """The joint steps on plain lists: clean_plane, snap_pts, conform_pts. Returns
    (pts, tris, moved, per-joint stats)."""
    pts, tris, cl = clean_plane(pts, tris, joints)
    pts, moved = snap_pts(pts, joints)
    pts, tris, stats = conform_pts(pts, tris, joints, n or JOINT_N, tol)
    pts, tris, nun = unfold(pts, tris, joints)
    tris, nfl = flip_flat(pts, tris)
    lips = joint_folds(pts, tris, joints)
    for st, lp in zip(stats, lips):
        st.update(cl)
        st['unfolded'] = nun
        st['flipped'] += nfl
        st['lips'] = lp['lips']
        st['creases_over_60'] = lp['over_60']
        st['lip_max_deg'] = lp['max_deg']
    return pts, tris, moved, stats


def joint_mesh(mesh, joints):
    """joint_pts on a DynamicMesh; returns (new mesh, per-joint stats, moved)."""
    GS_R.compact_mesh(mesh)
    pts = [(q.x, q.y, q.z) for q in GS_L.convert_vector_list_to_array(GS_Q.get_all_vertex_positions(mesh, False)[1])]
    tris = [(t.x, t.y, t.z) for t in GS_L.convert_triangle_list_to_array(GS_Q.get_all_triangle_indices(mesh, False)[1])]
    pts, out, moved, stats = joint_pts(pts, tris, joints)
    buf = U.GeometryScriptSimpleMeshBuffers()
    buf.vertices = [U.Vector(*q) for q in pts]
    buf.triangles = [U.IntVector(*t) for t in out]
    res = U.DynamicMesh()
    GS_E.append_buffers_to_mesh(res, buf, 0)
    if GS_Q.get_num_triangle_i_ds(res) != len(out):
        raise RuntimeError('rim rebuild dropped triangles: %d of %d' % (GS_Q.get_num_triangle_i_ds(res), len(out)))
    return res, stats, moved


JOINT_N = 720         # rim vertices at every joint, evenly spaced in angle: both chunks get the same polygon


def conform_pts(pts, tris, joints, n=JOINT_N, tol=0.5):
    """Give every joint rim the same canonical polygon: n vertices on the type's outline at
    even angles, counted from the joint's side axis.

    Each chunk is cut at its joints on its own, so its rim was its own polygon. Where one
    rim's edge cut inside the other's, a sliver up to ~0.3 cm wide opened, and the
    background showed through as a dashed hairline round the joint. Here each rim vertex
    moves along the outline to the nearest canonical angle (an order-keeping map, so no
    triangle flips), and each rim edge that spans several canonical angles gets the
    missing vertices, its triangle becoming a fan. Both sides of a joint then share their
    rim edge for edge. Returns (pts, tris, per-joint numbers); raises if a rim does
    not go once round, one way."""
    pts = list(pts)
    step = 2.0 * math.pi / n

    def canon(j, k):
        (cx, cy, cz), (fx, fy), t, sg = joints[j]
        ang = k * step
        R = joint_r(t, ang)
        c, s = R * math.cos(ang), R * math.sin(ang)
        return (cx - fy * c, cy + fx * c, cz + s)

    ring = {}
    for i, (x, y, z) in enumerate(pts):
        for j, ((cx, cy, cz), (fx, fy), t, sg) in enumerate(joints):
            dx, dy, dz = x - cx, y - cy, z - cz
            if abs(dx * fx + dy * fy) > tol:
                continue
            lat = -dx * fy + dy * fx
            th = math.atan2(dz, lat)
            if abs(math.hypot(lat, dz) - joint_r(t, th)) > 1.0:
                continue
            ring[i] = (j, int(round(th / step)) % n)
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
    for j in range(len(joints)):
        ds = [d for (jj, u, v, d) in rim if jj == j]
        turn = sum(ds)
        back = sum(1 for d in ds if d * turn < 0)
        st = {'type': joints[j][2], 'ring_verts': sum(1 for (jj, k) in ring.values() if jj == j), 'rim_edges': len(ds),
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
    # weld (round 6): rim vertices that landed on the same canonical vertex (the ends of a
    # collapsed rim edge) become one, and the triangles that collapse with them go; round 5
    # left these as zero-area triangles round every rim
    rep = {}
    first = {}
    for i, jk in ring.items():
        if jk in first:
            rep[i] = first[jk]
        else:
            first[jk] = i
    if rep:
        welded = []
        for t in out:
            q = tuple(rep.get(x, x) for x in t)
            if q[0] != q[1] and q[1] != q[2] and q[0] != q[2]:
                welded.append(q)
        used = sorted({x for t in welded for x in t})
        newi = {o: k for k, o in enumerate(used)}
        pts = [pts[o] for o in used]
        out = [tuple(newi[x] for x in t) for t in welded]
        for j in range(len(joints)):
            stats[j]['welded'] = sum(1 for i in rep if ring[i][0] == j)
    pts, out, nsl = collapse_slivers(pts, out, joints)
    out, nfl = flip_flat(pts, out)
    for st in stats:
        st['slivers'] = nsl
        st['flipped'] = nfl
    return pts, out, stats


def _tri_h(pts, a, b, c):
    """Height of triangle abc over its longest edge (cm), and that edge's index (0 ab, 1 bc, 2 ca)."""
    P, Q, R = pts[a], pts[b], pts[c]
    u = (Q[0] - P[0], Q[1] - P[1], Q[2] - P[2])
    v = (R[0] - P[0], R[1] - P[1], R[2] - P[2])
    cr = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    ar2 = math.sqrt(cr[0] ** 2 + cr[1] ** 2 + cr[2] ** 2)
    ls = (math.dist(P, Q), math.dist(Q, R), math.dist(R, P))
    k = max(range(3), key=lambda i: ls[i])
    return ar2 / max(ls[k], 1e-9), k


def flip_flat(pts, tris, min_h=0.01):
    """Flip away flat triangles (round 8). Some voxel triangles have their third vertex on
    the long edge (zero area, so 'degenerate') without any short edge for collapse_slivers
    to take. The long edge is flipped with the triangle across it: the quad b,c,a,d splits
    on c-d instead, which keeps the surface and its winding, and the flat triangle goes.
    Skipped where there is no neighbour (a rim), the edge c-d exists already, or a new
    triangle would be flat too. Returns (tris, flips)."""
    tris = [tuple(t) for t in tris]
    edge = {}
    for i, t in enumerate(tris):
        for e in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            edge[e] = i
    n = 0
    for i in range(len(tris)):
        t = tris[i]
        h, k = _tri_h(pts, *t)
        if h >= min_h:
            continue
        a, b, c = t[k], t[(k + 1) % 3], t[(k + 2) % 3]
        j = edge.get((b, a))
        if j is None or j == i:
            continue
        o = tris[j]
        d = [x for x in o if x != a and x != b]
        if len(d) != 1:
            continue
        d = d[0]
        if (c, d) in edge or (d, c) in edge:
            continue
        n1, n2 = (b, c, d), (c, a, d)
        if _tri_h(pts, *n1)[0] < min_h or _tri_h(pts, *n2)[0] < min_h:
            continue
        for tt in (t, o):
            for e in ((tt[0], tt[1]), (tt[1], tt[2]), (tt[2], tt[0])):
                edge.pop(e, None)
        tris[i], tris[j] = n1, n2
        for q, tt in ((i, n1), (j, n2)):
            for e in ((tt[0], tt[1]), (tt[1], tt[2]), (tt[2], tt[0])):
                edge[e] = q
        n += 1
    return tris, n


def collapse_slivers(pts, tris, joints, min_edge=0.2, rim_tol=0.6):
    """Collapse edges shorter than min_edge (cm): the voxel surfacing leaves a few dozen
    slivers per chunk with sub-millimetre edges (zero area, so 'degenerate'). Round 6 left
    them; a repair that deletes them opens tiny holes. Here both ends of a short edge
    become one vertex (a rim vertex wins, and two rim vertices never merge, so the rims
    stay canonical), the triangles that collapse go, and a pair of triangles left on the
    same three vertices (a fin) goes too. Returns (pts, tris, collapsed edges)."""
    def on_rim(i):
        x, y, z = pts[i]
        return any(abs((x - cx) * fx + (y - cy) * fy) < rim_tol for (cx, cy, cz), (fx, fy), t, sg in joints)

    parent = list(range(len(pts)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    m2 = min_edge * min_edge
    n = 0
    for t in tris:
        for u, v in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            a, b = pts[u], pts[v]
            d2 = (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2
            if d2 >= m2:
                continue
            ru, rv = find(u), find(v)
            if ru == rv:
                continue
            fu, fv = on_rim(ru), on_rim(rv)
            if fu and fv and d2 > 1e-4:
                # two rim vertices merge only when they coincide (round 8: the key's
                # steep relief left a pair 0.000 cm apart with two zero-area triangles)
                continue
            if fv:
                ru, rv = rv, ru
            parent[rv] = ru
            n += 1
    if not n:
        return pts, tris, 0
    keep = {}
    for t in tris:
        q = (find(t[0]), find(t[1]), find(t[2]))
        if q[0] == q[1] or q[1] == q[2] or q[0] == q[2]:
            continue
        keep.setdefault(frozenset(q), []).append(q)
    out = []
    for k, ts in keep.items():
        if len(ts) == 1:
            out.append(ts[0])
        elif len(ts) == 2:
            a, b = ts
            # same winding: a duplicate, keep one; opposite: a fin, drop both
            if (a[1], a[2]) in ((b[0], b[1]), (b[1], b[2]), (b[2], b[0])):
                out.append(a)
        else:
            out.append(ts[0])
    used = sorted({x for t in out for x in t})
    newi = {o: k for k, o in enumerate(used)}
    return [pts[o] for o in used], [tuple(newi[x] for x in t) for t in out], n


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
    if spec.get('in'):
        plane_cut(m, (0, 0, 0), (-1, 0, 0))
    if spec['exit'] is not None:
        ex, ey, ez, eyaw = spec['exit']
        (fx, fy), _ = yaw_dirs(eyaw)
        plane_cut(m, (ex, ey, ez), (fx, fy, 0))
    note('cut')
    joints = spec_joints(spec)
    m, spec['rim'], moved = joint_mesh(m, joints)
    finish(m)
    return m, time.time() - t0, moved


def core_exposure(mesh, spec, tol=4.0):
    """Final-mesh vertices on (or inside) the clear core, away from the joint necks.
    A rock surface that lies on the core would be a flat patch; 0 is the target."""
    core = spec['core']
    res = GS_Q.get_all_vertex_positions(mesh, False)
    pts = GS_L.convert_vector_list_to_array(res[1])
    frames = joint_frames(spec['exit'], bool(spec.get('in')))
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
FADE_OUTER = 650.0    # the rock relief grows from nothing at a joint plane to full over this (cm)
JFINE_REACH = 350.0   # a fine relief ramps in over this from the plane, where the big relief is still small
JFINE_AMP = 26.0


def joint_along(p, exit_t, entry=True):
    """Distance (cm) from p to the nearest joint plane it faces (within 13 m of the joint's
    centre), or None."""
    best = None
    for (cx, cy, cz), (fx, fy) in joint_frames(exit_t, entry):
        dx, dy, dz = p[0] - cx, p[1] - cy, p[2] - cz
        along = abs(dx * fx + dy * fy)
        lat = math.sqrt(max(0.0, dx * dx + dy * dy + dz * dz - along * along))
        if lat < 1300.0 and (best is None or along < best):
            best = along
    return best


def joint_fade(p, exit_t, inner=0.0, outer=FADE_OUTER, entry=True):
    """0 at either joint plane, 1 from FADE_OUTER in. Round 5 faded the relief in over
    0.4 to 2 m, and the roof snapped up to its bedding plane (up to 2 m) within that, so
    every joint stood in a steep collar: a dark crease ran round its rim and it read as a
    door frame. Round 6 ramps over 6.5 m, so the passage eases out of the outline."""
    a = joint_along(p, exit_t, entry)
    return 1.0 if a is None else sstep(inner, outer, a)


class JointFade:
    """The joint fade as a callable, plus near(p): the weight of a fine relief that keeps
    the first metres past a joint from being plain while the big relief is still small
    (0 on the plane, so the rims still match)."""

    def __init__(self, exit_t, entry=True, types=(None, None)):
        self.exit_t, self.entry = exit_t, entry
        # (entry type, exit type), for the joint's own relief
        self.frames = []
        for (c, f), t in zip(joint_frames(exit_t, entry), [x for x, on in zip(types, (entry, exit_t is not None)) if on]):
            self.frames.append((c, f, t))

    def __call__(self, p):
        return joint_fade(p, self.exit_t, entry=self.entry)

    def near(self, p):
        a = joint_along(p, self.exit_t, self.entry)
        if a is None:
            return 0.0
        # round 7: a slow ramp in and out (round 6 rose over 1.4 m and peaked there)
        return sstep(0.0, JFINE_REACH, a) * (1.0 - sstep(0.4 * JFINE_REACH, FADE_OUTER, a))

    def relief(self, p):
        """The nearest joint's own relief at p, weighted by 1 - fade, as a world offset
        (dx, dy, dz), or None. Same frame and lateral reach as joint_along."""
        best = None
        for (cx, cy, cz), (fx, fy), t in self.frames:
            dx, dy, dz = p[0] - cx, p[1] - cy, p[2] - cz
            a0 = dx * fx + dy * fy
            lat = -dx * fy + dy * fx
            if t and math.hypot(lat, dz) < 1300.0 and (best is None or abs(a0) < abs(best[1])):
                best = (t, a0, lat, dz, fx, fy)
        if best is None:
            return None
        t, a0, lat, dz, fx, fy = best
        w = 1.0 - sstep(0.0, FADE_OUTER, abs(a0))
        if w <= 0.0:
            return None
        th = math.atan2(dz, lat)
        d = w * joint_relief(t, th, a0)
        c, s_ = math.cos(th), math.sin(th)
        return (-fy * c * d, fx * c * d, s_ * d)


def section_defs(seed):
    """The section shapes for one chunk; meanders and widths vary along s."""
    rnd = random.Random(seed)
    ph = [rnd.uniform(0.0, 2 * math.pi) for _ in range(8)]
    return {
        'neck': (lambda s: sec_neck(), 'silt', 1.0, False),
        'j_bed': (lambda s: joint_outline('bed'), 'silt', 1.0, False),
        'j_rift': (lambda s: joint_outline('rift'), 'rock', 1.0, False),
        'j_key': (lambda s: joint_outline('key'), 'rock', 1.0, False),
        'round': (lambda s: sec_round(), 'silt', 0.9, False),
        'phreatic': (lambda s: sec_phreatic(0.12 * math.sin(s / 600.0 + ph[7]), 0.85 + 0.25 * math.sin(s / 450.0 + ph[2])),
                     'silt', 1.0, True),
        'throat': (lambda s: sec_throat(), 'rock', 1.0, False),
        'rift': (lambda s: sec_rift(0.22 + 0.3 * math.sin(s / 330.0 + ph[0]), 175.0 + 60.0 * math.sin(s / 260.0 + ph[1]),
                                   -0.15 + 0.25 * math.sin(s / 410.0 + ph[5])), 'rock', 0.9, True),
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
    """Straight 30 m tunnel: a rift joint, canyon, keyhole, a keyhole joint."""
    L = 3000.0
    ex = (L, 0.0, 0.0, 0.0)
    spec = {'name': 'A', 'type': 'tunnel', 'exit': ex, 'in': 'rift', 'out': 'key'}
    if not shapes:
        return spec
    p, s_list, slices, core = straight_core(L)
    secs = Sections([(-EXT, 'j_rift'), (100.0, 'j_rift'), (750.0, 'canyon'), (1350.0, 'canyon'), (1750.0, 'keyhole'),
                     (L, 'j_key'), (L + EXT, 'j_key')], section_defs(11), seed=11)
    rock = Rock(11)
    fade = JointFade(ex, types=(spec['in'], spec['out']))
    sh = Shapes(11)
    tv, info = tunnel(p, secs, rock, s_list, slices=slices, fade=fade)
    sh.add(tv)
    notes = tunnel_features(sh, info, rock, random.Random(12), L, core, fade, dome_secs=('keyhole',), dome_every=900.0)
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'path': p, 'notes': notes})
    return spec


def spec_B(shapes=True):
    """Straight 30 m wide, low chamber that flares from a rift joint and narrows to a
    bedding-plane joint: breakdown on a silt floor, domes."""
    L = 3000.0
    ex = (L, 0.0, 0.0, 0.0)
    spec = {'name': 'B', 'type': 'cavern', 'exit': ex, 'in': 'rift', 'out': 'bed'}
    if not shapes:
        return spec
    p, s_list, slices, core = straight_core(L)
    # each joint sits a few metres back down a rough throat that flares raggedly into the hall
    secs = Sections([(-EXT, 'j_rift'), (0.0, 'j_rift'), (1150.0, 'hall'), (1800.0, 'hall'), (L, 'j_bed'),
                     (L + EXT, 'j_bed')], section_defs(22), seed=22)
    rock = Rock(22)
    fade = JointFade(ex, types=(spec['in'], spec['out']))
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
    # recesses in the hall's end walls beside each mouth, under its lip, so the wall round
    # the throat is broken and overhung rather than a flat face with a hole in it
    for c, r, yaw in (((820.0, -820.0, 120.0), (420.0, 520.0, 330.0), 25.0), ((760.0, 760.0, -120.0), (380.0, 480.0, 300.0), -20.0),
                      ((2200.0, 820.0, 160.0), (440.0, 520.0, 320.0), 15.0), ((2260.0, -760.0, -100.0), (380.0, 460.0, 300.0), -30.0)):
        sh.add(rock_ellipsoid(c, r, rock, 'rock', res=40.0, rot_yaw=yaw, fade=fade, core=core, outward=True))
    notes = tunnel_features(sh, info, rock, random.Random(23), L, core, fade, slot_p=0.3, alcove_every=(900.0, 1300.0))
    # silt floor: rock below about z = -500, rising to -470 over the last 4+ m before each neck
    def top(x, y):
        rise = 1.0 - min(sstep(350, 1000, x), 1.0 - sstep(2000, 2650, x))
        slot = 1.0 - sstep(150.0, 950.0, x)
        return -505.0 + 35.0 * rise - 420.0 * slot + 22.0 * fbm(x, y, 0, 23, 300.0)
    sh.add(heightfield(150, 2850, -1700, 1700, -1400, top), True)
    # breakdown: angular blocks, a few big ones at the sides, all clear of the core
    nb = 0
    for i in range(160):
        if nb >= 34:
            break
        big = i % 4 == 0
        x = rnd.uniform(950, 2050)
        y = rnd.uniform(-1150, 1150)
        sz = ((rnd.uniform(260, 420), rnd.uniform(220, 380), rnd.uniform(110, 200)) if big else
              (rnd.uniform(100, 260), rnd.uniform(90, 230), rnd.uniform(50, 120)))
        # flat slabs half sunk in the silt: round 3's tilted, jittered boxes stood up as points
        c = (x, y, top(x, y) + sz[2] * rnd.uniform(-0.2, 0.12))
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, rnd.uniform(0, 180), rnd.uniform(-8, 8), rnd.uniform(-8, 8), rnd, 0.07), True)
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


# Round 8b: from Helm_A_T1 the squeeze read as a doorway, a dark rectangle with a notch
# on top. Its walls stood on the core's box plus a gap that only swelled here and there,
# so looking down its 14 m every ray found a pinch at the same 30 cm: the end-on outline
# was the core's rounded rectangle, and the roof slot (the same at every s) the notch.
# Now the walls lean (the right one out at the top, the left out at the bottom), stand
# off unevenly by angle, and pinch in at a different s for each angle; the slots' lean
# and width change along s. All of it is outward, so the 0.75 m margin still holds; less
# T1_FLOOR, so where lean, lump and pinch are all small the wall still comes in to it and
# the squeeze is no wider than before.
# Round 8c: past the squeeze T1 was still a square duct. The core's box (5.6 x 7.3 m, wider
# round the bend) is bigger than the rift, canyon and low sections, so 40 to 85% of each
# ring stood on the box plus 30 cm: a straight roof and vertical walls. The 8b gap only
# lived at the squeeze, and there T1_FLOOR (80) ate the whole lump, so the narrowest wall
# at every angle was the box again: seen end-on, a rectangle. Now the gap runs the whole
# length, fading out 1.5 to 8 m from each joint (the rims stay as they are): the walls lean
# one way and then the other along s, the per-angle stand-off is a blend of three lump
# patterns that changes along s, and the pinches come at a different s for each angle.
# Less a floor, so part of each ring still comes in to the box and the squeeze stays tight,
# but which part changes with angle: the narrowest outline is no longer the box. All of it
# is outward, so the 0.75 and 1.0 m margins hold.
T1_LEAN = (120.0, 150.0)    # cm the side walls lean out over the passage height: squeeze, elsewhere
T1_LUMP = (140.0, 150.0)    # per-angle stand-off, 0 to this
T1_PINCH = (110.0, 100.0)   # how far the walls stand back between pinches
T1_PINCH_L = (520.0, 900.0) # pinch spacing along s
T1_FLOOR = (60.0, 35.0)     # taken off the sum, so some angles still reach the core


def t1_gap():
    rnd = random.Random(331)
    lump = [jag(rnd, lobes=(3, 5, 7, 11), amp=1.0), jag(rnd, lobes=(2, 4, 6, 9), amp=1.0),
            jag(rnd, lobes=(3, 4, 7, 10), amp=1.0)]
    ph = jag(rnd, lobes=(2, 3), amp=math.pi)
    ph2 = jag(rnd, lobes=(2, 3, 5), amp=math.pi)
    a0, a1, a2, a3 = (rnd.uniform(0.0, 2 * math.pi) for _ in range(4))
    q0, q1 = T1_SQUEEZE
    L = t1_path().length

    def g(s, k, c, sn, rc, sl):
        w = sstep(150.0, 800.0, s) * (1.0 - sstep(L - 800.0, L - 150.0, s))
        if w <= 0.0:
            return 0.0
        q = sstep(q0 - 600.0, q0, s) * (1.0 - sstep(q1, q1 + 600.0, s))   # 1 in the squeeze
        t = max(0.0, min(1.0, (rc * sn - sl[2]) / max(sl[3] - sl[2], 1.0)))   # 0 floor, 1 roof
        # lean: in the squeeze the right wall out at the top, the left at the bottom (8b);
        # elsewhere the lean swings from one way to the other every 20 m or so
        la = q + (1.0 - q) * max(-1.0, min(1.0, 1.4 * math.sin(s / 650.0 + a0)))
        lean = (q * T1_LEAN[0] + (1.0 - q) * T1_LEAN[1]) * abs(la) * c * c * (t if (c > 0.0) == (la > 0.0) else 1.0 - t)
        wa = (0.6 + 0.5 * math.sin(s / 610.0 + a1), 0.6 + 0.5 * math.sin(s / 830.0 + a2),
              0.6 + 0.5 * math.sin(s / 470.0 + a3))
        v = sum(wi * li[k] for wi, li in zip(wa, lump)) / sum(wa)
        lmp = (q * T1_LUMP[0] + (1.0 - q) * T1_LUMP[1]) * 0.5 * (1.0 + max(-1.0, min(1.0, 1.6 * v)))
        p_sq = T1_PINCH[0] * 0.5 * (1.0 - math.cos(2.0 * math.pi * (s - q0) / T1_PINCH_L[0] + ph[k]))
        p_el = T1_PINCH[1] * 0.5 * (1.0 - math.cos(2.0 * math.pi * s / T1_PINCH_L[1] + ph2[k]))
        fl = q * T1_FLOOR[0] + (1.0 - q) * T1_FLOOR[1]
        return w * max(0.0, lean + lmp + q * p_sq + (1.0 - q) * p_el - fl)
    return g


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
    spec = {'name': 'T1', 'type': 'tunnel', 'exit': ex, 'path': p, 'squeeze': T1_SQUEEZE, 'in': 'key', 'out': 'bed'}
    if not shapes:
        return spec
    q0, q1 = T1_SQUEEZE
    s_list = ring_s(-EXT, L + EXT)
    slices = [core_slice(p, s, t1_margin) for s in s_list]
    core = Core()
    core.add_tube(p, s_list, slices)
    secs = Sections([(-EXT, 'j_key'), (0.0, 'j_key'), (q0, 'rift'), (q1, 'rift'),
                     (2900.0, 'keyhole'), (3650.0, 'keyhole'), (4050.0, 'canyon'), (6050.0, 'canyon'), (6350.0, 'widelow'),
                     (8200.0, 'widelow'), (8650.0, 'phreatic'), (L, 'j_bed'), (L + EXT, 'j_bed')],
                    section_defs(33), seed=33)
    rock = Rock(33)
    fade = JointFade(ex, types=(spec['in'], spec['out']))
    sh = Shapes(33)
    tv, info = tunnel(p, secs, rock, s_list, slices=slices, fade=fade, gap=t1_gap())
    sh.add(tv)
    notes = tunnel_features(sh, info, rock, random.Random(34), L, core, fade,
                            dome_secs=('keyhole', 'widelow', 'phreatic'), block_secs=('widelow',), blocks=12)
    # the outside of the bend, seen head-on down the keyhole: undercut bays at three heights,
    # so the far wall is broken, not a flat face across the flight path
    ss = [q[0] for q in info]
    for sb, dv, (ax, ay, az) in ((4450.0, 120.0, (650.0, 430.0, 480.0)), (4850.0, -120.0, (760.0, 480.0, 430.0)),
                                 (5250.0, 330.0, (620.0, 420.0, 420.0))):
        s, (x, y, z, yaw), uv, name = info[min(len(info) - 1, bisect.bisect_left(ss, sb))]
        uw = _ring_wall(uv, -1, dv, w=60.0)
        if uw is None:
            continue
        _, (rx, ry) = yaw_dirs(yaw)
        u = uw - 0.45 * ay
        sh.add(rock_ellipsoid((x + rx * u, y + ry * u, z + dv), (ax, ay, az), rock, 'rock', res=40.0, rot_yaw=yaw,
                              fade=fade, core=core, outward=True))
        notes['bays'] = notes.get('bays', 0) + 1
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
    spec = {'name': 'C1', 'type': 'stop', 'exit': ex, 'in': 'bed', 'out': 'rift',
            'sockets': {'Park': (cx, cy, C1_PARK_Z, 0.0), 'Serpent': (cx + 400.0, -1750.0, -1500.0, 90.0)},
            'salvage': salvage, 'col': C1_COL}
    if not shapes:
        return spec
    rnd = random.Random(44)
    # (round 6: sharper, finer relief; round 5's entry rock read as clay; beds unwarped so the
    # ledge salvage sockets stay level)
    rock = Rock(44, a_res=55.0, a_blk=34.0, a_fine=14.0, a_sharp=24.0, a_warp=0.0)
    core = c1_core()
    fade = JointFade(ex, types=(spec['in'], spec['out']))
    sh = Shapes(44)
    defs = section_defs(44)
    # entry tunnel, level, opening into the chamber
    pin = Path().straight(cx)
    # round 7: the entry run in the same beds with softer blocks (round 6's sharp, blocky
    # rock read as facets from the joint)
    rock_in = Rock(44, a_res=50.0, a_blk=18.0, a_fine=12.0, a_sharp=10.0, a_warp=0.0)
    secs_in = Sections([(-EXT, 'j_bed'), (0.0, 'j_bed'), (1100.0, 'keyhole'), (cx, 'keyhole')], defs, seed=44)
    tv, info_in = tunnel(pin, secs_in, rock_in, ring_s(-EXT, cx), core=core, fade=fade)
    sh.add(tv)
    # exit tunnel from the column axis, level at Park height
    pout = Path(cx, cy, C1_EXIT_YAW).straight(C1_EXIT_RUN)
    pout.zf = lambda s: C1_PARK_Z
    secs_out = Sections([(500.0, 'phreatic'), (1500.0, 'phreatic'), (1850.0, 'widelow'), (2250.0, 'widelow'),
                         (C1_EXIT_RUN, 'j_rift'), (C1_EXIT_RUN + EXT, 'j_rift')], defs, seed=45)
    # the mouth's lips stand inside the 9 m circle, so near the exit the wall keeps the same
    # uneven gap round the hull's swept box as the straight chunks do
    s_out = ring_s(500.0, C1_EXIT_RUN + EXT)
    sl_out = core_slice(pout, C1_EXIT_RUN, lambda s: M_BODY)
    tv, info_out = tunnel(pout, secs_out, rock, s_out, slices=[sl_out if s >= 1800.0 else None for s in s_out],
                          core=core, fade=fade)
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
        # the ledges carry salvage sockets: place them on the unwarped beds, as in round 5
        k = rock.bed(ztop - ((cx + ax * mid) * rock.dip[0] + (cy + ay * mid) * rock.dip[1]))
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


E_LEN = 2600.0          # the end chunk: B's far joint to where the passage closes
E_CORE = 800.0          # the core reaches the hull's nose (6.15 m + margin) with B's exit as its centre


def spec_E(shapes=True):
    """End chunk: the string ends in rock. A rough throat flares into a wide, low bedding-plane
    passage that closes down over 15 m, with a breakdown choke. One joint (its entry); no
    exit. The core covers only the hull's nose when the hull stands at B's exit."""
    L = E_LEN
    spec = {'name': 'E', 'type': 'end', 'exit': None, 'in': 'bed', 'out': None}
    if not shapes:
        return spec
    # the passage turns 50 degrees before it closes, so from the joint the eye meets the
    # outside of the bend, not the end of a straight slot
    p = Path().straight(900.0).arc(1500.0, -50.0)
    p.straight(L - p.length)
    s_list = ring_s(-EXT, L)
    sl = core_slice(p, 0.0, lambda s: M_BODY)
    slices = [sl if s <= E_CORE else None for s in s_list]
    core = Core()
    core.add_tube(p, s_list, slices)
    q0, q1 = 1000.0, L - 120.0
    defs = section_defs(55)
    # no snap of the roof to a bedding plane: it would hold the roof flat and open (a slot)
    # (round 7b: the section varies along s. Rounds 4 to 7 marked it constant, so it was
    # cached open at q0, the passage never closed and it ended in the tube's flat end cap:
    # the smooth oval plug seen from the joint)
    defs['pinch'] = (lambda s: sec_pinch(max(0.0, min(1.0, (s - q0) / (q1 - q0)))), 'silt', 1.0, True)
    secs = Sections([(-EXT, 'j_bed'), (0.0, 'j_bed'), (q0, 'pinch'), (L, 'pinch')], defs, seed=55)
    # (round 6: E's roof barely snaps, so no flat lintel shows through the bed joint from B;
    # round 8b: not at all, since its snap patches were the flat approach roof; the roof's
    # breakdown scars below give it its steps instead)
    rock = Rock(55, roof_snap=0.0)
    # the same beds, a little more broken, for the bays (round 7: round 6's harder rock_b,
    # its rock-floor snap and outward-only relief crumpled the bays; now they carry the
    # passage's own relief and only open the wall out)
    rock_b = Rock(55, a_res=50.0, a_var=30.0, a_blk=22.0, a_notch=16.0, a_fine=10.0, a_sharp=10.0)
    fade = JointFade(None, types=(spec['in'], None))
    sh = Shapes(55)
    tv, info = tunnel(p, secs, rock, s_list, slices=slices, fade=fade)
    sh.add(tv)
    notes = tunnel_features(sh, info, rock, random.Random(56), L, core, fade, slot_p=0.35,
                            alcove_every=(500.0, 800.0), block_secs=(), blocks=0, edge=500.0)
    # bays in both walls past the throat, more on the outside of the bend, which faces
    # the joint: broken rock, not flat walls
    ss = [q[0] for q in info]
    for sb, side, dv, (ax, ay, az) in ((1150.0, 1, 60.0, (560.0, 380.0, 360.0)), (1500.0, 1, -90.0, (620.0, 400.0, 300.0)),
                                       (1850.0, 1, 140.0, (520.0, 360.0, 280.0)), (1050.0, -1, -40.0, (650.0, 360.0, 330.0)),
                                       (1650.0, -1, 80.0, (520.0, 340.0, 260.0))):
        s, (x, y, z, yaw), uv, name = info[min(len(info) - 1, bisect.bisect_left(ss, sb))]
        uw = _ring_wall(uv, side, dv, w=60.0)
        if uw is None:
            continue
        _, (rx, ry) = yaw_dirs(yaw)
        # round 7: wider along the wall and shallower, centred nearer the wall, so each bay
        # meets the wall at a low angle instead of a fold
        ax, ay = 1.2 * ax, 0.7 * ay
        u = uw + side * 0.15 * ay
        c = (x + rx * u, y + ry * u, z + dv)
        sh.add(warp_blob(rock_ellipsoid(c, (ax, ay, az), rock_b, 'silt', res=40.0, rot_yaw=yaw, fade=fade, core=core),
                         c, 0.14, 520.0, 560 + int(sb)))
        notes['bays'] = notes.get('bays', 0) + 1
    # round 8b: breakdown scars in the approach roof. Round 7b's roof was smooth and flat
    # where it snapped to a bedding plane. Here slabs have fallen out of it: tilted,
    # overlapping recesses up to a metre deep, whose edges step the roof in courses (water
    # only, so the core keeps its clearance; none within 7 m of the joint)
    rs = random.Random(558)
    nsc = 0
    for i in range(80):
        if nsc >= 16:
            break
        sc_s = rs.uniform(700.0, 2050.0)
        _, (x, y, z, yaw), uv, _ = info[min(len(info) - 1, bisect.bisect_left(ss, sc_s))]
        _, (rx, ry) = yaw_dirs(yaw)
        hw = max(abs(q[0]) for q in uv)
        u = rs.uniform(-0.75, 0.75) * hw
        v = _ring_roof(uv, u, 60.0)
        if v is None or v < 120.0:
            continue
        sz = (rs.uniform(260.0, 520.0), rs.uniform(200.0, 420.0), rs.uniform(70.0, 140.0))
        c = (x + rx * u, y + ry * u, z + v + 0.2 * sz[2])
        sh.add(obox(c, sz, yaw + rs.uniform(-50.0, 50.0), rs.uniform(-18.0, 18.0), rs.uniform(-18.0, 18.0), rs, 0.15))
        nsc += 1
    notes['roof_scars'] = nsc
    # round 7b: the breakdown choke. Where the passage closes, angular blocks pile up from
    # the floor toward the roof, half buried and overlapping (lone blocks on the silt read
    # as boulders), with a few from the broken roof and walls, so it ends in broken rock
    # and no smooth patch shows. Round 8b: 7b's blocks, no more than 2 m across and nearly
    # cubic, came out of the 25 cm voxels as pale rounded lumps; now they are slabs (up to
    # 3.4 m by 2.6 m, under a metre thick) tilted up to 40 degrees, as roof beds fall
    rc = random.Random(557)
    s_ch = L - 1150.0
    nb = 0
    for i in range(1200):
        if nb >= 70:
            break
        t = math.sqrt(rc.random())
        s = s_ch + (L - 40.0 - s_ch) * t
        _, (x, y, z, yaw), uv, _ = info[min(len(info) - 1, bisect.bisect_left(ss, s))]
        _, (rx, ry) = yaw_dirs(yaw)
        sz = [rc.uniform(150.0, 340.0), rc.uniform(120.0, 260.0), rc.uniform(55.0, 100.0)]
        k = rc.random()
        hw = max(abs(q[0]) for q in uv)
        if k < 0.7:           # the pile, rising toward the roof at the end
            u = rc.uniform(-0.9, 0.9) * hw
            v = _ring_floor(uv, u, 60.0)
            rf = _ring_roof(uv, u, 60.0)
            if v is None or rf is None:
                continue
            v += t * rc.uniform(0.0, 0.7) * (rf - v) + rc.uniform(-0.3, 0.1) * sz[2]
        elif k < 0.88:        # from the roof
            u = rc.uniform(-0.8, 0.8) * hw
            v = _ring_roof(uv, u, 60.0)
            if v is None:
                continue
            v -= rc.uniform(0.0, 0.3) * sz[2]
        else:                 # off a wall
            side = 1 if rc.random() < 0.5 else -1
            v = rc.uniform(-0.4, 0.4) * (max(q[1] for q in uv) - min(q[1] for q in uv))
            u = _ring_wall(uv, side, v, 60.0)
            if u is None:
                continue
            u -= side * rc.uniform(0.0, 0.3) * sz[1]
        c = (x + rx * u, y + ry * u, z + v)
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, yaw + rc.uniform(-60.0, 60.0), rc.uniform(-40.0, 40.0), rc.uniform(-40.0, 40.0), rc, 0.12), True)
        nb += 1
    notes['choke'] = nb
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'path': p, 'notes': notes})

    return spec


# Stop 3's cavern (D-069, Nov 18): the string's last chunk, joined to B's exit. A bed
# joint, a tunnel that drops 8.5 m to the lip of a pit (SinkTop), a vertical shaft of at
# least 9.5 m clear radius that opens after about 12 m into the roof of a long dark hall,
# the berth 24 m below SinkTop facing down the hall, the serpent's bay off the middle of
# the hall and the relic's alcove at its far end. No exit: it closes in rock.
C2_L = 4500.0               # the entry joint to the shaft's axis along the tunnel
C2_DROP = (400.0, 3800.0)   # the tunnel's cosine drop, level at both ends: 21 degrees at most
C2_COL = (C2_L, 0.0)        # the shaft's axis
C2_SINKTOP_Z = -850.0       # the lip: the top of the sink (141 m physical depth)
C2_PARK_Z = -3250.0         # the berth, 24 m below SinkTop (165 m)
C2_FLOOR = -3800.0          # silt under the berth: keel gap 3 m, as in C1
C2_COL_R = 1000.0           # the shaft's clear radius before its outward relief
C2_HALL = ((C2_L + 1500.0, 0.0, -3550.0), (2900.0, 1750.0, 1500.0))   # hall centre and radii: 50 m by 35 m at the floor
C2_SERPENT = (C2_L + 2000.0, -2150.0, -2950.0)    # in the bay off the hall's -Y wall
C2_ALCOVE = ((C2_L + 4700.0, 350.0, -3500.0), (950.0, 780.0, 450.0))  # the relic's alcove past the far end
C2_RELIC = (C2_L + 4750.0, 350.0)
C2_PILE = (C2_L + 3000.0, -450.0)                 # breakdown that hides one salvage socket from the hatch
# ledges on the hall walls: (x0, x1, lip y at the socket, wall side, top z); each holds one
# salvage socket at its middle, 2.5 m in from the lip
C2_LEDGES = [(C2_L + 600.0, C2_L + 1400.0, -1050.0, -1, -3000.0), (C2_L + 2200.0, C2_L + 3600.0, 1000.0, 1, -3100.0)]


def c2_shelf(x0, x1, lip, side, zt, seed, hall, step=30.0, nj=18):
    """Round 2: a ledge that is part of the wall's bedding, not a slab (round 1's were boxes
    with straight edges). A closed rock solid over the wall's run (u = x) and the distance in
    from its lip (w). Its lip wanders and, past each end, retreats into the wall, so the ledge
    dies out; its top steps down a bed's fraction at jagged lines either side of the socket (the
    socket keeps the highest tier); its underside is undercut, thin at a rounded lip and
    thickening into the wall. Near the socket (the middle of the run, 250 cm in) the top stays
    level."""
    (hcx, hcy, hcz), (hrx, hry, hrz) = hall
    L_in, L_out = 250.0, 150.0
    um, us = 0.5 * (x0 + x1), abs(lip) + 250.0
    rnd = random.Random(seed)
    steps = [(um + rnd.uniform(230.0, 300.0), rnd.uniform(55.0, 95.0)),
             (um - rnd.uniform(230.0, 300.0), rnd.uniform(55.0, 95.0))]
    if x1 - x0 > 1000.0:
        steps.append((um + rnd.uniform(470.0, 560.0), rnd.uniform(40.0, 70.0)))

    def wall_abs(u):
        q = 1.0 - ((u - hcx) / hrx) ** 2 - ((zt - hcz) / hrz) ** 2
        return hry * math.sqrt(max(0.05, q))

    def env(u):
        return sstep(x0 - L_out, x0 + L_in, u) * (1.0 - sstep(x1 - L_in, x1 + L_out, u))

    def calm(u, w_abs):
        # 0 at the socket, 1 from 2.6 m away
        return sstep(120.0, 260.0, math.hypot(u - um, w_abs - us))

    def lip_abs(u):
        a = wall_abs(u) + 150.0
        base = abs(lip) + (a - abs(lip)) * (1.0 - env(u))
        wob = 85.0 * fbm(u, 0.0, zt, seed + 1, 260.0) + 35.0 * fbm(u, 0.0, zt, seed + 2, 90.0)
        return base + wob * env(u) * sstep(120.0, 300.0, abs(u - um))

    def tier(u, w):
        # the top drops at each step line, which wanders with the distance in from the lip
        dz = 0.0
        for us_, h in steps:
            jag = 45.0 * fbm(w, us_, 0.0, seed + 3, 120.0)
            e = (u - us_ - jag) if us_ > um else (us_ + jag - u)
            dz -= h * sstep(-14.0, 14.0, e)
        return dz

    u0, u1 = x0 - L_out - 30.0, x1 + L_out + 30.0
    ni = max(2, int((u1 - u0) / step))
    top, bot = [], []
    for i in range(ni + 1):
        u = u0 + (u1 - u0) * i / ni
        la = lip_abs(u)
        back = max(wall_abs(u) + 420.0, la + 500.0)
        tlip = 115.0 + 30.0 * fbm(u, 0.0, zt, seed + 4, 200.0)
        rt, rb = [], []
        for j in range(nj + 1):
            w = (back - la) * (j / nj) ** 1.6
            y_abs = la + w
            c = calm(u, y_abs)
            t = tier(u, w)
            zr = zt + t
            # rough top, a rounded droop at the lip
            # the bed sags and swells along the run and the lip droops unevenly, so no edge runs straight
            sag = 45.0 * fbm(u, 0.0, zt, seed + 8, 420.0) + 18.0 * fbm(u, w, zt, seed + 9, 150.0)
            z_top = zr + c * (sag + 16.0 * fbm(u, y_abs, zt, seed + 5, 140.0) + 7.0 * fbm(u, y_abs, zt, seed + 6, 45.0))
            droop = 40.0 + 30.0 * fbm(u, 0.0, zt, seed + 10, 180.0)
            z_top -= droop * (1.0 - sstep(0.0, 110.0, w))
            # undercut: thin at the lip, thickening into the wall, lumpy beneath
            z_bot = zr + c * sag - tlip - 0.75 * w + 28.0 * fbm(u, y_abs, zt - 300.0, seed + 7, 170.0)
            z_bot += 25.0 * (1.0 - sstep(0.0, 70.0, w))
            z_bot = min(z_bot, z_top - 25.0)
            rt.append(U.Vector(u, side * y_abs, z_top))
            rb.append(U.Vector(u, side * y_abs, z_bot))
        top.append(rt); bot.append(rb)
    verts = [v for r in top for v in r] + [v for r in bot for v in r]
    nt = (ni + 1) * (nj + 1)
    idx = lambda i, j: i * (nj + 1) + j
    tris = []
    for i in range(ni):
        for j in range(nj):
            a, b, c, d = idx(i, j), idx(i + 1, j), idx(i + 1, j + 1), idx(i, j + 1)
            tris.append(U.IntVector(a, b, c)); tris.append(U.IntVector(a, c, d))
            tris.append(U.IntVector(nt + a, nt + c, nt + b)); tris.append(U.IntVector(nt + a, nt + d, nt + c))
    border = [idx(i, 0) for i in range(ni)] + [idx(ni, j) for j in range(nj)] + \
             [idx(i, nj) for i in range(ni, 0, -1)] + [idx(0, j) for j in range(nj, 0, -1)]
    for k in range(len(border)):
        a, b = border[k], border[(k + 1) % len(border)]
        tris.append(U.IntVector(a, nt + a, nt + b)); tris.append(U.IntVector(a, nt + b, b))
    return verts, tris


def c2_path():
    p = Path().straight(C2_L)
    d0, d1 = C2_DROP
    dz = C2_SINKTOP_Z

    def zf(s):
        if s <= d0: return 0.0
        if s >= d1: return dz
        return 0.5 * dz * (1.0 - math.cos(math.pi * (s - d0) / (d1 - d0)))
    p.zf = zf
    return p


def c2_salvage():
    """(label, local xy, start z) of the five salvage candidates: two ledge tops (out of
    sight from the hatch), open floor, behind the breakdown pile, and in the bay beside the berth."""
    cx, cy = C2_COL
    out = []
    for x0, x1, ey, side, ztop in C2_LEDGES:
        out.append(('ledge%d' % side, (0.5 * (x0 + x1), ey + side * 250.0), ztop + 250.0))
    out.insert(1, ('floor', (cx + 1800.0, 700.0), C2_PARK_Z - 150.0))
    out.append(('pile', (C2_PILE[0] + 900.0, C2_PILE[1]), C2_PARK_Z - 150.0))
    out.append(('bay', (cx + 600.0, 1500.0), C2_PARK_Z - 150.0))
    return out


def c2_core():
    """The sloping tunnel's swept core (capped at the shaft's axis) and the sink box from
    SinkTop to Park."""
    p = c2_path()
    s_list = ring_s(-EXT, C2_L)
    slices = [core_slice(p, s, lambda s: M_BODY) for s in s_list]
    core = Core()
    core.add_tube(p, s_list, slices)
    # capped: past the shaft's axis the core is the sink box, not the tube run on straight
    core.capped = True
    cx, cy = C2_COL
    hx, hy, hz = core_half(M_BODY)
    top, bot = C2_SINKTOP_Z + hz, C2_PARK_Z - hz
    core.boxes.append(((cx, cy, 0.5 * (top + bot)), (hx, hy, 0.5 * (top - bot)), 0.0))
    return p, s_list, slices, core


def spec_C2(shapes=True):
    """Stop cavern at the end of the string: a dropping tunnel, the pit, the hall."""
    cx, cy = C2_COL
    spec = {'name': 'C2', 'type': 'stop', 'exit': None, 'in': 'bed', 'out': None,
            'sockets': {'Park': (cx, cy, C2_PARK_Z, 0.0), 'SinkTop': (cx, cy, C2_SINKTOP_Z, 0.0),
                        'Serpent': C2_SERPENT + (90.0,)},
            'salvage': c2_salvage(), 'salvage_range': (1000.0, 4500.0),
            'relic': (C2_RELIC, C2_PARK_Z - 150.0, 180.0), 'col': C2_COL}
    if not shapes:
        return spec
    rnd = random.Random(66)
    rock = Rock(66, a_res=55.0, a_blk=34.0, a_fine=14.0, a_sharp=24.0, a_warp=0.0)
    rock_in = Rock(66, a_res=50.0, a_blk=18.0, a_fine=12.0, a_sharp=10.0, a_warp=0.0)
    p, s_list, slices, core = c2_core()
    fade = JointFade(None, types=(spec['in'], None))
    sh = Shapes(66)
    defs = section_defs(66)
    # the tunnel: the bed joint opens into a phreatic tube, then a keyhole down the drop,
    # and a ragged throat where it meets the pit
    secs = Sections([(-EXT, 'j_bed'), (0.0, 'j_bed'), (900.0, 'phreatic'), (2300.0, 'phreatic'),
                     (2900.0, 'keyhole'), (3700.0, 'keyhole'), (4100.0, 'throat'), (C2_L, 'throat')], defs, seed=66)
    # round 2: the floor stands 20 cm further off the core down the drop, where round 1 left
    # three vertices 1.5 cm from it (the core's loft between vertical rings dips there)
    def floor_gap(s, k, c, sn, rc, sl):
        return 20.0 * sstep(-0.2, -0.5, sn) * sstep(600.0, 1000.0, s) * (1.0 - sstep(3900.0, 4200.0, s))
    tv, info = tunnel(p, secs, rock_in, s_list, slices=slices, fade=fade, gap=floor_gap)
    sh.add(tv)
    notes = tunnel_features(sh, info, rock_in, random.Random(67), C2_L - 700.0, core, fade,
                            dome_secs=('phreatic', 'keyhole'), block_secs=(), blocks=0, edge=700.0)
    # the pit: clear radius C2_COL_R, relief only outward, from inside the hall up past the
    # tunnel's roof, under a dome and stepped breakdown beyond the lamp
    sh.add(shaft(cx, cy, -2600.0, 0.0, C2_COL_R, rock, seed=68))
    sh.add(rock_ellipsoid((cx + 100.0, cy - 100.0, 350.0), (850.0, 750.0, 800.0), rock, 'rock', res=45.0, rot_yaw=25.0,
                          fade=fade))
    for vt in joint_dome(cx + 300.0, 200.0, 900.0, 600.0, 480.0, rock.fam[0][0] + 4.0, rock, random.Random(69), levels=3):
        sh.add(vt)
    # the hall: one worn vault, wider than it is tall (overlapping ellipsoids meet in
    # creases that read as holes), with a bay out of its +Y wall near the shaft
    hc, hr = C2_HALL
    sh.add(warp_blob(rock_ellipsoid(hc, hr, rock, 'silt', res=50.0, rot_yaw=3.0, fade=fade, core=core),
                     hc, 0.07, 1400.0, 661))
    bc = (cx + 600.0, 1450.0, -3250.0)
    sh.add(warp_blob(rock_ellipsoid(bc, (950.0, 600.0, 600.0), rock, 'silt', res=45.0, rot_yaw=-20.0, fade=fade, core=core),
                     bc, 0.12, 520.0, 662))
    # stepped breakdown domes in the hall's roof, beyond the lamp
    for (dx, dy, zb, a, b), seed in (((cx + 2400.0, -200.0, -2250.0, 800.0, 600.0), 70),
                                     ((cx + 3400.0, 600.0, -2450.0, 650.0, 500.0), 71)):
        for vt in joint_dome(dx, dy, zb, a, b, rock.fam[1][0] - 3.0, rock, random.Random(seed), levels=3):
            sh.add(vt)
    # the serpent's bay off the middle of the hall's -Y wall, with a narrower mouth
    sx, sy, sz = C2_SERPENT
    sh.add(rock_ellipsoid((sx, sy, sz), (620.0, 600.0, 440.0), rock, 'rock', res=40.0, rot_yaw=8.0, amp=0.5))
    sh.add(rock_ellipsoid((sx, sy + 420.0, sz), (360.0, 420.0, 330.0), rock, 'rock', res=40.0, amp=0.4))
    # the relic's alcove past the far end
    ac, ar = C2_ALCOVE
    sh.add(warp_blob(rock_ellipsoid(ac, ar, rock, 'silt', res=40.0, rot_yaw=-15.0, fade=fade, core=core),
                     ac, 0.12, 500.0, 663))
    # --- rock
    hx0, hy0 = hc[0], hc[1]

    def floor_top(x, y):
        # silt, flat under the berth, banking up toward the walls and rising into the alcove
        e = math.hypot((x - hx0) / hr[0], (y - hy0) / hr[1])
        bank = sstep(0.55, 1.0, e) * (120.0 + 120.0 * fbm(x, y, 0, 72, 500.0))
        dpark = math.hypot(x - cx, y - cy)
        bank *= sstep(900.0, 1500.0, dpark)
        rise = 80.0 * sstep(C2_L + 3800.0, C2_L + 4400.0, x)
        return C2_FLOOR + max(-20.0, bank) + rise + 15.0 * fbm(x, y, 1, 73, 150.0)
    sh.add(heightfield(cx - 2200.0, cx + 6000.0, -2600.0, 2600.0, -5200.0, floor_top), True)
    # ledges: bedding ledges grown from the walls (c2_shelf), tops on bedding planes, clear of
    # the shaft and the serpent's bay
    for i, (x0, x1, ey, side, ztop) in enumerate(C2_LEDGES):
        mx = 0.5 * (x0 + x1)
        k = rock.bed(ztop - (mx * rock.dip[0] + ey * rock.dip[1]))
        zt = rock.beds[k] + mx * rock.dip[0] + ey * rock.dip[1]
        sh.add(c2_shelf(x0, x1, ey, side, zt, 674 + 10 * i, C2_HALL), True)
        # round 1's boxes drew 28 numbers each; keep the breakdown below where it was
        for _ in range(28):
            rnd.random()
    # breakdown: a pile between the berth and one socket, and blocks on the floor, clear of
    # the shaft, the core, the sockets and the relic
    keep = [q[1] for q in c2_salvage()] + [C2_RELIC]
    nb = 0
    for i in range(240):
        if nb >= 32:
            break
        if i < 90:
            a = rnd.uniform(0, 2 * math.pi); d = rnd.uniform(0, 560)
            x, y = C2_PILE[0] + d * math.cos(a), C2_PILE[1] + d * math.sin(a)
            zf = C2_FLOOR + rnd.uniform(0, 330) * (1.0 - d / 600.0)
            sz = (rnd.uniform(160, 420), rnd.uniform(140, 360), rnd.uniform(100, 260))
        else:
            x = rnd.uniform(cx - 1200.0, cx + 4200.0); y = rnd.uniform(-1500.0, 1500.0)
            if math.hypot((x - hx0) / hr[0], (y - hy0) / hr[1]) > 0.85:
                continue
            zf = floor_top(x, y) + rnd.uniform(0, 60)
            sz = (rnd.uniform(110, 380), rnd.uniform(100, 320), rnd.uniform(70, 220))
        hd = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2)
        if math.hypot(x - cx, y - cy) - hd < C2_COL_R + 200.0:
            continue
        if min(math.hypot(x - q[0], y - q[1]) for q in keep) - hd < 400.0:
            continue
        c = (x, y, zf)
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, rnd.uniform(0, 180), rnd.uniform(-30, 30), rnd.uniform(-30, 30), rnd, 0.15), True)
        nb += 1
    notes['blocks'] = nb
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'path': p, 'notes': notes})
    return spec


S_LEN = 4200.0          # Park to the exit joint
S_DROP = (900.0, 3300.0, -250.0)   # cosine drop of 2.5 m on the way out: 9.3 deg at most, level at the joint
S_FLOOR = -545.0        # silt under the berth: keel gap 2.95 m, as in C1
S_SERPENT = (2400.0, -1950.0, 100.0, 90.0)


def s_path():
    p = Path().straight(S_LEN)
    d0, d1, dz = S_DROP

    def zf(s):
        if s <= d0: return 0.0
        if s >= d1: return dz
        return 0.5 * dz * (1.0 - math.cos(math.pi * (s - d0) / (d1 - d0)))
    p.zf = zf
    return p


def spec_S(shapes=True):
    """Start cavern: runs begin here, the sub at its berth (Park, the chunk's origin) with
    its nose toward the way on. One calm chamber, no squeeze and no breakdown: smooth
    water-worn walls, a flat silt floor, the chimney the sub came down in the roof above
    the berth (it narrows and bends out of sight), a Serpent alcove far from the berth, and
    the exit passage's mouth about 26 m ahead of the bow and a little lower, which runs
    down to a rift joint. No entry joint."""
    p = s_path()
    L = p.length
    ex = (S_LEN, 0.0, S_DROP[2], 0.0)
    salvage = [('floor_ahead', (1050.0, 800.0), -300.0), ('floor_left', (-850.0, -750.0), -300.0),
               ('floor_far', (1800.0, -650.0), -300.0), ('floor_back', (-450.0, 1250.0), -300.0)]
    spec = {'name': 'S', 'type': 'start', 'exit': ex, 'in': None, 'out': 'rift', 'path': p,
            'sockets': {'Park': (0.0, 0.0, 0.0, 0.0), 'Serpent': S_SERPENT}, 'salvage': salvage, 'col': (0.0, 0.0)}
    if not shapes:
        return spec
    # the core: the hull at the berth and along the way out (capped behind the berth)
    s_list = ring_s(-1000.0, L + EXT)
    slices = [None if s < -880.0 else core_slice(p, s, lambda q: M_BODY) for s in s_list]
    core = Core()
    core.add_tube(p, s_list, slices)
    # C1's bedded rock with its blocks worn down: water-worn, not blocky
    # (round 6: most of the roof no longer snaps flat to its bedding plane, which read as
    # straight bands, and the beds warp)
    rock = Rock(66, a_res=32.0, a_var=30.0, a_blk=8.0, a_notch=10.0, a_fine=8.0, a_sharp=6.0, roof_snap=0.35)
    # the same beds, broken: for the way out, whose mouth must read as broken rock
    rock_m = Rock(66, a_res=50.0, a_var=30.0, a_blk=30.0, a_notch=18.0, a_fine=10.0, a_sharp=16.0, roof_snap=0.8)
    fade = JointFade(ex, entry=False, types=(None, spec['out']))
    sh = Shapes(66)
    # chamber: one worn ellipsoid, warped at low frequency so its bays and bulges grow out
    # of one surface (Round 5's first try, six overlapping ellipsoids, met in creases that
    # read as round holes from inside)
    sh.add(warp_blob(rock_ellipsoid((300.0, 0.0, 0.0), (2700.0, 1900.0, 1000.0), rock, 'silt', res=45.0, rot_yaw=6.0,
                                    fade=fade, core=core), (300.0, 0.0, 0.0), 0.16, 1500.0, 661))
    # the chimney over the berth: a worn shaft that narrows as it climbs and bends away,
    # so from below its end is out of sight
    sh.add(bent_tube([(-100.0, 150.0, 600.0), (-60.0, 180.0, 1500.0), (250.0, 450.0, 2250.0), (850.0, 800.0, 2700.0),
                      (1350.0, 950.0, 2850.0)], [620.0, 540.0, 420.0, 330.0, 300.0], seed=67, flute=60.0, lobe=0.16))
    # the Serpent's bay off the -Y wall, far from the berth
    sx, sy, sz, _ = S_SERPENT
    # (round 7: taller and lower, so its floor runs under the silt and it has no lower lip,
    # which read as a hard black rim from the hatch)
    sc = (sx - 50.0, sy + 150.0, sz - 140.0)
    sh.add(warp_blob(rock_ellipsoid(sc, (780.0, 680.0, 580.0), rock, 'silt', res=40.0,
                                    rot_yaw=-25.0, amp=0.6), sc, 0.14, 700.0, 662))
    # the way on. Round 5 set a wide, low passage into the chamber's end wall and dressed
    # the mouth with two worn blobs: from the berth they bulged like foam round a door in a
    # wall. Now the passage starts well inside the chamber as a hall wider and taller than
    # the chamber's end, so the end wall itself becomes a funnel that closes in over 12 m
    # (no wall face to hold a door), cut in the broken, bedded rock of rock_m
    s_t = ring_s(1700.0, L + EXT)
    sl_t = [core_slice(p, s, lambda q: M_BODY) for s in s_t]
    secs = Sections([(1700.0, 'hall'), (2300.0, 'hall'), (3200.0, 'widelow'), (L, 'j_rift'), (L + EXT, 'j_rift')],
                    section_defs(66), seed=66)
    tv, info = tunnel(p, secs, rock_m, s_t, slices=sl_t, fade=fade)
    sh.add(tv)
    # breakdown fallen into the funnel's floor corners: angular slabs bedded in the silt,
    # so the mouth's lower outline breaks on rock. (Round 6's first try, round recesses,
    # read as foam again; blocks up the walls floated clear of them)
    rb = random.Random(663)
    nb = 0
    for i in range(200):
        if nb >= 14:
            break
        x = rb.uniform(1900.0, 3700.0)
        hw = 1150.0 - 360.0 * sstep(2300.0, 3200.0, x) - 200.0 * sstep(3200.0, 3800.0, x)
        side = 1.0 if i % 2 else -1.0
        sz = (rb.uniform(200, 480), rb.uniform(170, 380), rb.uniform(90, 230))
        y = side * (hw - rb.uniform(0.1, 0.9) * sz[1])
        zf = min(S_FLOOR - 160.0 * sstep(800.0, 2400.0, x), p.zf(x) - 500.0)
        c = (x, y, zf + rb.uniform(0.1, 0.35) * sz[2])
        rad = 0.5 * math.sqrt(sz[0] ** 2 + sz[1] ** 2 + sz[2] ** 2)
        if core.sdf(c)[0] - rad < DELTA + 10.0:
            continue
        sh.add(obox(c, sz, rb.uniform(0, 180), rb.uniform(-20, 20), rb.uniform(-20, 20), rb, 0.35), True)
        nb += 1
    # the back of the chamber: round 5's read as a box (one wall, straight roof bands, square
    # corners). Bays of bedded rock at three heights break its wall and corners
    # (round 7: in the chamber's own worn rock with a silt floor, and flatter: round 6's, in
    # rock_m with a rock-floor snap and outward-only relief, had hard black lower rims and
    # crumpled insides; the two low bays now reach down under the silt, so they have no
    # lower lip and the floor runs into them)
    for c, r, yaw, sd in (((-2150.0, -950.0, -100.0), (820.0, 560.0, 560.0), 30.0, 671),
                          ((-2300.0, 750.0, -260.0), (780.0, 620.0, 440.0), -25.0, 672),
                          ((-1950.0, 80.0, 560.0), (680.0, 820.0, 340.0), 8.0, 673),
                          ((-1500.0, -1650.0, 300.0), (600.0, 420.0, 380.0), 50.0, 674)):
        sh.add(warp_blob(rock_ellipsoid(c, r, rock, 'silt', res=40.0, rot_yaw=yaw, core=core),
                         c, 0.13, 600.0, sd))
    # (round 7 stood three masses of rock_m out of the funnel's walls to break the mouth's
    # outline; from the berth and the hatch they read as pale boulders, so round 7b took
    # them out and the mouth is round 6's again)
    # (round 8b cut the funnel's walls back in courses of bed_undercut wedges and joint
    # recesses: from the berth the mouth's edge read as torn paper and a patch beside it as a
    # grille, and from S_A's helm both walls carried horizontal smears. Round 8c tried one
    # soft bedding-plane notch per wall instead, after Palmer, Cave Geology (2007) and
    # Slabe, Cave Rocky Relief (1995): a continuous, rounded re-entrant along one bed. From
    # the berth the right one showed past the funnel's edge as a straight tube, a pipe and not
    # rock. So the funnel walls are plain again and the mouth is round 7b's)
    # flat silt floor, easing down to the passage floor at the mouth; it banks up against the
    # walls (round 5's met them in a straight, square crease) but not in the mouth
    def top(x, y):
        z = min(S_FLOOR - 160.0 * sstep(800.0, 2400.0, x), p.zf(x) - 500.0) + 14.0 * fbm(x, y, 0, 67, 350.0)
        e = math.hypot((x - 300.0) / 2700.0, y / 1900.0)
        bank = 150.0 * sstep(0.78, 1.02, e) * (0.6 + 0.4 * fbm(x, y, 0, 68, 500.0))
        bank *= 1.0 - sstep(1500.0, 2100.0, x) * (1.0 - sstep(1100.0, 1500.0, abs(y)))
        return z + max(0.0, bank)
    sh.add(heightfield(-2900, 3600, -2300, 2300, -1600, top), True)
    sh.core.extend(core.meshes())
    spec.update({'shapes': sh, 'core': core, 'notes': {'rings': len(info)}})
    return spec


SPECS = {'S': spec_S, 'A': spec_A, 'B': spec_B, 'T1': spec_T1, 'C1': spec_C1, 'E': spec_E, 'C2': spec_C2}


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
