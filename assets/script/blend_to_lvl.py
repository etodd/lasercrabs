import json
import bpy
import sys
import mathutils
import os

IGNORE_PROPS = {'_RNA_UI', 'cycles'}

result = []

def clean_name(name):
	result = []
	for i in range(len(name)):
		c = name[i]
		if (c < 'A' or c > 'Z') \
			and (c < 'a' or c > 'z') \
			and (i == 0 or c < '0' or c > '9') \
			and c != '_':
			result.append('_')
		else:
			result.append(c)
	return ''.join(result)

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
		node['scale'] = obj.scale.length

	for key in obj.keys():
		if key not in IGNORE_PROPS:
			node[key] = obj[key]

	obj_type = getattr(obj, 'type', None)
	if obj_type == 'MESH':
		meshes = []
		clean_obj_name = clean_name(obj.name)
		meshes.append('{0}_{1}'.format(output_asset_name, clean_obj_name))
		for i in range(1, len(obj.data.materials)):
			meshes.append('{0}_{1}_{2}'.format(output_asset_name, clean_obj_name, i))
		node['meshes'] = meshes
	elif obj_type == 'LAMP':
		lamp_type = obj.data.type
		if lamp_type == 'POINT':
			node['PointLight'] = True
			node['radius'] = obj.data.distance
			node['color'] = list(obj.data.color)
		elif lamp_type == 'SUN':
			node['DirectionalLight'] = True
			node['color'] = list(obj.data.color)
			node['shadowed'] = obj.data.shadow_method != 'NOSHADOW'
	elif obj_type == 'CAMERA':
		node['Camera'] = True
		node['far_plane'] = obj.data.clip_end

	result.append(node)
	index = len(result) - 1

	if hasattr(obj, 'children'):
		for child in obj.children:
			add(child, index)

	if hasattr(obj, 'constraints'):
		node['links'] = links = []
		for constraint in obj.constraints:
			if constraint.type == 'ACTION' and constraint.target is not None:
				links.append(constraint.target.name)

	return node

try:
	argv = sys.argv[sys.argv.index('--') + 1:] # get all args after '--'
	output_file = argv[0]

	output_asset_name = clean_name(os.path.basename(output_file)[:-4])

	world_node = add(bpy.data.worlds[0])
	world_node['World'] = True
	world_node['skybox_color'] = list(bpy.data.worlds[0].horizon_color)
	world_node['ambient_color'] = list(bpy.data.worlds[0].ambient_color)
	world_node['zenith_color'] = list(bpy.data.worlds[0].zenith_color)
		
	for obj in (x for x in bpy.data.objects if x.parent is None):
		add(obj)

	with open(output_file, 'w') as f:
		json.dump(result, f)
except Exception as e:
	print(str(e), file = sys.stderr)
	exit(1)
