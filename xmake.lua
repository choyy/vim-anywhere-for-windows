set_version("1.0.0")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})
set_languages("c++20")
set_encodings("utf-8")

target("vim-anywhere-for-windows")
    set_arch("x86")
    set_kind("binary")
    set_optimize("smallest")

    add_files("src/main.cpp", "src/editor.cpp", "res/*.rc")
    add_includedirs("build")
    add_configfiles("src/version.h.in")

    add_cxflags("/GR-")
    add_ldflags("/SUBSYSTEM:WINDOWS")
    add_syslinks("user32", "gdi32", "shell32", "advapi32")
