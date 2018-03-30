

#ifndef _PUSHBUTTON_H
#define _PUSHBUTTON_H

// Switch button
#define BTN_GPIO    14
#define BTN_DEBOUNCE_DELAY  100
#define BTN_LONG_PUSH_DELAY 1000

// Button pressed set pin port to LOW
#define BTN_PRESSED   LOW
#define BTN_RELEASED HIGH

// The button state machine when pressed
typedef enum {
  BTN_WAIT_PUSH,
  BTN_WAIT_RELEASE,
  BTN_WAIT_DEBOUNCE,
  BTN_WAIT_LONG_RELEASE
} 
btn_state_e;

// The actions possibe with different button press
typedef enum {
  BTN_NONE,        // do nothing.
  BTN_BAD_PRESS,   // button pressed lower than debounce time
  BTN_QUICK_PRESS, // button pressed with debounce OK
  BTN_PRESSED_12,  // pressed between 1 and 2 seconds
  BTN_PRESSED_23,  // pressed between 2 and 3 seconds 
  BTN_PRESSED_34,  // pressed between 3 and 4 seconds 
  BTN_PRESSED_45,  // pressed between 4 and 5 seconds 
  BTN_PRESSED_56,  // pressed between 5 and 6 seconds 
  BTN_PRESSED_67,  // pressed between 6 and 7 seconds 
  BTN_TIMEOUT      // Long press timeout
} 
btn_action_e;


// These variables manages button feature
extern btn_state_e   _btn_State;    // Button management state machine
extern btn_action_e  _btn_Action;   // button action after press
extern bool          _btn_LongPress;// indicate a long press on button
extern unsigned long _btn_StartTime;// when push started

// Exported functions
btn_state_e buttonManageState(uint8_t buttonLevel);

#endif