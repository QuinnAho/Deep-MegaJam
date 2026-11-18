"""DEEP cave dive: export cavestring.dive_path to Content/Data/CaveChunkPath.csv (editor Python).

Each sample of the dive path (the sub's origin, the hatch) is written in the frame of the
chunk it was laid out in (Chunk, X, Y, Z, Yaw, Region; cm, degrees), so the game can
place it on the string wherever the string hangs (Sim/DeepCaveStringRules::PathPoses).
The chunk is taken from the place cavestring.compose was called with, so no sample is
guessed from its position. The tool's output is not changed.
    import export_cave_path as ep; ep.export()
"""
import importlib
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
CAVEGEN = os.path.normpath(os.path.join(HERE, '..', 'CaveGen', 'Python'))
if CAVEGEN not in sys.path:
    sys.path.append(CAVEGEN)

import cavestring as cs  # noqa: E402

COLS = ['Chunk', 'X', 'Y', 'Z', 'Yaw', 'Region']


def export(step=25.0, turn_step=2.0):
    """Writes the path CSV; returns (path, sample count, largest round-trip error in cm)."""
    rows = cs.load_chunks()
    plist, _ = cs.string_places(rows)
    by_place = {tuple(pl): name for name, pl in plist}
    calls = []
    real = cs.compose

    def logged(place, local):
        out = real(place, local)
        calls.append((tuple(place), tuple(local), out))
        return out

    cs.compose = logged
    try:
        pts = cs.dive_path(rows, step, turn_step)
    finally:
        cs.compose = real
    # dive_path first composes the string itself (string_places: one call per chunk); the
    # rest are its samples, one call each, in order.
    samples = calls[len(cs.STRING):]
    if len(samples) != len(pts):
        raise RuntimeError('expected %d sample composes, got %d' % (len(pts), len(samples)))
    out = []
    worst = 0.0
    for (place, local, w), p in zip(samples, pts):
        name = by_place.get(place)
        if name is None:
            raise RuntimeError('a sample was composed in no chunk of the string: %r' % (place,))
        if len(local) < 4:
            raise RuntimeError('a sample without a yaw: %r' % (local,))
        worst = max(worst, math.dist(w[:3], p[:3]))
        out.append({'Chunk': name, 'X': float(local[0]), 'Y': float(local[1]), 'Z': float(local[2]),
                    'Yaw': float(local[3]), 'Region': p[4]})
    # Round trip: the written (rounded) local points compose back onto the path.
    places = dict(plist)
    for r, p in zip(out, pts):
        w = real(places[r['Chunk']], (round(r['X'], 3), round(r['Y'], 3), round(r['Z'], 3), r['Yaw']))
        worst = max(worst, math.dist(w[:3], p[:3]))
    path = os.path.join(cs.data_dir(), 'CaveChunkPath.csv')
    cs.write_csv(path, COLS, out)
    unreal.log('export_cave_path: %d samples to %s (round trip within %.4f cm)' % (len(out), path, worst))
    return path, len(out), worst


if __name__ == '__main__':
    importlib.reload(cs)
    print(export())
