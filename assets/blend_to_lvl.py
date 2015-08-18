import json
import bpy
import sys

argv = sys.argv[sys.argv.index("--") + 1:] # get all args after "--"
output_file = argv[0]

meshes = {}
index = 0
for mesh in bpy.data.meshes:
    materials = len(mesh.materials)
    meshes[mesh] = list(range(index, materials))
    index += materials

result = []
def add(obj, parent_index):
    pos = obj.location
    rot = obj.rotation_quaternion
    node = {
        'pos': [pos.x, pos.y, pos.z],
        'rot': [rot.w, rot.x, rot.y, rot.z],
        'name': obj.name,
        'parent': parent_index,
    }
    for key in obj.keys():
        if key != '_RNA_UI':
            node[key] = obj[key]
    if obj.type == 'MESH':
        node['meshes'] = meshes[obj.data]
    result.append(node)
    index = len(result) - 1
    for child in obj.children:
        add(child, index)
    
for obj in (x for x in bpy.data.objects if x.parent is None):
    add(obj, -1)

with open(output_file, 'w') as f:
    json.dump(result, f)