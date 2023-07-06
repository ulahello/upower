rules = [
    '95-upower-wup.rules',
    '95-upower-hid.rules',
]

hwdb = [
    '95-upower-hid.hwdb',
]

if os_backend == 'linux'
    install_data(
        rules,
        install_dir: udevrulesdir,
    )

    install_data(
        hwdb,
        install_dir: udevhwdbdir,
    )
endif
