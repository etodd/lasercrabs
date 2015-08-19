import json
import bpy
import sys
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:] # get all args after "--"
output_file = argv[0]

meshes = {}
index = 0
for o in sorted((x for x in bpy.data.objects if x.type == 'MESH'), key = lambda x: x.name):
	materials = max(1, len(o.data.materials))
	meshes[o] = list(range(index, index + materials))
	index += materials

result = []
def add(obj, parent_index):
	pos = obj.matrix_basis.translation
	axis, angle = obj.matrix_basis.to_quaternion().to_axis_angle()
	rot = mathutils.Quaternion(mathutils.Vector([axis.y, axis.z, axis.x]), angle)
	node = {
		'pos': [pos.y, pos.z, pos.x],
		'rot': [rot.w, rot.x, rot.y, rot.z],
		'name': obj.name,
		'parent': parent_index,
	}
	for key in obj.keys():
		if key != '_RNA_UI':
			node[key] = obj[key]
	if obj.type == 'MESH':
		node['meshes'] = meshes[obj]
	result.append(node)
	index = len(result) - 1
	for child in obj.children:
		add(child, index)
	
for obj in (x for x in bpy.data.objects if x.parent is None):
	add(obj, -1)

with open(output_file, 'w') as f:
	json.dump(result, f)
