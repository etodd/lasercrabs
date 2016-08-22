import bpy
import sys
import os

sys.path.append(os.path.dirname(__file__))
from etodd_blender_fbx import export_fbx_bin
 
class DummyOperator(object):
	def __init__(self):
		self.report = None
	 
try:
	argv = sys.argv
	argv = argv[argv.index('--') + 1:] # get all args after "--"
	 
	fbx_out = argv[0]

	export_fbx_bin.save(DummyOperator(), bpy.context, filepath=fbx_out,
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
		add_leaf_bones = False,
		bake_anim_force_startend_keying = False,
		use_mesh_edges = False,
		use_tspace = False,
	)

except Exception as e:
	print(str(e), file = sys.stderr)
	exit(1)