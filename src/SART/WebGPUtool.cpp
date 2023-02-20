#include "WebGPUtool.h"

#include <iostream>

#if __has_include("d3d12.h") || (_MSC_VER >= 1900)
#define DAWN_ENABLE_BACKEND_D3D12
#endif
#if __has_include("vulkan/vulkan.h") && (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64))
#define DAWN_ENABLE_BACKEND_VULKAN
#endif

//****************************************************************************/

//#include <dawn/webgpu.h>
//#include <dawn/webgpu_cpp.h>
#pragma comment(lib, "dawn_native.dll.lib")
#pragma comment(lib, "dawn_proc.dll.lib")
#ifdef DAWN_ENABLE_BACKEND_VULKAN
#pragma comment(lib, "vulkan-1.lib")
#endif

#include <GLFW/glfw3.h>



#ifdef __EMSCRIPTEN__
	// temporary dummy window handle
struct HandleImpl {} DUMMY;
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#else
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dawn/dawn_proc.h>
#include <dawn/native/NullBackend.h>
#ifdef DAWN_ENABLE_BACKEND_D3D12
#include <dawn/native/D3D12Backend.h>
#endif
#ifdef DAWN_ENABLE_BACKEND_VULKAN
#include <dawn/native/VulkanBackend.h>
#include <vulkan/vulkan_win32.h>
#endif
#endif


WGPUDevice _NULLABLE SARTwgpu_device = NULLPTR;
/*
 * Chosen backend type for \c #device.
 */
WGPUBackendType backend;

/*
 * Something needs to hold onto this since the address is passed to the WebGPU
 * native API, exposing the type-specific swap chain implementation. The struct
 * gets filled out on calling the respective XXX::CreateNativeSwapChainImpl(),
 * binding the WebGPU device and native window, then its raw pointer is passed
 * into WebGPU as a 64-bit int. The browser API doesn't have an equivalent
 * (since the swap chain is created from the canvas directly).
 *
 * Is the struct copied or does it need holding for the lifecycle of the swap
 * chain, i.e. can it just be a temporary?
 *
 * After calling wgpuSwapChainRelease() does it also call swapImpl::Destroy()
 * to delete the underlying NativeSwapChainImpl(), invalidating this struct?
 */
#ifndef __EMSCRIPTEN__
static DawnSwapChainImplementation swapImpl;

/**
 * Analogous to the browser's \c GPU.requestAdapter().
 * \n
 * The returned \c Adapter is a wrapper around the underlying Dawn adapter (and
 * owned by the single Dawn instance).
 *
 * \todo we might be interested in whether the \c AdapterType is discrete or integrated for power-management reasons
 *
 * \param[in] type1st first choice of \e backend type (e.g. \c WGPUBackendType_D3D12)
 * \param[in] type2nd optional fallback \e backend type (or \c WGPUBackendType_Null to pick the first choice or nothing)
 * \return the best choice adapter or an empty adapter wrapper
 */
static dawn_native::Adapter requestAdapter(WGPUBackendType type1st, WGPUBackendType type2nd = WGPUBackendType_Null)
{
	static dawn_native::Instance instance;
	instance.DiscoverDefaultAdapters();
	wgpu::AdapterProperties properties;
	std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
	for (auto it = adapters.begin(); it != adapters.end(); ++it)
	{
		it->GetProperties(&properties);
		if (static_cast<WGPUBackendType>(properties.backendType) == type1st) {
			return *it;
		}
	}
	if (type2nd)
	{
		for (auto it = adapters.begin(); it != adapters.end(); ++it)
		{
			it->GetProperties(&properties);
			if (static_cast<WGPUBackendType>(properties.backendType) == type2nd) {
				return *it;
			}
		}
	}
	return dawn::native::Adapter();
}

#endif // !Emsc

#ifdef DAWN_ENABLE_BACKEND_VULKAN
/**
 * Helper to obtain a Vulkan surface from the supplied window.
 *
 * \todo what's the lifecycle of this?
 *
 * \param[in] device WebGPU device
 * \param[in] window window on which the device will be bound
 * \return window surface (or \c VK_NULL_HANDLE if creation failed)
 */
static VkSurfaceKHR createVkSurface(WGPUDevice device, Handle window)
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
#ifdef WIN32
	VkWin32SurfaceCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hinstance = GetModuleHandle(NULL);
	info.hwnd = reinterpret_cast<HWND>(window);
	vkCreateWin32SurfaceKHR(
		dawn_native::vulkan::GetInstance(device),
		&info, nullptr, &surface);
#endif
	return surface;
}
#endif


void SARTprintError(WGPUErrorType /*type*/, const char* message, void*) {
	puts(message);
}

/**
 * Obtaining a WebGPU device based on the available system's backend.
 */
WGPUDevice SARTcreateDevice(WGPUBackendType type)
{
#ifdef __EMSCRIPTEN__
	wgpu_device = emscripten_webgpu_get_device();
	if (!wgpu_device) {
		return NULLPTR;
	}
	wgpuDeviceSetUncapturedErrorCallback(wgpu_device, print_wgpu_error, NULL);

	// Use C++ wrapper due to misbehavior in Emscripten.
	// Some offset computation for wgpuInstanceCreateSurface in JavaScript
	// seem to be inline with struct alignments in the C++ structure
	wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};
	html_surface_desc.selector = "#canvas";

	wgpu::SurfaceDescriptor surface_desc = {};
	surface_desc.nextInChain = &html_surface_desc;

	// Use 'null' instance
	wgpu::Instance instance = {};
	wgpu_surface = instance.CreateSurface(&surface_desc).Release();

#else
	if (type > WGPUBackendType_OpenGLES)
	{
#ifdef DAWN_ENABLE_BACKEND_VULKAN
		type = WGPUBackendType_Vulkan;
#else
#ifdef DAWN_ENABLE_BACKEND_D3D12
		type = WGPUBackendType_D3D12;
#endif
#endif
	}
	/*
	 * First go at this. We're only creating one global device/swap chain so far.
	 */
	SARTwgpu_device = NULL;
	if (dawn_native::Adapter adapter = requestAdapter(type))
	{
		wgpu::AdapterProperties properties;
		adapter.GetProperties(&properties);
		backend = static_cast<WGPUBackendType>(properties.backendType);

		//add somthings about limitations for binding size
		dawn::native::DawnDeviceDescriptor deviceDes = {};
		WGPUSupportedLimits suplimits = {};
		WGPURequiredLimits reqlimits = {};
		adapter.GetLimits(&suplimits);
		reqlimits.limits = suplimits.limits;
		deviceDes.requiredLimits = &reqlimits;

		SARTwgpu_device = adapter.CreateDevice(&deviceDes);
		DawnProcTable procs(dawn_native::GetProcs());
		procs.deviceSetUncapturedErrorCallback(SARTwgpu_device, SARTprintError, nullptr);
		dawnProcSetProcs(&procs);
	}

	if (SARTwgpu_device == NULL) {
		printf("GPU device generate fail!\n");
	}
#endif

	return SARTwgpu_device;
}


