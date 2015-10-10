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
def add(obj, parent_index = -1):
	node = {
		'name': obj.name,
		'parent': parent_index,
	}

	if hasattr(obj, 'matrix_local'):
		pos = obj.matrix_local.translation
		axis, angle = obj.matrix_local.to_quaternion().to_axis_angle()
		rot = mathutils.Quaternion(mathutils.Vector([axis.y, axis.z, axis.x]), angle)
		node['pos'] = [pos.y, pos.z, pos.x]
		node['rot'] = [rot.w, rot.x, rot.y, rot.z]

	for key in obj.keys():
		if key != '_RNA_UI':
			node[key] = obj[key]

	if getattr(obj, 'type', None) == 'MESH':
		node['meshes'] = meshes[obj]

	result.append(node)
	index = len(result) - 1

	if hasattr(obj, 'children'):
		for child in obj.children:
			add(child, index)

	return node
	
for obj in (x for x in bpy.data.objects if x.parent is None):
	add(obj)

world_node = add(bpy.data.worlds[0])
world_node['World'] = True
world_node['skybox_color'] = list(bpy.data.worlds[0].horizon_color)

with open(output_file, 'w') as f:
	json.dump(result, f)
