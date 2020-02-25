#include "libretro.h"
#include "risc.h"
#include "disk.h"
#include "pclink.h"
#include "raw-serial.h"
#include "sdl-ps2.h"

#include <stdlib.h>
#include <stdio.h>

static int clamp(int x, int min, int max) {
	if (x < min) return min;
	if (x > max) return max;
	return x;
}

#define CPU_HZ 25000000
#define FPS 30

static uint16_t FOR = 0x0000, AFT = 0xFFFF;

unsigned retro_api_version(void) {
	return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
	info->library_name     = "Oberon";
	info->library_version  = GIT_VERSION;
	info->valid_extensions = "dsk";
	info->need_fullpath    = true;
	info->block_extract    = false;
}


void dummy_log(enum retro_log_level level, const char *fmt, ...) { }

static retro_log_printf_t _log_cb;

static retro_environment_t   _environ_cb;
static retro_video_refresh_t _video_cb;
static retro_input_poll_t    _input_poll_cb;
static retro_input_state_t   _input_state_cb;

static struct RISC *_risc = NULL;
static struct RISC_SPI *_spi_disk = NULL;

static uint32_t _ms_counter;

static int _mouse_x, _mouse_y;

enum k_type {
	K_UNKNOWN = 0,
	K_NORMAL,
	K_EXTENDED,
	K_NUMLOCK_HACK,
	K_SHIFT_HACK,
};

struct k_info {
	uint8_t code;
	uint8_t type;
};

static struct k_info _keymap[RETROK_LAST];

static struct retro_framebuffer _framebuffer;

void _keyboard_cb(bool down, unsigned keycode,
                  uint32_t character, uint16_t key_modifiers)
{
	uint8_t buf[MAX_PS2_CODE_LEN];

	int i = 0;
	struct k_info info = _keymap[keycode];

	switch (info.type) {
	case K_UNKNOWN:
		printf("unknown Libretro keycode %d\n", keycode);
		break;

	case K_NORMAL:
		if (!down)
			buf[i++] = 0xf0;
		buf[i++] = info.code;
		break;

	case K_EXTENDED:
		buf[i++] = 0xe0;
		if (!down)
			buf[i++] = 0xf0;
		buf[i++] = info.code;
		break;

	case K_NUMLOCK_HACK:
		// This assumes Num Lock is always active
		if (down) {
	        // fake shift press
	        buf[i++] = 0xe0;
	        buf[i++] = 0x12;
	        buf[i++] = 0xe0;
	        buf[i++] = info.code;
		} else {
	        buf[i++] = 0xe0;
	        buf[i++] = 0xf0;
	        buf[i++] = info.code;
		// fake shift release
		buf[i++] = 0xe0;
		buf[i++] = 0xf0;
			buf[i++] = 0x12;
      	}
		break;

	case K_SHIFT_HACK:
		if (down) {
	        // fake shift release
			if (key_modifiers & RETROKMOD_SHIFT) {
				buf[i++] = 0xe0;
				buf[i++] = 0xf0;
				buf[i++] = 0x12;
		}
		buf[i++] = 0xe0;
			buf[i++] = info.code;
		} else {
			buf[i++] = 0xe0;
			buf[i++] = 0xf0;
			buf[i++] = info.code;
			// fake shift press
			if (key_modifiers & RETROKMOD_SHIFT) {
				buf[i++] = 0xE0;
				buf[i++] = 0x59;
			}
		}
		break;
	}

	risc_keyboard_input(_risc, buf, i);
}

static const struct retro_controller_description ports_keyboard[] =
{
	{ "Keyboard + Mouse", RETRO_DEVICE_KEYBOARD },
	{ 0 },
};
static const struct retro_controller_info ports[] = {
        { ports_keyboard, 1 },
        { 0 },
};

void retro_set_environment(retro_environment_t cb) {
	_environ_cb = cb;
	_environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
	_video_cb = cb; }

void retro_set_input_poll(retro_input_poll_t cb) {
	_input_poll_cb = cb; }

void retro_set_input_state(retro_input_state_t cb) {
	_input_state_cb = cb; }

void retro_set_audio_sample(retro_audio_sample_t cb) { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { }

void retro_init(void)
{
	_risc = risc_new();
	risc_set_serial(_risc, &pclink);

	struct retro_log_callback log_callback;
	_log_cb = _environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_callback)
		? log_callback.log
		: dummy_log;
}

void retro_deinit(void)
{
	if (_risc) {
		free(_risc);
		_risc = NULL;
	}
}

void retro_set_controller_port_device(unsigned port, unsigned device) { }

void retro_reset(void) {
	_ms_counter = 1;
	risc_reset(_risc);
}

size_t retro_serialize_size(void) { return 0; }

bool retro_serialize(        void *data, size_t size) { return false; }
bool retro_unserialize(const void *data, size_t size) { return false; }

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { }

/* Loads a game. */
bool retro_load_game(const struct retro_game_info *game)
{
	struct retro_keyboard_callback callback = { _keyboard_cb };
	if (!_environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &callback)) {
		_log_cb(RETRO_LOG_ERROR, "core requires keyboard callback support\n");
		return false;
	}

	if (game->path)
		_spi_disk = disk_new(game->path);
	if (!_spi_disk) {
		_log_cb(RETRO_LOG_ERROR, "failed to load disk image\n");
		return false;
	}

	risc_set_spi(_risc, 1, _spi_disk);
	risc_set_serial(_risc, raw_serial_new("/dev/null", "/dev/null"));

	enum retro_pixel_format pf = RETRO_PIXEL_FORMAT_RGB565;
	_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pf);
	/* if this fails then its still 0RGB1555 */

	if (!_environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &_framebuffer)) {
		_framebuffer.width  = RISC_FRAMEBUFFER_WIDTH;
		_framebuffer.height = RISC_FRAMEBUFFER_HEIGHT;
	}

	_framebuffer.width = _framebuffer.width & ~31U;

	if (_framebuffer.width < 32)
		_framebuffer.width = 32;

	if (_framebuffer.height < 32)
		_framebuffer.height = 32;

	_framebuffer.data = malloc(
		_framebuffer.width *
		_framebuffer.height *
		sizeof(uint16_t));

	risc_configure_memory(_risc, 1, _framebuffer.width, _framebuffer.height);

	_ms_counter = 1;
	_mouse_x = 0;
	_mouse_y = _framebuffer.height;

	return true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->geometry.base_width     =
	info->geometry.max_width      = _framebuffer.width;
	info->geometry.base_height    =
	info->geometry.max_height     = _framebuffer.height;
	info->geometry.aspect_ratio   = 0.0;
	info->timing.fps              = FPS;
	info->timing.sample_rate      = 0;
}

bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
) { return false; }

/* Unloads a currently loaded game. */
void retro_unload_game(void)
{
	if (!_risc && _spi_disk) {
		free(_spi_disk);
		_spi_disk = NULL;
	}
}

unsigned retro_get_region(void) { return ~0U; }

void  *retro_get_memory_data(unsigned id) { return NULL; }
size_t retro_get_memory_size(unsigned id) { return 0; }

void retro_run(void)
{
	_input_poll_cb();

	int mx = _mouse_x + _input_state_cb(
		0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
	int my = _mouse_y - _input_state_cb(
		0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

	_mouse_x = clamp(mx, 0, _framebuffer.width);
	_mouse_y = clamp(my, 0, _framebuffer.height);
	risc_mouse_moved(_risc, _mouse_x, _mouse_y);

	risc_mouse_button(_risc, 1,
		_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT));
	risc_mouse_button(_risc, 2,
		_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE));
	risc_mouse_button(_risc, 3,
		_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT));

	risc_set_time(_risc, _ms_counter);
	_ms_counter += 1000 / FPS;
	risc_run(_risc, CPU_HZ / FPS);

 	struct Damage damage = risc_get_framebuffer_damage(_risc);
	if (damage.y1 <= damage.y2) {

		uint32_t *in = risc_get_framebuffer_ptr(_risc);
		uint16_t *out = _framebuffer.data;
		uint16_t colors[] = { AFT, FOR };

		for (int line = damage.y2; line >= damage.y1; line--) {
			int in_line = line * (_framebuffer.width / 32);

			int out_idx = ((_framebuffer.height-line-1) * _framebuffer.width)
			            + damage.x1 * 32;

			for (int col = damage.x1; col <= damage.x2; col++) {
				uint32_t chunk = in[in_line + col];
				for (int b = 0; b < 32; b++) {
					out[out_idx] = colors[chunk&1];
					chunk >>= 1;
					out_idx++;
				}
			}
		}
	}

	_video_cb(
		_framebuffer.data,
		_framebuffer.width,
		_framebuffer.height,
		_framebuffer.width << 1);
}

static struct k_info _keymap[RETROK_LAST] = {
	[ RETROK_BACKSPACE    ] = { 0x66, K_NORMAL },
	[ RETROK_TAB          ] = { 0x0d, K_NORMAL },
	[ RETROK_RETURN       ] = { 0x5a, K_NORMAL },
	[ RETROK_QUOTE        ] = { 0x52, K_NORMAL },
	[ RETROK_ESCAPE       ] = { 0x76, K_NORMAL },
	[ RETROK_SPACE        ] = { 0x29, K_NORMAL },
	[ RETROK_COMMA        ] = { 0x41, K_NORMAL },
	[ RETROK_MINUS        ] = { 0x4e, K_NORMAL },
	[ RETROK_PERIOD       ] = { 0x49, K_NORMAL },
	[ RETROK_SLASH        ] = { 0x4a, K_NORMAL },
	[ RETROK_0            ] = { 0x45, K_NORMAL },
	[ RETROK_1            ] = { 0x16, K_NORMAL },
	[ RETROK_2            ] = { 0x1e, K_NORMAL },
	[ RETROK_3            ] = { 0x26, K_NORMAL },
	[ RETROK_4            ] = { 0x25, K_NORMAL },
	[ RETROK_5            ] = { 0x2e, K_NORMAL },
	[ RETROK_6            ] = { 0x36, K_NORMAL },
	[ RETROK_7            ] = { 0x3d, K_NORMAL },
	[ RETROK_8            ] = { 0x3e, K_NORMAL },
	[ RETROK_9            ] = { 0x46, K_NORMAL },
	[ RETROK_SEMICOLON    ] = { 0x4c, K_NORMAL },
	[ RETROK_EQUALS       ] = { 0x55, K_NORMAL },
	[ RETROK_LEFTBRACKET  ] = { 0x54, K_NORMAL },
	[ RETROK_BACKSLASH    ] = { 0x5d, K_NORMAL },
	[ RETROK_RIGHTBRACKET ] = { 0x5b, K_NORMAL },
	[ RETROK_BACKQUOTE    ] = { 0x0e, K_NORMAL },
	[ RETROK_a            ] = { 0x1c, K_NORMAL },
	[ RETROK_b            ] = { 0x32, K_NORMAL },
	[ RETROK_c            ] = { 0x21, K_NORMAL },
	[ RETROK_d            ] = { 0x23, K_NORMAL },
	[ RETROK_e            ] = { 0x24, K_NORMAL },
	[ RETROK_f            ] = { 0x2b, K_NORMAL },
	[ RETROK_g            ] = { 0x34, K_NORMAL },
	[ RETROK_h            ] = { 0x33, K_NORMAL },
	[ RETROK_i            ] = { 0x43, K_NORMAL },
	[ RETROK_j            ] = { 0x3b, K_NORMAL },
	[ RETROK_k            ] = { 0x42, K_NORMAL },
	[ RETROK_l            ] = { 0x4b, K_NORMAL },
	[ RETROK_m            ] = { 0x3a, K_NORMAL },
	[ RETROK_n            ] = { 0x31, K_NORMAL },
	[ RETROK_o            ] = { 0x44, K_NORMAL },
	[ RETROK_p            ] = { 0x4d, K_NORMAL },
	[ RETROK_q            ] = { 0x15, K_NORMAL },
	[ RETROK_r            ] = { 0x2d, K_NORMAL },
	[ RETROK_s            ] = { 0x1b, K_NORMAL },
	[ RETROK_t            ] = { 0x2c, K_NORMAL },
	[ RETROK_u            ] = { 0x3c, K_NORMAL },
	[ RETROK_v            ] = { 0x2a, K_NORMAL },
	[ RETROK_w            ] = { 0x1d, K_NORMAL },
	[ RETROK_x            ] = { 0x22, K_NORMAL },
	[ RETROK_y            ] = { 0x35, K_NORMAL },
	[ RETROK_z            ] = { 0x1a, K_NORMAL },
	[ RETROK_LEFTBRACE    ] = { 123, K_NORMAL },
	[ RETROK_BAR          ] = { 124, K_NORMAL },
	[ RETROK_RIGHTBRACE   ] = { 125, K_NORMAL },
	[ RETROK_TILDE        ] = { 126, K_NORMAL },
	[ RETROK_DELETE       ] = { 127, K_NORMAL },

	[ RETROK_KP0          ] = { 0x70, K_NORMAL },
	[ RETROK_KP1          ] = { 0x69, K_NORMAL },
	[ RETROK_KP2          ] = { 0x72, K_NORMAL },
	[ RETROK_KP3          ] = { 0x7a, K_NORMAL },
	[ RETROK_KP4          ] = { 0x6b, K_NORMAL },
	[ RETROK_KP5          ] = { 0x73, K_NORMAL },
	[ RETROK_KP6          ] = { 0x74, K_NORMAL },
	[ RETROK_KP7          ] = { 0x6c, K_NORMAL },
	[ RETROK_KP8          ] = { 0x75, K_NORMAL },
	[ RETROK_KP9          ] = { 0x7d, K_NORMAL },
	[ RETROK_KP_PERIOD    ] = { 0x71, K_NORMAL },
	[ RETROK_KP_DIVIDE    ] = { 0x4a, K_SHIFT_HACK },
	[ RETROK_KP_MULTIPLY  ] = { 0x7c, K_NORMAL },
	[ RETROK_KP_MINUS     ] = { 0x7b, K_NORMAL },
	[ RETROK_KP_PLUS      ] = { 0x79, K_NORMAL },
	[ RETROK_KP_ENTER     ] = { 0x5A, K_EXTENDED },

	[ RETROK_UP           ] = { 0x75, K_NUMLOCK_HACK },
	[ RETROK_DOWN         ] = { 0x72, K_NUMLOCK_HACK },
	[ RETROK_RIGHT        ] = { 0x74, K_NUMLOCK_HACK },
	[ RETROK_LEFT         ] = { 0x6b, K_NUMLOCK_HACK },
	[ RETROK_INSERT       ] = { 0x70, K_NUMLOCK_HACK },
	[ RETROK_HOME         ] = { 0x6c, K_NUMLOCK_HACK },
	[ RETROK_END          ] = { 0x69, K_NUMLOCK_HACK },
	[ RETROK_PAGEUP       ] = { 0x7d, K_NUMLOCK_HACK },
	[ RETROK_PAGEDOWN     ] = { 0x7a, K_NUMLOCK_HACK },

	[ RETROK_F1           ] = { 0x05, K_NORMAL },
	[ RETROK_F2           ] = { 0x06, K_NORMAL },
	[ RETROK_F3           ] = { 0x04, K_NORMAL },
	[ RETROK_F4           ] = { 0x0c, K_NORMAL },
	[ RETROK_F5           ] = { 0x03, K_NORMAL },
	[ RETROK_F6           ] = { 0x0b, K_NORMAL },
	[ RETROK_F7           ] = { 0x83, K_NORMAL },
	[ RETROK_F8           ] = { 0x0a, K_NORMAL },
	[ RETROK_F9           ] = { 0x01, K_NORMAL },
	[ RETROK_F10          ] = { 0x09, K_NORMAL },
	[ RETROK_F11          ] = { 0x78, K_NORMAL },
	[ RETROK_F12          ] = { 0x07, K_NORMAL },


	[ RETROK_RSHIFT       ] = { 0x59, K_EXTENDED },
	[ RETROK_LSHIFT       ] = { 0x12, K_NORMAL },
	[ RETROK_RCTRL        ] = { 0x14, K_EXTENDED },
	[ RETROK_LCTRL        ] = { 0x14, K_NORMAL },
	[ RETROK_RALT         ] = { 0x11, K_EXTENDED },
	[ RETROK_LALT         ] = { 0x11, K_NORMAL },

	[ RETROK_MENU         ] = { 0x27, K_EXTENDED },
};
