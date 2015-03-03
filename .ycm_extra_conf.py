import subprocess, os
import ycm_core

flags = ['-Wall']


def DirectoryOfThisScript():
	return os.path.dirname( os.path.abspath( __file__ ) )

compilation_database_folder = os.path.abspath(
				os.path.join(DirectoryOfThisScript(), 'build'))

if os.path.exists( compilation_database_folder ):
	database = ycm_core.CompilationDatabase( compilation_database_folder )
else:
	database = None

SOURCE_EXTENSIONS = [ '.cpp', '.cxx', '.cc', '.c' ]

def MakeRelativePathsInFlagsAbsolute( flags, working_directory ):
	if not working_directory:
		return list( flags )
	new_flags = []
	make_next_absolute = False
	path_flags = [ '-isystem', '-I', '-iquote', '--sysroot=' ]
	for flag in flags:
		new_flag = flag

		if make_next_absolute:
			make_next_absolute = False
			if not flag.startswith( '/' ):
				new_flag = os.path.join( working_directory, flag )

		for path_flag in path_flags:
			if flag == path_flag:
				make_next_absolute = True
				break

			if flag.startswith( path_flag ):
				path = flag[ len( path_flag ): ]
				new_flag = path_flag + os.path.join( working_directory, path )
				break

		if new_flag:
			new_flags.append( new_flag )
	return new_flags


def IsHeaderFile( filename ):
	extension = os.path.splitext( filename )[ 1 ]
	return extension in [ '.h', '.hxx', '.hpp', '.hh' ]


def GetCompilationInfoForFile( filename ):
	if IsHeaderFile( filename ):
		basename = os.path.splitext( filename )[ 0 ]
		for extension in SOURCE_EXTENSIONS:
			replacement_file = basename + extension
			if os.path.exists( replacement_file ):
				compilation_info = database.GetCompilationInfoForFile(
					replacement_file )
				if compilation_info.compiler_flags_:
					return compilation_info
		return None
	return database.GetCompilationInfoForFile( filename )


def GetSystemIncludePaths():
	cache = os.path.join(DirectoryOfThisScript(), "build/.ycm_sys_incs")
	if os.path.exists(cache):
		fp = open(cache, 'r')
		flags = fp.readlines()
		fp.close()
		flags = [s.strip() for s in flags]
	else:
		devnull = open(os.devnull, 'r')
		child = subprocess.Popen(["/usr/bin/cpp", "-xc++", "-v"],
				stdin = devnull, stderr = subprocess.PIPE)
		output = child.communicate()[1].split('\n')
		devnull.close()
		flags = []
		status = 0
		for l in output:
			l = l.strip()
			if l == '#include "..." search starts here:':
				status = 1
			elif l == '#include <...> search starts here:':
				status = 2
			elif status:
				if l == 'End of search list.':
					break
				elif l.endswith('(framework directory)'):
					continue
				elif status == 1:
					flags.append('-I')
				elif status == 2:
					flags.append('-isystem')
				flags.append(os.path.normpath(l))
		fp = open(cache, 'w')
		fp.write('\n'.join(flags))
		fp.close()
	fp = open('build/.includes', 'r')
	if fp:
		workingDir = DirectoryOfThisScript()
		for x in fp.read().split(';'):
			flags.append('-I')
			flags.append(os.path.join(workingDir, x))
		fp.close()
	return flags


def FlagsForFile( filename, **kwargs ):
	if database:
		compilation_info = GetCompilationInfoForFile( filename )
		if compilation_info:
			final_flags = MakeRelativePathsInFlagsAbsolute(
				compilation_info.compiler_flags_,
				compilation_info.compiler_working_dir_ )
			sys_incs = GetSystemIncludePaths()
			if sys_incs:
					final_flags += sys_incs
		else:
			final_flags = ['-Wall']
	else:
		relative_to = DirectoryOfThisScript()
		final_flags = MakeRelativePathsInFlagsAbsolute( flags, relative_to )

	return {
		'flags': final_flags,
		'do_cache': True
	}
