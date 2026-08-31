-- Hyprland config for the hyprlogin greeter session.
-- https://wiki.hypr.land/Configuring/Start/

hl.on("hyprland.start", function()
    hl.exec_cmd("hyprlogin --verbose")
end)

hl.monitor({
    output   = "",
    mode     = "preferred",
    position = "auto",
    scale    = 1,
})

hl.config({
    input = {
        kb_layout  = "us,fr",
        kb_variant = ",azerty",
        kb_options = "grp:alt_shift_toggle",
    },
})
