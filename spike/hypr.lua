-- Shinto uses Chromium --app windows. Wayland app_id looks like
-- chrome-<host>__<path>-Default, not the --class flag.

o.window("shinto", { tag = "+chromium-based-browser" })
o.window("^chrome-.*-Default$", { tag = "+chromium-based-browser" })

-- Keep-alive page and the brief launch trampoline stay off-screen.
o.window("chrome-badlilcpkdpfckbkeaejdnpinhejkiao__(spare|launch)\\.html-Default", {
  workspace = "special:shinto-spare silent",
  no_initial_focus = true,
  group = "barred",
})
