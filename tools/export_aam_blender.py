"""Blender exporter for Arena Assault AAM1/AAM2.

Default export is AAM2 with skin weights. The mesh may use these vertex-group
names (case-insensitive):
  root, torso, head, arm_l, arm_r, leg_l, leg_r, weapon

Up to four strongest weights are stored per vertex. Missing weights fall back
to root=1.0. Bone IDs are fixed to keep the runtime skeleton tiny and fast.

Usage in Blender Scripting workspace:
    export_selected(r"C:\\temp\\enemy_body.aam2", skinned=True)
    export_selected(r"C:\\temp\\enemy_static.aam", skinned=False)

Coordinate conversion: Blender Z-up -> game Y-up
  game.x = blender.x
  game.y = blender.z
  game.z = -blender.y
"""

import bpy
import struct
from mathutils import Vector

BONE_IDS = {
    'root': 0,
    'torso': 1,
    'head': 2,
    'arm_l': 3,
    'armleft': 3,
    'left_arm': 3,
    'arm_r': 4,
    'armright': 4,
    'right_arm': 4,
    'leg_l': 5,
    'legleft': 5,
    'left_leg': 5,
    'leg_r': 6,
    'legright': 6,
    'right_leg': 6,
    'weapon': 7,
}
BONE_COUNT = 8


def to_game(v):
    return (float(v.x), float(v.z), float(-v.y))


def skin_for_vertex(obj, vertex):
    candidates = []
    for membership in vertex.groups:
        if membership.group >= len(obj.vertex_groups):
            continue
        name = obj.vertex_groups[membership.group].name.lower().replace(' ', '_')
        bone = BONE_IDS.get(name)
        if bone is not None and membership.weight > 0.00001:
            candidates.append((float(membership.weight), bone))
    candidates.sort(reverse=True)
    candidates = candidates[:4]
    if not candidates:
        candidates = [(1.0, 0)]
    total = sum(w for w, _ in candidates) or 1.0
    bones = [b for _, b in candidates]
    weights = [w / total for w, _ in candidates]
    while len(bones) < 4:
        bones.append(0)
        weights.append(0.0)
    return tuple(bones), tuple(weights)


def export_selected(path, skinned=True):
    obj = bpy.context.active_object
    if obj is None or obj.type != 'MESH':
        raise RuntimeError("Select one mesh object first")

    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = obj.evaluated_get(depsgraph)
    mesh = eval_obj.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
    try:
        mesh.calc_loop_triangles()
        uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
        normal_matrix = obj.matrix_world.to_3x3().inverted().transposed()

        vertices = []
        indices = []
        dedupe = {}

        for tri in mesh.loop_triangles:
            for loop_index in tri.loops:
                loop = mesh.loops[loop_index]
                vertex = mesh.vertices[loop.vertex_index]
                world_pos = obj.matrix_world @ vertex.co
                world_normal = (normal_matrix @ loop.normal).normalized()
                uv = uv_layer[loop_index].uv if uv_layer else Vector((0.0, 0.0))

                p = to_game(world_pos)
                n = to_game(world_normal)
                geom = (*p, *n, float(uv.x), float(uv.y))
                if skinned:
                    bones, weights = skin_for_vertex(obj, vertex)
                    key = tuple(round(x, 7) for x in geom) + bones + tuple(round(w, 7) for w in weights)
                else:
                    bones, weights = None, None
                    key = tuple(round(x, 7) for x in geom)

                idx = dedupe.get(key)
                if idx is None:
                    idx = len(vertices)
                    dedupe[key] = idx
                    vertices.append((geom, bones, weights))
                indices.append(idx)

        if not vertices or not indices:
            raise RuntimeError("Mesh produced no triangles")

        with open(path, 'wb') as f:
            if skinned:
                f.write(struct.pack('<4sIIII', b'AAM2', 2, len(vertices), len(indices), BONE_COUNT))
                for geom, bones, weights in vertices:
                    f.write(struct.pack('<8f4I4f', *geom, *bones, *weights))
            else:
                f.write(struct.pack('<4sIII', b'AAM1', 1, len(vertices), len(indices)))
                for geom, _, _ in vertices:
                    f.write(struct.pack('<8f', *geom))
            for i in indices:
                f.write(struct.pack('<I', i))

        kind = 'AAM2 skinned' if skinned else 'AAM1 static'
        print(f"Arena Assault: wrote {kind} {path}: {len(vertices)} vertices, {len(indices)//3} triangles")
    finally:
        eval_obj.to_mesh_clear()


# Example:
# export_selected(r"C:\\temp\\enemy_body.aam2", skinned=True)
