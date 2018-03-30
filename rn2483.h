

#ifndef _RN2483_H
#define _RN2483_H

#define RN2483_RESET_PIN 15

// The radio state machine when talking with RN2483
typedef enum {
  RADIO_IDLE,
  RADIO_SENDING,
  RADIO_LISTENING,
  RADIO_RECEIVED_DATA,
  RADIO_WAIT_OK,					 /* just Wait Ok  */
  RADIO_WAIT_OK_SEND, 		 /* Wait Ok and after we sent a packet */
  RADIO_WAIT_OK_LISTENING,
  RADIO_ERROR
} 
radio_state_e;

// The application state machine 
typedef enum {
  APP_IDLE,
  APP_CONTINUOUS_LISTEN,
  APP_CONTINUOUS_SEND,
} 
app_state_e;


// exported vars
extern app_state_e app_state ;

// exported functions
void radioExec( char * cmd);
void radioExec( PGM_P cmd);
void radioInit(uint32_t baudrate);
void radioIdle(void);
bool radioSend(uint32_t value);
void radioListen(void) ;
bool radioResponse(String inputString) ;
void radioManageState(btn_action_e btn) ;
radio_state_e radioState(void);

#endif