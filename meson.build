project(
  'sgreet',
  'c',
  version : '1.0',
  default_options: ['c_std=c11', 'warning_level=2', 'b_ndebug=if-release'],
)

version_str = meson.project_version()

git_prog = find_program('git', required: false)
if git_prog.found()
  git_cmd = run_command('git', 'describe', '--always', '--dirty', check: false)
  if git_cmd.returncode() == 0
    version_str = '@0@ (@1@)'.format(meson.project_version(), git_cmd.stdout().strip())
  endif
endif

version_macro = '-DSGREET_VERSION="@0@ @1@"'.format(meson.project_name(), version_str)

add_project_arguments(
  ['-D_GNU_SOURCE', version_macro],
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
