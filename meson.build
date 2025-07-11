project('perentie', 'c',
  version : '0.9.1',
  license : 'GPL-3.0-or-later', 
  default_options : ['warning_level=1', 'optimization=2'],
  meson_version: '>=1.4.0'
)

fs = import('fs')

if host_machine.system() == 'msdos'
  exe_name = 'pt_base.exe'
  platform_src = [
    'src/dos.c',
    'src/dos.h',
    'src/dos_vga.c',
  ]
  platform_args = [
    '-DSYSTEM_DOS',
  ]
  platform_link_args = []
  platform_deps = []
elif host_machine.system() == 'emscripten'
  em_pre_js = files('src/em_pre.js')[0]
  exe_name = 'perentie'
  platform_src = [
    'src/sdl.c',
    'src/sdl.h'
  ]
  platform_args = [
    '-DSYSTEM_SDL', '-DOPLTYPE_IS_OPL3', '--use-port=sdl3',
  ]
  platform_link_args = ['--use-port=sdl3', '-sFORCE_FILESYSTEM', '-lidbfs.js', '--pre-js', em_pre_js.full_path()]
  # we don't need threads... yet
  #if false
  #  platform_args += ['-sUSE_PTHREADS=1']
  #  platform_link_args += ['-sUSE_PTHREADS=1']
  #endif
  if get_option('buildtype') in ['debug', 'debugoptimized']
    platform_link_args += ['-gsource-map']
  endif
  platform_deps = []
  fs.copyfile('html/index.html')
  fs.copyfile('html/style.css')
  run_target('webserver',
    command : 'scripts/webserver.py')

else
  exe_name = 'perentie'
  sdl_dep = dependency(
    'sdl3', version : '>=3.0.0'
  )
  platform_src = [
    'src/sdl.c',
    'src/sdl.h',
  ]
  platform_args = [
    '-DSYSTEM_SDL', '-DOPLTYPE_IS_OPL3',
  ]
  platform_link_args = []
  platform_deps = [sdl_dep]
endif


cc = meson.get_compiler('c')
libm = cc.find_library('m', required : true)

prog_lua = find_program('lua', '/usr/bin/lua', required : true)
prog_luac = find_program('luac', '/usr/bin/luac', required : true)
prog_ldoc = find_program('ldoc', required : false)

liblua = static_library(
  'lua',
  [
    'src/lua/lapi.c',
    'src/lua/lapi.h',
    'src/lua/lauxlib.c',
    'src/lua/lauxlib.c',
    'src/lua/lbaselib.c',
    'src/lua/lcode.c',
    'src/lua/lcode.h',
    'src/lua/lcorolib.c',
    'src/lua/lctype.c',
    'src/lua/lctype.h',
    'src/lua/ldblib.c',
    'src/lua/ldebug.c',
    'src/lua/ldebug.h',
    'src/lua/ldo.c',
    'src/lua/ldo.h',
    'src/lua/ldump.c',
    'src/lua/lfunc.c',
    'src/lua/lfunc.h',
    'src/lua/lgc.c',
    'src/lua/lgc.h',
    'src/lua/linit.c',
    'src/lua/liolib.c',
    'src/lua/ljumptab.h',
    'src/lua/llex.c',
    'src/lua/llex.h',
    'src/lua/llimits.h',
    'src/lua/lmathlib.c',
    'src/lua/lmem.c',
    'src/lua/lmem.h',
    'src/lua/loadlib.c',
    'src/lua/lobject.c',
    'src/lua/lobject.h',
    'src/lua/lopcodes.c',
    'src/lua/lopcodes.h',
    'src/lua/lopnames.h',
    'src/lua/loslib.c',
    'src/lua/lparser.c',
    'src/lua/lprefix.h',
    'src/lua/lstate.c',
    'src/lua/lstate.h',
    'src/lua/lstring.c',
    'src/lua/lstring.h',
    'src/lua/lstrlib.c',
    'src/lua/ltable.c',
    'src/lua/ltable.h',
    'src/lua/ltablib.c',
    'src/lua/ltests.c',
    'src/lua/ltests.h',
    'src/lua/ltm.c',
    'src/lua/ltm.h',
    'src/lua/lua.c',
    'src/lua/lua.h',
    'src/lua/luaconf.h',
    'src/lua/lualib.h',
    'src/lua/lundump.c',
    'src/lua/lundump.h',
    'src/lua/lutf8lib.c',
    'src/lua/lvm.c',
    'src/lua/lvm.h',
    'src/lua/lzio.c',
    'src/lua/lzio.h',
    'src/lua/lparser.h',
  ],
  c_args : ['-fno-stack-protector', '-fno-common'] + platform_args,
)


libminiz = static_library(
  'miniz',
  [
    'src/miniz/miniz.c',
    'src/miniz/miniz.h',
    'src/miniz/miniz_common.h',
    'src/miniz/miniz_export.h',
    'src/miniz/miniz_tdef.c',
    'src/miniz/miniz_tdef.h',
    'src/miniz/miniz_tinfl.c',
    'src/miniz/miniz_tinfl.h',
    'src/miniz/miniz_zip.c',
    'src/miniz/miniz_zip.h',
  ],
  c_args : platform_args
)

libspng = static_library(
  'spng',
  [
    'src/spng/spng.c',
    'src/spng/spng.h'
  ],
  c_args : ['-DSPNG_STATIC', '-DSPNG_DISABLE_OPT=1', '-DSPNG_USE_MINIZ'] + platform_args,
  include_directories : ['src/miniz'],
)

libwave = static_library(
  'wave',
  [
    'src/wave/wave.c',
    'src/wave/wave.h'
  ],
  c_args : ['-D__STDC_CONSTANT_MACROS', '-D__STDC_FORMAT_MACROS', '-D__STDC_LIMIT_MACROS', '-Dwave_EXPORTS'] + platform_args,
  include_directories : ['src/wave'],
)

woodyopl = static_library(
  'woodyopl',
  [
    'src/woodyopl/opl.c',
    'src/woodyopl/opl.h',
  ],
  c_args : ['-DOPLTYPE_IS_OPL3'] + platform_args,
  include_directories : ['src/woodyopl'],
)

woodypc = static_library(
  'woodypc',
  [
    'src/woodypc/pcspeak.c',
    'src/woodypc/pcspeak.h',
  ],
  c_args : platform_args,
  include_directories : ['src/woodypc'],
)


siphash = static_library(
  'siphash',
  [
    'src/siphash/halfsip.c',
  ],
)

simplex = static_library(
  'simplex',
  [
    'src/simplex/simplex.c',
  ],
)

if host_machine.system() == 'msdos'
  physfs_args = ['-DPHYSFS_NO_64BIT_SUPPORT']
else
  physfs_args = []
endif

physfs = static_library(
  'physfs',
  [
    'src/physfs/physfs.c',
    'src/physfs/physfs_archiver_dir.c',
    'src/physfs/physfs_archiver_zip.c',
    'src/physfs/physfs_byteorder.c',
    'src/physfs/physfs_unicode.c',
    'src/physfs/physfs_platform_djgpp.c',
    'src/physfs/physfs_platform_posix.c',
    'src/physfs/physfs_platform_unix.c'
  ],
  c_args : ['-DPHYSFS_SUPPORTS_DEFAULT=0', '-DPHYSFS_SUPPORTS_ZIP'] + physfs_args + platform_args,
  include_directories : ['src/miniz'],
)

if host_machine.system() == 'msdos'
  platform_libs = []
else
  platform_libs = [woodyopl, woodypc]
endif

boot_lo = custom_target(
  'boot.lo',
  input : 'src/boot.lua',
  output : 'boot.lo',
  command : [prog_luac, '-o', '@OUTPUT@', '@INPUT@']
)

boot_h = custom_target(
  'boot.h',
  input : ['src/bin2c.lua', boot_lo],
  output : 'boot.h',
  command : [prog_lua, '@INPUT0@', '@INPUT1@'],
  capture : true
)

cbor_lo = custom_target(
  'cbor.lo',
  input : 'src/cbor.lua',
  output : 'cbor.lo',
  command : [prog_luac, '-o', '@OUTPUT@', '@INPUT@']
)

cbor_h = custom_target(
  'cbor.h',
  input : ['src/bin2c.lua', cbor_lo],
  output : 'cbor.h',
  command : [prog_lua, '@INPUT0@', '@INPUT1@'],
  capture : true
)

inspect_lo = custom_target(
  'inspect.lo',
  input : 'src/inspect.lua',
  output : 'inspect.lo',
  command : [prog_luac, '-o', '@OUTPUT@', '@INPUT@']
)

inspect_h = custom_target(
  'inspect.h',
  input : ['src/bin2c.lua', inspect_lo],
  output : 'inspect.h',
  command : [prog_lua, '@INPUT0@', '@INPUT1@'],
  capture : true
)

luascripts_dep = declare_dependency(
  sources : [boot_h, cbor_h, inspect_h],
  include_directories : include_directories('.'),
)

core_src = [
  'src/colour.c', 
  'src/colour.h', 
  'src/event.c', 
  'src/event.h', 
  'src/font.c', 
  'src/font.h', 
  'src/fs.c', 
  'src/fs.h', 
  'src/image.c', 
  'src/image.h', 
  'src/log.c', 
  'src/log.h', 
  'src/image.h', 
  'src/main.c', 
  'src/musicrad.c', 
  'src/musicrad.h', 
  'src/pcspeak.c',
  'src/pcspeak.h',
  'src/point.h', 
  'src/rect.h', 
  'src/repl.c', 
  'src/repl.h', 
  'src/script.c', 
  'src/script.h', 
  'src/system.c',
  'src/system.h',
  'src/text.c',
  'src/text.h',
  'src/utils.h',
  'src/version.h',
]

deps = [
  libm
]

libs = [
  liblua,
  libminiz,
  libspng,
  libwave,
  siphash,
  simplex,
  physfs
]

exe = executable(
  exe_name,
  sources : core_src + platform_src + [
    boot_h,
    cbor_h,
    inspect_h
  ],
  c_args : platform_args,
  link_args : platform_link_args,
  dependencies : deps + platform_deps + luascripts_dep,
  link_with : libs + platform_libs,
)

if host_machine.system() == 'msdos'
  # DJGPP will build EXEs with a loader stub that looks for CWSDPMI.EXE.
  # But we have the option of replacing the stub with CWSDPMI itself, so
  # we may as well do that.
  prog_exe2coff = find_program('exe2coff', required : true)
  exe_coff = custom_target(
    'pt_base',
    input : [exe],
    output : 'pt_base',
    depends : [exe],
    command : [prog_exe2coff, '@INPUT0@'],
    build_by_default : true
  )

  prog_final = custom_target(
    'perentie.exe',
    input : ['src/CWSDSTUB.EXE', exe_coff],
    output : 'perentie.exe',
    depends: [exe_coff],
    command : ['cat', '@INPUT0@', '@INPUT1@'],
    capture : true,
    build_by_default : true
  )
endif

if prog_ldoc.found()
  doc = custom_target(
    'doc',
    input : 'src/boot.lua',
    output : 'doc',
    command : [prog_ldoc, '-p', 'Perentie', '-d', '@OUTPUT@', '@INPUT@'],
  )
endif
