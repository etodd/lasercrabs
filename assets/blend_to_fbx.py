import bpy
import sys
 
try:
	argv = sys.argv
	argv = argv[argv.index('--') + 1:] # get all args after "--"
	 
	fbx_out = argv[0]
	 
	bpy.ops.export_scene.fbx(filepath=fbx_out,
		axis_forward='-Z',
		axis_up='Y',
		use_custom_props = True,
		object_types = {'EMPTY', 'CAMERA', 'LAMP', 'ARMATURE', 'MESH', 'OTHER'},
	)
except Exception as e:
	print(str(e), file = sys.stderr)
	exit(1)