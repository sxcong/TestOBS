// WindowsProject1.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "WindowsProject1.h"

#include <stdio.h>
#include <time.h>

#include <intrin.h>
#include <filesystem>


static const int cx = 640;
static const int cy = 480;


#include <graphics/graphics.h>
#include <d3d11.h>

#include "ScreenshotObj.hpp"

#pragma comment(lib, "lib/Debug/obs.lib")


void SetCurrentDirToExePath() {
	char exePath[MAX_PATH];
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
	SetCurrentDirectoryA(exeDir.string().c_str());
}

static void do_log(int log_level, const char* msg, va_list args, void* param)
{
	char bla[4096];
	vsnprintf(bla, 4095, msg, args);

	OutputDebugStringA(bla);
	OutputDebugStringA("\n");

	//if (log_level < LOG_WARNING)
	//	__debugbreak();

	UNUSED_PARAMETER(param);
}


obs_display_t* display = nullptr;
obs_source_t* desktop_source = nullptr;
obs_scene_t* main_scene = nullptr;
HWND main_hwnd = nullptr;


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:

		if (display) {
			obs_display_destroy(display);
			display = nullptr;
		}


		if (desktop_source) {
			obs_source_dec_showing(desktop_source);
			obs_source_release(desktop_source);
			desktop_source = nullptr;
		}
		if (main_scene) {
			obs_scene_release(main_scene);
			main_scene = nullptr;
		}
		
		obs_shutdown();//There may be an exception in the debug version, which is likely due to an issue with the compiled pthred32 and can be ignored. Or recompile it yourself
		PostQuitMessage(0);
		break;

	case WM_SIZE:
		if (display)
			obs_display_resize(display, LOWORD(lparam), HIWORD(lparam));
		break;

	case WM_PAINT:
		ValidateRect(hwnd, nullptr);
		break;


	case WM_KEYDOWN://Any key will trigger
		//if (wparam == VK_SPACE && (GetKeyState(VK_CONTROL) & 0x8000))
		{
			OBSBasic ob;
			ob.Screenshot(desktop_source);
		}
		break;

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	return 0;
}


bool InitializeOBS(HWND hwnd)
{

	base_set_log_handler(do_log, nullptr);
	obs_initialized();


	bool bInit = obs_startup("en-US", nullptr, nullptr);

	char exePath[MAX_PATH];


	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

	obs_add_module_path(exeDir.string().c_str(), nullptr);
	obs_load_all_modules(); //Some modules fail to load. You can delete the corresponding dll directly
	obs_post_load_modules();


	struct obs_video_info ovi;
	ovi.adapter = 0;
	ovi.base_width = 2560;
	ovi.base_height = 1440;
	ovi.output_width = 2560;
	ovi.output_height = 1440;
	ovi.fps_num = 30000;
	ovi.fps_den = 1001;

	ovi.graphics_module = "libobs-opengl.dll";// DL_OPENGL;

	ovi.output_format = VIDEO_FORMAT_BGRA;
	ovi.colorspace = VIDEO_CS_DEFAULT;
	ovi.range = VIDEO_RANGE_DEFAULT;
	ovi.scale_type = OBS_SCALE_BILINEAR;

	if (obs_reset_video(&ovi) != 0)
	{
		throw "Couldn't initialize video";
		return false;
	}


	RECT rc;
	GetClientRect(hwnd, &rc);

	gs_init_data info = {};
	info.cx = rc.right;
	info.cy = rc.bottom;
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = hwnd;

	display = obs_display_create(&info, 0);

	if (!display)
		return false;

	obs_data_t* settings = obs_data_create();
	obs_data_set_int(settings, "monitor", 0);//first screen
	desktop_source = obs_source_create("monitor_capture", "Desktop", settings, nullptr);
	obs_data_release(settings);

	if (!desktop_source)
		return false;

	blog(LOG_INFO, "Desktop source created: %s", obs_source_get_name(desktop_source));

	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//Without this sentence, the screen will be black
	obs_source_inc_showing(desktop_source);
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!


	main_scene = obs_scene_create("MainScene");
	obs_scene_add(main_scene, desktop_source);


	obs_source_t* scene_source = obs_scene_get_source(main_scene);


	obs_display_add_draw_callback(display, [](void* data, uint32_t cx, uint32_t cy) {
		obs_source_t* scene = (obs_source_t*)data;
		if (scene) {

			float src_width = (float)obs_source_get_width(scene);
			float src_height = (float)obs_source_get_height(scene);

			gs_viewport_push();
			gs_projection_push();
			const bool previous = gs_set_linear_srgb(true);

			gs_ortho(0.0f, float(src_width), 0.0f, float(src_height), -100.0f, 100.0f);
			gs_set_viewport(0, 0, cx, cy);
			obs_source_video_render(scene);

			gs_set_linear_srgb(previous);
			gs_projection_pop();
			gs_viewport_pop();
		}
		}, scene_source);

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	const wchar_t CLASS_NAME[] = L"OBSDesktopCaptureWindow";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	RegisterClass(&wc);

	SetCurrentDirToExePath();

	main_hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"OBS desktp capture",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		nullptr, nullptr, hInstance, nullptr
	);

	if (!main_hwnd)
		return 0;

	if (!InitializeOBS(main_hwnd))
	{
		MessageBox(main_hwnd, L"OBS init error", L"error", MB_ICONERROR);
		return 0;
	}

	ShowWindow(main_hwnd, nCmdShow);
	UpdateWindow(main_hwnd);

	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}