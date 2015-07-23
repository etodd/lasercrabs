import bpy
import math
from math import pi
import sys
import os

try:
	bpy.ops.object.delete()
	
	argv = sys.argv[sys.argv.index("--") + 1:] # get all args after "--"

	ttf_input = argv[0]
	fbx_output = argv[1]

	characters = [
		'!', '"', '#', '$', '%',
		'&', '\'', '(', ')', '*',
		'+', ',', '-', '.', '/',
		'0', '1', '2', '3', '4',
		'5', '6', '7', '8', '9',
		':', ';', '<', '=', '>',
		'?', '@',
		'A', 'B', 'C', 'D', 'E',
		'F', 'G', 'H', 'I', 'J',
		'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T',
		'U', 'V', 'W', 'X', 'Y',
		'Z',
		'[', '\\', ']', '^', '_',
		'`',
		'a', 'b', 'c', 'd', 'e',
		'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y',
		'z',
		'{', '|', '}', '~',
	]

	for character in characters:
		bpy.ops.object.text_add(
		location=(0, 0, 0),
		rotation=(0, 0, 0)
		)
		ob = bpy.context.object
		ob.name = character
		# TextCurve attributes
		ob.data.name = 'TextData{}'.format(ord(character))
		ob.data.body = character
		fnt = bpy.data.fonts.load(os.path.join(os.getcwd(), ttf_input))
		ob.data.font = fnt
		ob.data.size = 1
		# Inherited Curve attributes
		bpy.ops.object.convert(target='MESH', keep_original=False)

	bpy.ops.export_scene.fbx(filepath=fbx_output, axis_forward='-Z', axis_up='Y')
except Exception as e:
	print(str(e), file = sys.stderr)
	exit(1)
exit(0)