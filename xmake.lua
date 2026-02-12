add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina", {configs = {target_type = "server"}})
add_requires("sqlitecpp")

add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("BakaPerms")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE")
    add_defines("BAKA_PERM_EXPORT")
    add_packages("levilamina")
    add_packages("sqlitecpp")
    set_exceptions("none") -- To avoid conflicts with /EHa.
    set_kind("shared")
    set_languages("c++23")
    set_symbols("debug")
    add_headerfiles("src/(BakaPerms/**.h)", "src/(BakaPerms/**.hpp)")
    add_files("src/**.cpp")
    add_files("src/BakaPerms/BakaPerms.rc")
    add_includedirs("src")