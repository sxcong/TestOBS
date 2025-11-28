Compile libobs yourself and call obs.lib in win32 program to record screen and capture pictures.
cmake+vs2022

1 monitor_capture key code:

base_set_log_handler(do_log, nullptr);
obs_initialized
obs_startup
obs_add_module_path
obs_load_all_modules
obs_post_load_modules
obs_reset_video
obs_display_create
obs_source_create

	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//Without this sentence, the screen will be black
	obs_source_inc_showing(desktop_source);
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!

obs_scene_create
obs_display_add_draw_callback

2 save image key code:
OBSBasic ob;
ob.Screenshot(desktop_source);

This is the code in OBS Studio, Remove Qt related content.
