"""Does this PC have a graphics card the game can run on? Asked before the build (tools/setup.py, step check).

The game needs what src/vk/vk.c asks for: Vulkan 1.3 with VK_EXT_shader_object, VK_EXT_external_memory_host and
VK_KHR_swapchain. A Radeon RX 570 (Polaris, Windows) builds for 40 minutes and then stops at start with "Vulkan 1.3
with VK_EXT_shader_object is required" (2026-10-29): AMD's Windows driver for Polaris and Vega has no shader objects,
nor has Intel's for HD 6xx (#17). Read through the Vulkan loader (vulkan-1.dll) with ctypes: nothing to install.

usage: python tools/gpu_check.py   (prints each graphics card and what it lacks; exit code 1 when none will do)
"""
import ctypes
import sys

NEEDED = ('VK_EXT_shader_object', 'VK_EXT_external_memory_host', 'VK_KHR_swapchain')
API_1_3 = (1 << 22) | (3 << 12)
TYPES = {1: 'integrated', 2: 'discrete', 3: 'virtual', 4: 'CPU'}


class AppInfo(ctypes.Structure):
    _fields_ = [('sType', ctypes.c_int), ('pNext', ctypes.c_void_p), ('pApplicationName', ctypes.c_char_p),
                ('applicationVersion', ctypes.c_uint32), ('pEngineName', ctypes.c_char_p), ('engineVersion', ctypes.c_uint32),
                ('apiVersion', ctypes.c_uint32)]


class InstanceInfo(ctypes.Structure):
    _fields_ = [('sType', ctypes.c_int), ('pNext', ctypes.c_void_p), ('flags', ctypes.c_uint32),
                ('pApplicationInfo', ctypes.POINTER(AppInfo)), ('enabledLayerCount', ctypes.c_uint32),
                ('ppEnabledLayerNames', ctypes.c_void_p), ('enabledExtensionCount', ctypes.c_uint32),
                ('ppEnabledExtensionNames', ctypes.c_void_p)]


class ExtensionProperties(ctypes.Structure):
    _fields_ = [('extensionName', ctypes.c_char * 256), ('specVersion', ctypes.c_uint32)]


def gpus():
    """[(name, type, 'major.minor.patch', missing requirements)] of every graphics card the Vulkan loader reports;
    None when there is no Vulkan at all."""
    try:
        vk = ctypes.WinDLL('vulkan-1.dll')
    except OSError:
        return None
    app = AppInfo(0, None, b'UltraTears setup', 0, None, 0, API_1_3)   # VK_STRUCTURE_TYPE_APPLICATION_INFO
    ci = InstanceInfo(1, None, 0, ctypes.pointer(app), 0, None, 0, None)   # VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    inst = ctypes.c_void_p()
    if vk.vkCreateInstance(ctypes.byref(ci), None, ctypes.byref(inst)) != 0:
        return None
    try:
        n = ctypes.c_uint32()
        vk.vkEnumeratePhysicalDevices(inst, ctypes.byref(n), None)
        devs = (ctypes.c_void_p * n.value)()
        vk.vkEnumeratePhysicalDevices(inst, ctypes.byref(n), devs)
        out = []
        for d in devs[:n.value]:
            props = ctypes.create_string_buffer(1024)   # VkPhysicalDeviceProperties (824 bytes): apiVersion at 0,
            vk.vkGetPhysicalDeviceProperties(ctypes.c_void_p(d), props)   # deviceType at 16, deviceName at 20
            api = int.from_bytes(props.raw[0:4], 'little')
            kind = int.from_bytes(props.raw[16:20], 'little')
            name = props.raw[20:276].split(b'\0', 1)[0].decode(errors='replace')
            m = ctypes.c_uint32()
            vk.vkEnumerateDeviceExtensionProperties(ctypes.c_void_p(d), None, ctypes.byref(m), None)
            exts = (ExtensionProperties * m.value)()
            vk.vkEnumerateDeviceExtensionProperties(ctypes.c_void_p(d), None, ctypes.byref(m), exts)
            have = {e.extensionName.decode(errors='replace') for e in exts[:m.value]}
            missing = (['Vulkan 1.3'] if api < API_1_3 else []) + [x for x in NEEDED if x not in have]
            out.append((name, TYPES.get(kind, 'other'), f'{api >> 22}.{(api >> 12) & 0x3ff}.{api & 0xfff}', missing))
        return out
    finally:
        vk.vkDestroyInstance(inst, None)


def check():
    """(ok, text): whether a card will do, and what to tell the player."""
    found = gpus()
    if found is None:
        return False, ('no Vulkan driver was found (vulkan-1.dll): install the current driver for your graphics card '
                       '(NVIDIA, AMD or Intel)')
    lines = [f'{name} ({kind}, Vulkan {api})' + (': lacks ' + ', '.join(missing) if missing else ': supported')
             for name, kind, api, missing in found if kind != 'CPU']
    ok = any(not missing for _, kind, _, missing in found if kind != 'CPU')
    if ok:
        return True, '; '.join(l for l in lines if l.endswith('supported'))
    return False, ('no graphics card here can run the game: ' + '; '.join(lines or ['none reported']) +
                   '. It needs Vulkan 1.3 with VK_EXT_shader_object: a current driver for NVIDIA GTX 900 or newer, AMD '
                   'RX 5000 or newer, or Intel Arc / Iris Xe. AMD RX 400/500 and Vega and Intel HD 600 have no shader '
                   'objects in their Windows drivers. Updating the driver may help; otherwise this PC is not supported '
                   'yet (ULTRS_SKIP_GPU_CHECK=1 builds anyway)')


if __name__ == '__main__':
    ok, text = check()
    print(text)
    sys.exit(0 if ok else 1)
