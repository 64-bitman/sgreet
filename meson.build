project(
  'sgreet',
  'c',
  default_options: ['c_std=c11', 'warning_level=2', 'b_ndebug=if-release'],
)

add_project_arguments(
  ['-D_GNU_SOURCE'],
  language: 'c'
)

sources = []
includes = []

json_dep = dependency('json-c')
ncurses_dep = dependency('ncurses')

subdir('src')

wlip = executable(
  'sgreet',
  'src/main.c',
  sources,
  include_directories: includes,
  dependencies: [json_dep, ncurses_dep],
  install: true
)
