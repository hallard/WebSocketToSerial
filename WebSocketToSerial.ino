#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <SoftwareSerial.h>

// Main project include file
#include "WebSocketToSerial.h"

// RGB Led
#ifdef RGB_LED_PIN

  #ifdef RGBW_LED
    MyPixelBus rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
    RgbwColor rgb_led_color; // RGBW Led color
  #else
    MyPixelBus rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
    RgbColor rgb_led_color; // RGB Led color
  #endif

  // general purpose variable used to store effect state
  rgb_anim_state_e  rgb_anim_state;  
  uint8_t rgb_luminosity = 20 ; // Luminosity from 0 to 100%

  // one entry per pixel to match the animation timing manager
  NeoPixelAnimator animations(1); 
  MyAnimationState animationState[1];

#endif

#ifdef MOD_RN2483
  // If you want to use soft serial, not super reliable with AsyncTCP
  //SoftwareSerial SoftSer(12, 13, false, 128);
  app_state_e app_state ;
#endif

const char* ssid = "*******";
const char* password = "*******";
const char* http_username = "admin";
const char* http_password = "admin";
char thishost[17];

String inputString = "";
bool serialSwapped = false;

// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// State Machine for WebSocket Client;
_ws_client ws_client[MAX_WS_CLIENT]; 

#ifndef RGB_LED_PIN
void LedRGBFadeAnimUpdate(void * param) {}
void LedRGBAnimate(uint16_t param) {}
void LedRGBON (uint16_t hue, bool now) {}
void LedRGBOFF(void) {}
#else
/* ======================================================================
Function: LedRGBBlinkAnimUpdate
Purpose : Blink Anim update for RGB Led
Input   : -
Output  : - 
Comments: grabbed from NeoPixelBus library examples
====================================================================== */
void LedRGBBlinkAnimUpdate(const AnimationParam& param)
{
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0
  rgb_led.SetPixelColor(0, RgbColor(0));

  // 25% on so 75% off
  if (param.progress < 0.25f) {
    rgb_led.SetPixelColor(0, rgb_led_color);
  }
}

/* ======================================================================
Function: LedRGBFadeAnimUpdate
Purpose : Fade in and out effect for RGB Led
Input   : -
Output  : - 
Comments: grabbed from NeoPixelBus library examples
====================================================================== */
void LedRGBFadeAnimUpdate(const AnimationParam& param)
{
  // this gets called for each animation on every time step
  // progress will start at 0.0 and end at 1.0
  // apply a exponential curve to both front and back
  float progress = NeoEase::QuadraticInOut(param.progress) ;

  // we use the blend function on the RgbColor to mix
  // color based on the progress given to us in the animation
  #ifdef RGBW_LED
    RgbwColor updatedColor = RgbwColor::LinearBlend(  
                                animationState[param.index].RgbStartingColor, 
                                animationState[param.index].RgbEndingColor, 
                                progress);
    rgb_led.SetPixelColor(0, updatedColor);
  #else
    RgbColor updatedColor = RgbColor::LinearBlend(  
                                animationState[param.index].RgbStartingColor, 
                                animationState[param.index].RgbEndingColor, 
                                progress);
    rgb_led.SetPixelColor(0, updatedColor);
  #endif
}


/* ======================================================================
Function: LedRGBAnimate
Purpose : Manage RGBLed Animations
Input   : parameter (here animation time in ms)
Output  : - 
Comments: 
====================================================================== */
void LedRGBAnimate(uint16_t param)
{
  if ( animations.IsAnimating() ) {
    // the normal loop just needs these two to run the active animations
    animations.UpdateAnimations();
    rgb_led.Show();

  } else {

    // We start animation with current led color
    animationState[0].RgbStartingColor = rgb_led.GetPixelColor(0);

    // Node default fade in
    if ( rgb_anim_state==RGB_ANIM_NONE )
      rgb_anim_state=RGB_ANIM_FADE_IN ;

    // Fade in 
    if ( rgb_anim_state==RGB_ANIM_NONE || rgb_anim_state==RGB_ANIM_FADE_IN ) {
      animationState[0].RgbEndingColor = rgb_led_color; // selected color
      rgb_anim_state=RGB_ANIM_FADE_OUT; // Next
      animations.StartAnimation(0, param, LedRGBFadeAnimUpdate);

    // Fade out
    } else if ( rgb_anim_state==RGB_ANIM_FADE_OUT ) {
      animationState[0].RgbEndingColor = RgbColor(0); // off
      rgb_anim_state=RGB_ANIM_FADE_IN; // Next
      animations.StartAnimation(0, param, LedRGBFadeAnimUpdate);

    // Blink ON
    } else if ( rgb_anim_state==RGB_ANIM_BLINK_ON ) {
      rgb_anim_state=RGB_ANIM_BLINK_OFF; // Next
      animations.StartAnimation(0, param, LedRGBBlinkAnimUpdate);

    // Blink OFF
    } else if ( rgb_anim_state==RGB_ANIM_BLINK_OFF ) {
      rgb_anim_state=RGB_ANIM_BLINK_ON; // Next
      animations.StartAnimation(0, param, LedRGBBlinkAnimUpdate);
    }
  }
}

/* ======================================================================
Function: LedRGBON
Purpose : Set RGB LED strip color, but does not lit it
Input   : Hue (0..360)
          if led should be lit immediatly
Output  : - 
Comments: 
====================================================================== */
void LedRGBON (uint16_t hue, bool doitnow)
{
  // Convert to neoPixel API values
  // H (is color from 0..360) should be between 0.0 and 1.0
  // S is saturation keep it to 1
  // L is brightness should be between 0.0 and 0.5
  // rgb_luminosity is between 0 and 100 (percent)
  RgbColor target = HslColor( hue / 360.0f, 1.0f, 0.005f * rgb_luminosity);
  rgb_led_color = target;

  // do it now ?
  if (doitnow) {
    // Stop animation
    animations.StopAnimation(0);
    animationState[0].RgbStartingColor = target;
    animationState[0].RgbEndingColor = RgbColor(0);

    // set the strip
    rgb_led.SetPixelColor(0, target);
    rgb_led.Show();  
  }
}

/* ======================================================================
Function: LedRGBOFF 
Purpose : light off the RGB LED strip
Input   : -
Output  : - 
Comments: -
====================================================================== */
void LedRGBOFF(void)
{
  // stop animation, reset params
  animations.StopAnimation(0);
  animationState[0].RgbStartingColor = RgbColor(0);
  animationState[0].RgbEndingColor = RgbColor(0);
  rgb_led_color = RgbColor(0);
  rgb_anim_state=RGB_ANIM_NONE;

  // clear the strip
  rgb_led.SetPixelColor(0, RgbColor(0));
  rgb_led.Show();  
}
#endif


/* ======================================================================
Function: execCommand
Purpose : translate and execute command received from serial/websocket
Input   : client if coming from Websocket
          command received
Output  : - 
Comments: -
====================================================================== */
void execCommand(AsyncWebSocketClient * client, char * msg) {
  uint16_t l = strlen(msg);
  uint8_t index=MAX_WS_CLIENT;


  // Search if w're known client
  if (client) {
    for (index=0; index<MAX_WS_CLIENT ; index++) {
      // Exit for loop if we are there
      if (ws_client[index].id == client->id() ) 
        break;
    } // for all clients
  }

  //if (client)
  //  client->printf_P(PSTR("command[%d]='%s'"), l, msg);
  // Display on debug
  SERIAL_DEBUG.printf("  -> \"%s\"\r\n", msg);

  // Custom command to talk to device
  if (!strcmp_P(msg,PSTR("ping"))) {
    if (client)
      client->printf_P(PSTR("received your [[b;cyan;]ping], here is my [[b;cyan;]pong]"));

  } else if (!strcmp_P(msg,PSTR("swap"))) {
    Serial.swap();
    serialSwapped =! serialSwapped;
    if (client)
      client->printf_P(PSTR("Swapped UART pins, now using [[b;green;]RX-GPIO%d TX-GPIO%d]"),
                              serialSwapped?13:3,serialSwapped?15:1);

  // Debug information
  } else if ( !strncmp_P(msg,PSTR("debug"), 5) ) {
    int br = SERIAL_DEVICE.baudRate();
    if (client) {
      client->printf_P(PSTR("Baud Rate : [[b;green;]%d] bps"), br);
      #ifdef MOD_RN2483
      client->printf_P(PSTR("States : appli=[[b;green;]%d] radio=[[b;green;]%d]"), app_state, radioState() );
      #endif
    }

  // baud only display current Serial Speed
  } else if ( client && l==4 && !strncmp_P(msg,PSTR("baud"), 4) ) {
    client->printf_P(PSTR("Current Baud Rate is [[b;green;]%d] bps"), SERIAL_DEVICE.baudRate());

  // baud speed only display current Serial Speed
  } else if (l>=6 && !strncmp_P(msg,PSTR("baud "), 5) ) {
    uint32_t br = atoi(&msg[5]);
    if ( br==115200 || br==57600 || br==19200 || br==9600 ) {
      #ifdef MOD_RN2483
        radioInit(br);
      #elif defined (MOD_TERMINAL)
        SERIAL_DEVICE.begin(br);      
      #endif
      if (client)
        client->printf_P(PSTR("Serial Baud Rate is now [[b;green;]%d] bps"), br);
    } else {
      if (client) {
        client->printf_P(PSTR("[[b;red;]Error: Invalid Baud Rate %d]"), br);
        client->printf_P(PSTR("Valid baud rate are 115200, 57600, 19200 or 9600"));
      }
    } 

#ifdef RGB_LED_PIN  
  // rgb led current luminosity
  } else if ( client && l==3 && !strncmp_P(msg,PSTR("rgb"), 3) ) {
    client->printf_P(PSTR("Current RGB Led Luminosity is [[b;green;]%d%%]"), rgb_luminosity);

  // rgb led luminosity
  } else if (l>=5 && !strncmp_P(msg,PSTR("rgb "), 4) ) {
    uint8_t lum = atoi(&msg[4]);
    if ( lum>=0 && lum<=100) {
      rgb_luminosity = lum;
      if (client)
        client->printf_P(PSTR("RGB Led Luminosity is now [[b;green;]%d]"), lum);
    } else {
      if (client) {
        client->printf_P(PSTR("[[b;red;]Error: Invalid RGB Led Luminosity value %d]"), lum);
        client->printf_P(PSTR("Valid is from 0 (off) to 100 (full)"));
      }
    } 
#endif

  } else if (client && !strcmp_P(msg,PSTR("hostname")) ) {
    client->printf_P(PSTR("[[b;green;]%s]"), thishost);

  // Dir files on SPIFFS system
  // --------------------------
  } else if (!strcmp_P(msg,PSTR("ls")) ) {
    Dir dir = SPIFFS.openDir("/");
    uint16_t cnt = 0;
    String out = PSTR("SPIFFS Files\r\n Size   Name");
    char buff[16];

    while ( dir.next() ) {
      cnt++;
      File entry = dir.openFile("r");
      sprintf_P(buff, "\r\n%6d ", entry.size());
      //client->printf_P(PSTR("%5d %s"), entry.size(), entry.name());
      out += buff;
      out += String(entry.name()).substring(1);
      entry.close();
    }
    if (client) 
      client->printf_P(PSTR("%s\r\nFound %d files"), out.c_str(), cnt);

  // read file and send to serial
  // ----------------------------
  } else if (l>=6 && !strncmp_P(msg,PSTR("read "), 5) ) {
    char * pfname = &msg[5];

    if ( *pfname != '/' )
      *--pfname = '/';

    // file exists
    if (SPIFFS.exists(pfname)) {
      // open file for reading.
      File ofile = SPIFFS.open(pfname, "r");
      if (ofile) {
        char c;
        String str="";
        if (client)
          client->printf_P(PSTR("Reading file %s"), pfname);
        // Read until end
        while (ofile.available())
        {
          // Read all chars
          c = ofile.read();
          if (c=='\r') {
            // Nothing to do 
          } else if (c == '\n') {
            str.trim();
            if (str!="") {
              char c = str.charAt(0);
              // Not a comment
              if ( c != '#' ) {
                // internal command for us ?
                if ( c=='!' || c=='$' ) {
                  // Call ourselve to execute internal, command
                  execCommand(client, (char*) (str.c_str())+1);

                  // Don't read serial in async (websocket)
                  if (c=='$' && !client) {
                    String ret = SERIAL_DEVICE.readStringUntil('\n');
                    ret.trim();
                    SERIAL_DEBUG.printf("  <- \"%s\"\r\n", ret.c_str());
                  }
                } else {
                  // send content to Serial (passtrough)
                  SERIAL_DEVICE.print(str);
                  SERIAL_DEVICE.print("\r\n");

                  // and to do connected client
                  if (client)
                    client->printf_P(PSTR("[[b;green;]%s]"), str.c_str());
                }
              } else {
                // and to do connected client
                if (client)
                  client->printf_P(PSTR("[[b;grey;]%s]"), str.c_str());
              }
            }
            str = "";
          } else {
            // prepare line
            str += c;
          }
        }
        ofile.close();
      } else {
        if (client)
          client->printf_P(PSTR("[[b;red;]Error: opening file %s]"), pfname);
      }
    } else {
      if (client)
        client->printf_P(PSTR("[[b;red;]Error: file %s not found]"), pfname);
    }

  // show file content
  // -----------------
  } else if (l>=6 && !strncmp_P(msg,PSTR("cat "), 4) ) {
    char * pfname = &msg[4];

    if ( *pfname != '/' )
      *--pfname = '/';

    // file exists
    if (SPIFFS.exists(pfname)) {
      // open file for reading.
      File ofile = SPIFFS.open(pfname, "r");
      if (ofile) {

        size_t size = ofile.size();
        size_t chunk;
        char * p;

        client->printf_P(PSTR("content of file %s size %u bytes"), pfname, size);

        // calculate chunk size (max 1Kb)
        chunk = size>=1024?1024:size;

        // Allocate a buffer to store contents of the file + \0
        p = (char *) malloc( chunk+1 );

        while (p && size) {
          ofile.readBytes(p, chunk);
          *(p+chunk) = '\0';

          if (client)
            client->text(p);

          // This is done 
          size -= chunk;

          // Last chunk
          if (size<chunk)
            chunk = size;
        }

        // Free up our buffer
        if (p)
          free(p);

      } else {
        if (client)
          client->printf_P(PSTR("[[b;red;]Error: opening file %s]"), pfname);
      }
    } else {
      if (client)
        client->printf_P(PSTR("[[b;red;]Error: file %s not found]"), pfname);
    }

  // no delay in client (websocket)
  // ----------------------------
  } else if (client==NULL && l>=7 && !strncmp_P(msg,PSTR("delay "), 6) ) {
    uint16_t v= atoi(&msg[6]);
    if (v>=0 && v<=60000 ) {
      while(v--) {
        LedRGBAnimate(500);
        delay(1);
      }
    }

  // ----------------------------
  } else if (l>=7 && !strncmp_P(msg,PSTR("reset "), 6) ) {
    int v= atoi(&msg[6]);
    if (v>=0 && v<=16) {
      pinMode(v, OUTPUT);
      digitalWrite(v, HIGH);
      if (client)
        client->printf_P(PSTR("[[b;orange;]Reseting pin %d]"), v);
      digitalWrite(v, LOW);
      delay(50);
      digitalWrite(v, HIGH);
    } else {
      if (client) {
        client->printf_P(PSTR("[[b;red;]Error: Invalid pin number %d]"), v);
        client->printf_P(PSTR("Valid pin number are 0 to 16"));
      }
    }

  } else if (client && !strcmp_P(msg,PSTR("fw"))) {
    client->printf_P(PSTR("Firmware version [[b;green;]%s %s]"), __DATE__, __TIME__);

  } else if (client && !strcmp_P(msg,PSTR("whoami"))) {
    client->printf_P(PSTR("[[b;green;]You are client #%u at index[%d&#93;]"),client->id(), index );

  } else if (client && !strcmp_P(msg,PSTR("who"))) {
    uint8_t cnt = 0;
    // Count client
    for (uint8_t i=0; i<MAX_WS_CLIENT ; i++) {
      if (ws_client[i].id ) {
        cnt++;
      }
    }
    
    client->printf_P(PSTR("[[b;green;]Current client total %d/%d possible]"), cnt, MAX_WS_CLIENT);
    for (uint8_t i=0; i<MAX_WS_CLIENT ; i++) {
      if (ws_client[i].id ) {
        client->printf_P(PSTR("index[[[b;green;]%d]]; client [[b;green;]#%d]"), i, ws_client[i].id );
      }
    }

  } else if (client && !strcmp_P(msg,PSTR("heap"))) {
    client->printf_P(PSTR("Free Heap [[b;green;]%ld] bytes"), ESP.getFreeHeap());

  } else if (client && (*msg=='?' || !strcmp_P(msg,PSTR("help")))) {
    client->printf_P(PSTR(HELP_TEXT));

  // all other to serial Proxy
  }  else if (*msg) {
    //SERIAL_DEBUG.printf("Text '%s'", msg);
    // Send text to serial
    SERIAL_DEVICE.print(msg);
    SERIAL_DEVICE.print("\r\n");
  }
}

/* ======================================================================
Function: execCommand
Purpose : translate and execute command received from serial/websocket
Input   : client if coming from Websocket
          command from Flash
Output  : - 
Comments: -
====================================================================== */
void execCommand(AsyncWebSocketClient * client, PGM_P msg) {
  size_t n = strlen_P(msg);
  char * cmd = (char*) malloc(n+1);
  if( cmd) {
    for(size_t b=0; b<n; b++) {
      cmd[b] = pgm_read_byte(msg++);
    }
    cmd[n] = 0;
    execCommand(client, cmd);
  } // if cmd
}

/* ======================================================================
Function: onEvent
Purpose : Manage routing of websocket events
Input   : -
Output  : - 
Comments: -
====================================================================== */
void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    uint8_t index;
    SERIAL_DEBUG.printf("ws[%s][%u] connect\n", server->url(), client->id());

    for (index=0; index<MAX_WS_CLIENT ; index++) {
      if (ws_client[index].id == 0 ) {
        ws_client[index].id = client->id();
        ws_client[index].state = CLIENT_ACTIVE;
        SERIAL_DEBUG.printf("added #%u at index[%d]\n", client->id(), index);
        client->printf("[[b;green;]Hello Client #%u, added you at index %d]", client->id(), index);
        client->ping();
        break; // Exit for loop
      }
    }
    if (index>=MAX_WS_CLIENT) {
      SERIAL_DEBUG.printf("not added, table is full");
      client->printf("[[b;red;]Sorry client #%u, Max client limit %d reached]", client->id(), MAX_WS_CLIENT);
      client->ping();
    }

  } else if(type == WS_EVT_DISCONNECT){
    SERIAL_DEBUG.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    for (uint8_t i=0; i<MAX_WS_CLIENT ; i++) {
      if (ws_client[i].id == client->id() ) {
        ws_client[i].id = 0;
        ws_client[i].state = CLIENT_NONE;
        SERIAL_DEBUG.printf("freed[%d]\n", i);
        break; // Exit for loop
      }
    }
  } else if(type == WS_EVT_ERROR){
    SERIAL_DEBUG.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    SERIAL_DEBUG.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*) arg;
    char * msg = NULL;
    size_t n = info->len;
    uint8_t index;

    // Size of buffer needed
    // String same size +1 for \0
    // Hex size*3+1 for \0 (hex displayed as "FF AA BB ...")
    n = info->opcode == WS_TEXT ? n+1 : n*3+1;

    msg = (char*) calloc(n, sizeof(char));
    if (msg) {
      // Grab all data
      for(size_t i=0; i < info->len; i++) {
        if (info->opcode == WS_TEXT ) {
          msg[i] = (char) data[i];
        } else {
          sprintf_P(msg+i*3, PSTR("%02x "), (uint8_t) data[i]);
        }
      }
    }

    SERIAL_DEBUG.printf("ws[%s][%u] message %s\n", server->url(), client->id(), msg);

    // Search if it's a known client
    for (index=0; index<MAX_WS_CLIENT ; index++) {
      if (ws_client[index].id == client->id() ) {
        SERIAL_DEBUG.printf("known[%d] '%s'\n", index, msg);
        SERIAL_DEBUG.printf("client #%d info state=%d\n", client->id(), ws_client[index].state);

        // Received text message
        if (info->opcode == WS_TEXT) {
          execCommand(client, msg);
        } else {
          SERIAL_DEBUG.printf("Binary 0x:%s", msg);
        }
        // Exit for loop
        break;
      } // if known client
    } // for all clients

    // Free up allocated buffer
    if (msg) 
      free(msg);

  } // EVT_DATA
}

/* ======================================================================
Function: setup
Purpose : Setup I/O and other one time startup stuff
Input   : -
Output  : - 
Comments: -
====================================================================== */
void setup() {
  // Set Hostname for OTA and network (add only 2 last bytes of last MAC Address)
  // You can't have _ or . in hostname 
  sprintf_P(thishost, PSTR("WS2Serial-%04X"), ESP.getChipId() & 0xFFFF);

  #ifdef MOD_RN2483
    SERIAL_DEVICE.begin(57600);
  #else
    SERIAL_DEVICE.begin(115200);
  #endif

  SERIAL_DEBUG.begin(115200);
  SERIAL_DEBUG.print(F("\r\nBooting on "));
  SERIAL_DEBUG.println(ARDUINO_BOARD);
  SPIFFS.begin();
  WiFi.mode(WIFI_STA);


  // No empty sketch SSID, try connect 
  if (*ssid!='*' && *password!='*' ) {
    SERIAL_DEBUG.printf("connecting to %s with psk %s\r\n", ssid, password );
    WiFi.begin(ssid, password);
  } else {
    // empty sketch SSID, try autoconnect with SDK saved credentials
    SERIAL_DEBUG.println(F("No SSID/PSK defined in sketch\r\nConnecting with SDK ones if any"));
  }

  // Loop until connected
  while ( WiFi.status() !=WL_CONNECTED ) {
    LedRGBON(COLOR_ORANGE_YELLOW);
    LedRGBAnimate(500);
    yield(); 
  }

  SERIAL_DEBUG.print(F("I'm network device named "));
  SERIAL_DEBUG.println(thishost);

  ArduinoOTA.setHostname(thishost);

  // OTA callbacks
  ArduinoOTA.onStart([]() {
    // Clean SPIFFS
    SPIFFS.end();

    // Light of the LED, stop animation
    LedRGBOFF();

    ws.textAll("OTA Update Started");
    ws.enable(false);
    ws.closeAll();

  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint8_t percent = progress / (total / 100);
    // hue from 0.0 to 1.0 (rainbow) with 33% (of 0.5f) luminosity
    #ifdef RGB_LED_PIN
      rgb_led.SetPixelColor(0, HslColor(percent * 0.01f , 1.0f, 0.17f ));
      rgb_led.Show();  
    #endif
  });

  ArduinoOTA.onEnd([]() { 
    #ifdef RGB_LED_PIN
      rgb_led.SetPixelColor(0, HslColor(COLOR_GREEN/360.0f, 1.0f, 0.25f));
      rgb_led.Show();  
    #endif
  });

  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef RGB_LED_PIN
      rgb_led.SetPixelColor(0, HslColor(COLOR_RED/360.0f, 1.0f, 0.25f));
      rgb_led.Show();  
    #endif
    ESP.restart(); 
  });

  ArduinoOTA.begin();
  MDNS.addService("http","tcp",80);


  // Enable and start websockets
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  
  server.addHandler(new SPIFFSEditor(http_username,http_password));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
  

  server.onNotFound([](AsyncWebServerRequest *request){
    SERIAL_DEBUG.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      SERIAL_DEBUG.printf("GET");
    else if(request->method() == HTTP_POST)
      SERIAL_DEBUG.printf("POST");
    else if(request->method() == HTTP_DELETE)
      SERIAL_DEBUG.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      SERIAL_DEBUG.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      SERIAL_DEBUG.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      SERIAL_DEBUG.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      SERIAL_DEBUG.printf("OPTIONS");
    else
      SERIAL_DEBUG.printf("UNKNOWN");

    SERIAL_DEBUG.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      SERIAL_DEBUG.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      SERIAL_DEBUG.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      SERIAL_DEBUG.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        SERIAL_DEBUG.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        SERIAL_DEBUG.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        SERIAL_DEBUG.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      SERIAL_DEBUG.printf("UploadStart: %s\n", filename.c_str());
    SERIAL_DEBUG.printf("%s", (const char*)data);
    if(final)
      SERIAL_DEBUG.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      SERIAL_DEBUG.printf("BodyStart: %u\n", total);
    SERIAL_DEBUG.printf("%s", (const char*)data);
    if(index + len == total)
      SERIAL_DEBUG.printf("BodyEnd: %u\n", total);
  });

  // Set on board led GPIO to outout if not GPIO2 and Debug Serial1
  #if (SERIAL_DEBUG != Serial1) && (BUILTIN_LED != 2)
  pinMode(BUILTIN_LED, OUTPUT);
  #endif

  #if defined (BTN_GPIO)
    pinMode(BTN_GPIO, INPUT);
  #endif

  // Start Server
  WiFiMode_t con_type = WiFi.getMode();
  uint16_t lcolor = 0;
  server.begin();
  SERIAL_DEBUG.print(F("Started "));

  if (con_type == WIFI_STA) {
    SERIAL_DEBUG.print(F("WIFI_STA"));
    lcolor=COLOR_GREEN;
  } else if (con_type == WIFI_AP_STA) {
    SERIAL_DEBUG.print(F("WIFI_AP_STA"));
    lcolor=COLOR_CYAN;
  } else if (con_type == WIFI_AP) {
    SERIAL_DEBUG.print(F("WIFI_AP"));
    lcolor=COLOR_MAGENTA;
  } else {
    SERIAL_DEBUG.print(F("????"));
    lcolor = COLOR_RED;
  }

  SERIAL_DEBUG.print(F("on HTTP://"));
  SERIAL_DEBUG.print(WiFi.localIP());
  SERIAL_DEBUG.print(F(" and WS://"));
  SERIAL_DEBUG.print(WiFi.localIP());
  SERIAL_DEBUG.println(F("/ws"));

  // Set Breathing color during startup script
  LedRGBON(lcolor, true);

  // Run startup script if any
  execCommand(NULL, PSTR("read /startup.ini") );

  // We're alive, CYAN LED
  // move animation from fade to blink
  #ifdef RGB_LED_PIN
  rgb_anim_state=RGB_ANIM_BLINK_ON;
  LedRGBON(COLOR_CYAN, true);
  #endif

  #ifdef RN2483
  app_state = APP_IDLE;
  #endif
}

/* ======================================================================
Function: loop
Purpose : infinite loop main code
Input   : -
Output  : - 
Comments: -
====================================================================== */
void loop() {
  static bool led_state ; 
  bool new_led_state ; 
  uint16_t anim_speed;
  uint8_t button_port;

  #ifdef SERIAL_DEVICE
  // Got one serial char ?
  if (SERIAL_DEVICE.available()) {
    // Read it and store it in buffer
    char inChar = (char)SERIAL_DEVICE.read();

    // CR line char, discard ?
    if (inChar == '\r') {
      // Do nothing

    // LF ok let's do our job
    } else if (inChar == '\n') {
      // Send to all client without cr/lf
      ws.textAll(inputString);
      // Display on debug
      SERIAL_DEBUG.printf("  <- \"%s\"\r\n", inputString.c_str());

      #ifdef MOD_RN2483
        if (radioResponse(inputString)) {
          delay(200);
        }
      #endif

      inputString = "";
    } else {
      // Add char to input string
      if (inChar>=' ' && inChar<='}')
        inputString += inChar;
      else
        inputString += '.';
    }
  }
  #endif

  // Led blink management 
  if (WiFi.status()==WL_CONNECTED) {
    new_led_state = ((millis() % 1000) < 200) ? LOW:HIGH; // Connected long blink 200ms on each second
    anim_speed = 1000;
  } else {
    new_led_state = ((millis() % 333) < 111) ? LOW:HIGH;// AP Mode or client failed quick blink 111ms on each 1/3sec
    anim_speed = 333;
  }
    // Led management
  if (led_state != new_led_state) {
    led_state = new_led_state;

    #if (SERIAL_DEBUG != Serial1) && (BUILTIN_LED != 2)
      digitalWrite(BUILTIN_LED, led_state);
    #endif
  }

  #if defined (BTN_GPIO) && defined (MOD_RN2483)

  // Get switch port state 
  button_port = digitalRead(BTN_GPIO);

  // Button pressed 
  if (button_port==BTN_PRESSED){
    btn_state_e btn_state;

    // we enter into the loop to manage
    // the function that will be done
    // depending on button press duration
    do {
      // keep watching the push button:
      btn_state = buttonManageState(button_port);

      // read new state button
      button_port = digitalRead(BTN_GPIO);

      // this loop can be as long as button is pressed
      yield();
      ArduinoOTA.handle();
    }
    // we loop until button state machine finished
    while (btn_state != BTN_WAIT_PUSH);

    // Do what we need to do depending on action
    radioManageState(_btn_Action);

    //SERIAL_DEBUG.printf("Button %d\r\n", _btn_Action);

  // button not pressed
  } else {
    radioManageState(BTN_NONE);
  }

  // move next animation to blink
  if (radioState()==RADIO_IDLE && app_state==APP_IDLE) {
    rgb_anim_state=RGB_ANIM_BLINK_ON;
    LedRGBON(COLOR_CYAN);
  }
  // move next animation to default fade
  if (app_state==APP_CONTINUOUS_LISTEN) {
    rgb_anim_state=RGB_ANIM_NONE;
    LedRGBON(COLOR_CYAN);
  }
  if (radioState()==RADIO_IDLE && app_state==APP_CONTINUOUS_SEND) {
    rgb_anim_state=RGB_ANIM_NONE;
    LedRGBON(COLOR_MAGENTA);
  }
  #endif

  // Handle remote Wifi Updates
  ArduinoOTA.handle();

  // RGB LED Animation
  LedRGBAnimate(anim_speed);
}
