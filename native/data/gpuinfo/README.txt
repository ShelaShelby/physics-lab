vulkan.gpuinfo.org (the Vulkan Hardware Database by Sascha Willems), read 2026-11-21 for 0.69.0's rule on Vulkan 1.2 (owner decision 20 Nov 2026).
The files themselves are in the private sources repo (ShelaShelby/physics-lab-sources, data/gpuinfo/; listed in native/data/SOURCES.md):
the Windows device list (each device's newest reported Vulkan version and driver), the same list filtered by VK_KHR_dynamic_rendering and by
VK_KHR_synchronization2, and report 22394 (Radeon Pro 580, driver 2.0.204, Vulkan 1.2.196) that shows both features true.
Result (REPORT.md 0.69.0): 19 Windows devices top out at Vulkan 1.2 with both extensions; the families from 2016 on among them are AMD Polaris
(Radeon Pro 555/555X/560/580), Vega (Radeon Pro Vega 20/56/64/64X, Vega II) and Navi (Radeon Pro 5300, W5700X) on Apple's Boot Camp driver 2.0.204;
so the app accepts Vulkan 1.2 with the two extensions.
