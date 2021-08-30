executable('upower',
    sources: [
        'up-tool.c',
    ],
    dependencies: [ libupower_glib_dep ],
    gnu_symbol_visibility: 'hidden',
    install: true,
    install_dir: get_option('prefix') / get_option('bindir'),
    include_directories: [ '../libupower-glib/' ],
    c_args: [ '-DG_LOG_DOMAIN="UPower"' ],
)
