// Teclast P80h: touch, ~1 GB RAM, no GPU driver (software WebRender).
pref("dom.w3c_touch_events.enabled", 1);
pref("apz.allow_zooming", true);
pref("apz.gtk.kinetic_scroll.enabled", true);
pref("gfx.webrender.software", true);
pref("dom.ipc.processCount", 2);
pref("dom.ipc.processCount.webIsolated", 1);
pref("fission.autostart", false);
pref("browser.sessionstore.interval", 60000);
pref("browser.cache.disk.capacity", 102400);
pref("media.ffmpeg.vaapi.enabled", false);
