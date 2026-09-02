-- Shinto uses Chromium --app windows. Wayland app_id looks like
-- chrome-<host>__<path>-Default, not the --class flag.

o.window("shinto", { tag = "+chromium-based-browser" })
o.window("^chrome-.*-Default$", { tag = "+chromium-based-browser" })

-- Keep-alive page stays off-screen (loopback spare, same origin as the gate).
o.window("chrome-127\\.0\\.0\\.1__spare\\.html-Default", {
  workspace = "special:shinto-spare silent",
  no_initial_focus = true,
  group = "barred",
})
-- Old extension-origin spare (pre-loopback keep-alive).
o.window("chrome-badlilcpkdpfckbkeaejdnpinhejkiao__(spare|launch)\\.html-Default", {
  workspace = "special:shinto-spare silent",
  no_initial_focus = true,
  group = "barred",
})
