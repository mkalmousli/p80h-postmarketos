// 1 GB RAM, 4 slow cores, no GPU: few processes, small caches, no animations that need compositing
pref("dom.ipc.processCount", 2);
pref("browser.cache.memory.capacity", 32768);
pref("browser.sessionhistory.max_total_viewers", 1);
pref("gfx.webrender.software", true);
pref("layers.acceleration.disabled", true);
pref("general.smoothScroll", false);
pref("browser.tabs.unloadOnLowMemory", true);
pref("apz.allow_zooming", true);
pref("browser.startup.homepage_override.mstone", "ignore");
pref("datareporting.policy.dataSubmissionEnabled", false);
pref("browser.shell.checkDefaultBrowser", false);
