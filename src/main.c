
#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0xD1, 0xAD, 0x93, 0x6B, 0x17, 0x68, 0x46, 0x0E, 0x88, 0x5A, 0xD7, 0x96, 0x89, 0x6A, 0x38, 0xC2 }
PBL_APP_INFO(MY_UUID, "Ring My Phone", "Dark Rock Studios", 1 , 0/* App version */, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_STANDARD_APP);

const int STATUS_RESET_TIME = 5000;
const int STATUS_COOKIE = 1;

Window window;

TextLayer statusTextLayer;

AppContextRef appContext;
ActionBarLayer actionBar;
HeapBitmap bitmapRing;
HeapBitmap bitmapSilence;

AppTimerHandle statusResetTimer = APP_TIMER_INVALID_HANDLE;

static bool callbacks_registered = false;
static AppMessageCallbacksNode app_callbacks;

const char * const WINDOW_TITLE = "Ring My Phone";
const char * const STATUS_READY = "Ready";
const char * const STATUS_DONE = "Done";
const char * const STATUS_RINGING = "Ringing";
const char * const STATUS_SILENCING = "Silencing";
const char * const STATUS_FAILED = "Failed :(";

enum {
  CMD_KEY = 0x0, // TUPLE_INTEGER
};

enum {
  CMD_START = 0x01,
  CMD_STOP = 0x02,
};

static void send_cmd(uint8_t cmd);
void start_reset_timer();

// Modify these common button handlers

void up_single_click_handler(ClickRecognizerRef recognizer, Window *window)
{
  (void)recognizer;
  (void)window;

	text_layer_set_text(&statusTextLayer, STATUS_RINGING);
	send_cmd( CMD_START );
}


void down_single_click_handler(ClickRecognizerRef recognizer, Window *window)
{
  (void)recognizer;
  (void)window;

	text_layer_set_text(&statusTextLayer, STATUS_SILENCING);	
	send_cmd( CMD_STOP );
}

// App message callbacks
static void app_send_failed(DictionaryIterator* failed, AppMessageResult reason, void* context)
{
	text_layer_set_text(&statusTextLayer, STATUS_FAILED);
	start_reset_timer();
}

static void app_received_msg(DictionaryIterator* received, void* context)
{
	vibes_short_pulse();
	text_layer_set_text(&statusTextLayer, STATUS_DONE);
	start_reset_timer();
}

bool register_callbacks()
{
	if (callbacks_registered) {
		if (app_message_deregister_callbacks(&app_callbacks) == APP_MSG_OK)
			callbacks_registered = false;
	}
	if (!callbacks_registered) {
		app_callbacks = (AppMessageCallbacksNode){
			.callbacks = {
				.out_failed = app_send_failed,
        .in_received = app_received_msg
			},
			.context = NULL
		};
		if (app_message_register_callbacks(&app_callbacks) == APP_MSG_OK) {
      callbacks_registered = true;
    }
	}
	return callbacks_registered;
}

void cancel_reset_timer()
{
	if (statusResetTimer != APP_TIMER_INVALID_HANDLE)
	{
		if (app_timer_cancel_event(appContext, statusResetTimer))
		{
			statusResetTimer = APP_TIMER_INVALID_HANDLE;
		}
	}
}

void start_reset_timer()
{
	cancel_reset_timer();
	statusResetTimer = app_timer_send_event(appContext, STATUS_RESET_TIME, STATUS_COOKIE);
}

static void send_cmd(uint8_t cmd)
{
	start_reset_timer();
		
	Tuplet value = TupletInteger(CMD_KEY, cmd);
	
	DictionaryIterator *iter;
	app_message_out_get(&iter);
	
	if (iter == NULL)
		return;
	
	dict_write_tuplet(iter, &value);
	dict_write_end(iter);
	
	app_message_out_send();
	app_message_out_release();
}

void handle_timer(AppContextRef app_ctx, AppTimerHandle handle, uint32_t cookie)
{
    (void) app_ctx;
    (void) handle;
    
	text_layer_set_text(&statusTextLayer, STATUS_READY);
}

// Standard app initialisation

void click_config_provider(ClickConfig **config, void *context)
{
  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
}

void window_load(Window *window)
{
	// Initialize the action bar:
	action_bar_layer_init(&actionBar);
	// Associate the action bar with the window:
	action_bar_layer_add_to_window(&actionBar, window);
	// Set the click config provider:
	action_bar_layer_set_click_config_provider(&actionBar,
											   click_config_provider);
	// Set the icons:
	heap_bitmap_init( &bitmapRing, RESOURCE_ID_IMAGE_RING_ICON );
	heap_bitmap_init( &bitmapSilence, RESOURCE_ID_IMAGE_SILENCE_ICON );
	
	action_bar_layer_set_icon(&actionBar, BUTTON_ID_UP, &bitmapRing.bmp);
	action_bar_layer_set_icon(&actionBar, BUTTON_ID_DOWN, &bitmapSilence.bmp);
}

void window_unload(Window *window)
{
	heap_bitmap_deinit( &bitmapRing );
	heap_bitmap_deinit( &bitmapSilence );
}

void handle_init(AppContextRef ctx)
{
  (void)ctx;

	appContext = ctx;
	resource_init_current_app(&APP_RESOURCES);
	
	window_init(&window, WINDOW_TITLE);
	window_stack_push(&window, true /* Animated */);
	
	int16_t aThird = window.layer.frame.size.h / 3;	
	
	GRect textFrame;
	textFrame.origin.x = 0;
	textFrame.origin.y = 0;
	textFrame.size.w = window.layer.frame.size.w;
	textFrame.size.h = aThird;
	
	textFrame.origin.y = aThird;
	text_layer_init(&statusTextLayer, textFrame);
	text_layer_set_text(&statusTextLayer, STATUS_READY);
	text_layer_set_text_alignment(&statusTextLayer, GTextAlignmentLeft);
	text_layer_set_font(&statusTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	layer_add_child(&window.layer, &statusTextLayer.layer);
	
	register_callbacks();
	
	WindowHandlers handlers = {
		.load = window_load,
		.unload = window_unload
	};
	
	window_set_window_handlers(&window, handlers);
}

void handle_deinit(AppContextRef app_ctx)
{
	cancel_reset_timer();
	appContext = NULL;
}

void pbl_main(void *params)
{
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
	.deinit_handler = &handle_deinit,
	.timer_handler = &handle_timer,
	.messaging_info = {
      .buffer_sizes = {
        .inbound = 256,
        .outbound = 256,
      }
    }
  };
  app_event_loop(params, &handlers);
}
