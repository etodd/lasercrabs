import bpy
import sys
 
try:
	argv = sys.argv
	argv = argv[argv.index('--') + 1:] # get all args after "--"
	 
	fbx_out = argv[0]
	 
	bpy.ops.export_scene.fbx(filepath=fbx_out,
		axis_forward = '-Z',
		axis_up = 'Y',
		object_types = {'EMPTY', 'CAMERA', 'LAMP', 'ARMATURE', 'MESH', 'OTHER'},
		bake_anim = True,
		bake_anim_use_all_bones = False,
		use_armature_deform_only = False,
		bake_anim_use_nla_strips = False,
		bake_anim_use_all_actions = True,
		use_anim = True,
		use_anim_action_all = True,
	)
except Exception as e:
	print(str(e), file = sys.stderr)
	exit(1)
