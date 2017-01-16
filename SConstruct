# SConstruct for FontSystem library

obj_env = Environment()
obj_env.Append( CCFLAGS = '--std=c++11' )
obj_env.Append( CCFLAGS = '-DLINUX' )
obj_env.Append( CCFLAGS = '-I/usr/include/freetype2' )
obj_env.Append( CCFLAGS = '-ggdb' )

cpp_source_list = Glob( 'Code/*.cpp' )
h_source_list = Glob( 'Code/*.h' )
source_list = cpp_source_list + h_source_list

object_list = []
for source_file in cpp_source_list:
  object_file = obj_env.StaticObject( source_file )
  object_list.append( object_file )

lib_env = Environment()
lib_env.Append( LIBS = '-lGL' )
lib_env.Append( LIBS = '-lGLU' )
lib_env.Append( LIBS = '-lfreetype6' )
lib_env.StaticLibrary( 'FontSystem', object_list )
