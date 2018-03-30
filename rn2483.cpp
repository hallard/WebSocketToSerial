
#include "WebSocketToSerial.h"

radio_state_e _radio_state;
bool 	radio_continuous_send = false;

void radioInit(uint32_t baudrate) {
	/*
  // Close proper
  SERIAL_DEVICE.flush(); 
  //SERIAL_DEVICE.end();

  // enable auto baud rate (see RN2483 datasheet)
  SERIAL_DEVICE.begin(baudrate);
  SERIAL_DEVICE.write((byte) 0x00);
  SERIAL_DEVICE.flush(); 
  SERIAL_DEVICE.write(0x55);
  SERIAL_DEVICE.flush(); 
  SERIAL_DEVICE.write(0x0A);
  SERIAL_DEVICE.write(0x0D);
  */
  _radio_state = RADIO_IDLE;
}

// Send a radio command
void radioExec( char * cmd) {
  // Set to blue immediatly
  LedRGBON(COLOR_BLUE, true);
  ws.textAll(cmd);
  execCommand(NULL, cmd);
	_radio_state = RADIO_WAIT_OK;
}

// Send a radio command
void radioExec( PGM_P cmd) {
	String str = cmd;	
	radioExec( str.c_str());
}

// Send data 
bool radioSend(uint32_t value) {

  if (_radio_state != RADIO_IDLE) 
    return false;

  // Set to BLUE immediatly
  LedRGBON(COLOR_BLUE, true);
  char cmd[32];
  sprintf_P(cmd, PSTR("radio tx %08X"), value);
  ws.textAll(cmd);
  execCommand(NULL, cmd);
  _radio_state = RADIO_WAIT_OK_SEND;
  return true;
}

// Put RN2483 into listening mode
void radioListen(void) {

	// Idle or previously listening ?
	if ( _radio_state==RADIO_IDLE || _radio_state==RADIO_LISTENING ) {
	  char cmd[32];

	  // Set receive Watchdog to 1H
	  //sprintf_P( cmd, PSTR("radio set wdt 3600000"));
	  //ws.textAll(cmd);
	  //execCommand(NULL, cmd);
	  //delay(250);

	  // Enable Receive mode
	  sprintf_P( cmd, PSTR("radio rx 0"));
	  ws.textAll(cmd);
	  execCommand(NULL, cmd);

	  // Wait ok listenning
	  _radio_state = RADIO_WAIT_OK_LISTENING;
	}
}

// we received a RN2483 serial response
bool radioResponse(String inputString) {
	// got OK
	if (inputString == "ok") {
		if (_radio_state == RADIO_WAIT_OK_LISTENING) {
	  	_radio_state = RADIO_LISTENING;
		} else if (_radio_state == RADIO_WAIT_OK_SEND) {
	  	_radio_state = RADIO_SENDING;
		} else {
	  	_radio_state = RADIO_IDLE;
		}

	// "radio_tx_ok"
	} else if (inputString == "radio_tx_ok") {
	  _radio_state = RADIO_IDLE;

	  // Set to GREEN immediatly
	  LedRGBON(COLOR_GREEN, true);

	// "radio_rx data"
	} else if (inputString.startsWith("radio_rx ")) {
		// Stop Animation for being sure to start a full one
	  LedRGBOFF();
	  // Set to GREEN immediatly
	  LedRGBON(COLOR_GREEN, true);

	  _radio_state = RADIO_RECEIVED_DATA;

	  // got something to do
	  return (true);

	// radio_err
	} else if (inputString == "radio_err") {
		// We were listening, restart listen (timed out)
		// not really an error
	  if ( _radio_state == RADIO_LISTENING ) {
      radioListen();
	  } else {
	  	_radio_state = RADIO_ERROR;
		  
		  // Set to RED immediatly
		  LedRGBON(COLOR_RED, true);
	  }
	}
	return false;
}

// Manage radio state machine
void radioManageState(btn_action_e btn) {
	static radio_state_e old_radio_state = _radio_state;

	// Action due to push button?
	if (btn != BTN_NONE) {
    if ( btn == BTN_QUICK_PRESS) {
      radioSend(millis()/1000);
    } else if ( btn == BTN_PRESSED_12) {
      LedRGBON(COLOR_CYAN, true);
      radioListen();
    } else if ( btn == BTN_PRESSED_23) {
 			if (!radio_continuous_send) {
	      LedRGBON(COLOR_MAGENTA, true);
 				radio_continuous_send = true;
 			} else {
	      LedRGBOFF();
 				radio_continuous_send = false;
 			}
    }
	}

  // Check radio state changed ?
  if (_radio_state != old_radio_state) {
    old_radio_state = _radio_state;

    // Set Breathing to cyan when listening
    if (_radio_state == RADIO_LISTENING ) {
      LedRGBON(COLOR_CYAN);

    } else if (_radio_state == RADIO_RECEIVED_DATA) {
    	_radio_state=RADIO_IDLE;
    	// Listen back
      radioListen();
    } else if (_radio_state == RADIO_SENDING) {
      LedRGBON(COLOR_RED, true);

    } else if (_radio_state == RADIO_IDLE) {
      //LedRGBOFF();
    }
  } // Radio state changed
}

// get radio state machine
radio_state_e radioState(void) {
  return _radio_state;
}