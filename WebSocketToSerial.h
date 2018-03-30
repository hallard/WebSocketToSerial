
#ifndef _WEBSOCKETTOSERIAL_H
#define _WEBSOCKETTOSERIAL_H

#include <arduino.h>
#include <ESPAsyncWebServer.h>
#include <SoftwareSerial.h>

#include "pushbutton.h"
#include "rn2483.h"

// Define here the target serial you connected to this app
// -------------------------------------------------------
// Classic terminal connected to RX/TX of ESP8266 
// using Arduino IDE or other terminal software
#define MOD_TERMINAL

// LoraWan Microchip RN2483, see hardware here
// https://github.com/hallard/WeMos-RN2483
//#define MOD_RN2483

// Uncomment 3 lines below if you have an WS1812B RGB LED 
// like shield here https://github.com/hallard/WeMos-RN2483
#ifdef MOD_RN2483
#define SERIAL_DEVICE Serial
#define SERIAL_DEBUG  Serial1
#define RGB_LED_PIN   0
#define RGB_LED_COUNT 1
#define RGBW_LED 
#endif

// Uncomment 3 lines below if you have an WS1812B RGB LED 
// like shield here https://github.com/hallard/WeMos-RN2483
#ifdef MOD_TERMINAL
#define SERIAL_DEVICE Serial
#define SERIAL_DEBUG  Serial1
#endif

#ifdef RGB_LED_PIN
  #include <NeoPixelAnimator.h>
  #include <NeoPixelBus.h>

  #ifdef RGBW_LED
		typedef NeoPixelBus<NeoGrbwFeature, NeoEsp8266BitBang800KbpsMethod> MyPixelBus;

    // what is stored for state is specific to the need, in this case, the colors.
    // basically what ever you need inside the animation update function
    struct MyAnimationState {
      RgbwColor RgbStartingColor;
      RgbwColor RgbEndingColor;
      uint8_t   IndexPixel;   // general purpose variable used to store pixel index
    };
	#else
		typedef NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod> MyPixelBus;

    // what is stored for state is specific to the need, in this case, the colors.
    // basically what ever you need inside the animation update function
    struct MyAnimationState  {
      RgbColor  RgbStartingColor;
      RgbColor  RgbEndingColor;
      uint8_t   IndexPixel;   // general purpose variable used to store pixel index
    };
	#endif 
#endif

// The RGB animation state machine 
typedef enum {
	RGB_ANIM_NONE,
	RGB_ANIM_FADE_IN,
	RGB_ANIM_FADE_OUT,
	RGB_ANIM_BLINK_ON,
	RGB_ANIM_BLINK_OFF,
} 
rgb_anim_state_e;

// value for HSL color
// see http://www.workwithcolor.com/blue-color-hue-range-01.htm
#define COLOR_RED             0
#define COLOR_ORANGE         30
#define COLOR_ORANGE_YELLOW  45
#define COLOR_YELLOW         60
#define COLOR_YELLOW_GREEN   90
#define COLOR_GREEN         120
#define COLOR_GREEN_CYAN    165
#define COLOR_CYAN          180
#define COLOR_CYAN_BLUE     210
#define COLOR_BLUE          240
#define COLOR_BLUE_MAGENTA  275
#define COLOR_MAGENTA       300
#define COLOR_PINK          350


// Maximum number of simultaned clients connected (WebSocket)
#define MAX_WS_CLIENT 5

// Client state machine
#define CLIENT_NONE     0
#define CLIENT_ACTIVE   1

#define HELP_TEXT "[[b;green;]WebSocket2Serial HELP]\n" \
                  "---------------------\n" \
                  "[[b;cyan;]?] or [[b;cyan;]help] show this help\n" \
                  "[[b;cyan;]swap]      swap serial UART pin to GPIO15/GPIO13\n" \
                  "[[b;cyan;]ping]      send ping command\n" \
                  "[[b;cyan;]heap]      show free RAM\n" \
                  "[[b;cyan;]whoami]    show client # we are\n" \
                  "[[b;cyan;]who]       show all clients connected\n" \
                  "[[b;cyan;]fw]        show firmware date/time\n"  \
                  "[[b;cyan;]baud]      show current serial baud rate\n" \
                  "[[b;cyan;]baud n]    set serial baud rate to n\n" \
                  "[[b;cyan;]rgb l]     set RGB Led luminosity l (0..100)\n" \
                  "[[b;cyan;]reset p]   do a reset pulse on gpio pin number p\n" \
                  "[[b;cyan;]hostname]  show network hostname of device\n" \
                  "[[b;cyan;]ls]        list SPIFFS files\n" \
                  "[[b;cyan;]debug]     show debug information\n" \
                  "[[b;cyan;]cat file]  display content of file\n"  \
                  "[[b;cyan;]read file] send SPIFFS file to serial (read)" 

// Web Socket client state
typedef struct {
  uint32_t  id;
  uint8_t   state;
} _ws_client; 


// Exported vars
extern AsyncWebSocket ws;

// RGB Led exported vars
#ifdef RGB_LED_PIN
  #ifdef RGBW_LED
    extern RgbwColor rgb_led_color; // RGBW Led color
	#else
    extern RgbColor rgb_led_color; // RGB Led color
	#endif 

	extern MyPixelBus rgb_led;
  extern uint8_t  rgb_led_effect_state;  
  extern MyAnimationState animationState[];
#endif

#ifdef MOD_RN2483
  extern SoftwareSerial SoftSer;
#endif


// Exported function
#ifdef RGB_LED_PIN
void LedRGBFadeAnimUpdate(const AnimationParam& param);
#else
void LedRGBFadeAnimUpdate(void * param);
#endif

void LedRGBAnimate(uint16_t param);
void LedRGBON (uint16_t hue, bool now=false);
void LedRGBOFF(void);

void execCommand(AsyncWebSocketClient * client, char * msg) ;

#endif



