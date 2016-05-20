#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Maximum number of simultaned clients connected (WebSocket)
#define MAX_WS_CLIENT 5

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
                  "[[b;cyan;]baud n]    set serial baud rate to n\n" \
                  "[[b;cyan;]reset p]   reset gpio pin number p\n" \
                  "[[b;cyan;]ls]        list SPIFFS files\n" \
                  "[[b;cyan;]read file] send SPIFFS file to serial (read)" 

// Web Socket client state
typedef struct {
  uint32_t  id;
  uint8_t   state;
} _ws_client; 

// Uncomment 3 lines below if you have an WS1812B RGB LED 
// like shield here https://github.com/hallard/WeMos-RN2483
//#define RGB_LED_PIN   0
//#define RGB_LED_COUNT 1
//#define RGBW_LED // 

// RGB Led
#ifdef RGB_LED_PIN
  #include <NeoPixelAnimator.h>
  #include <NeoPixelBus.h>

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

  uint8_t  rgb_led_effect_state;  // general purpose variable used to store effect state

  #ifdef RGBW_LED
    NeoPixelBus<NeoGrbwFeature, NeoEsp8266BitBang800KbpsMethod> rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
    // what is stored for state is specific to the need, in this case, the colors.
    // basically what ever you need inside the animation update function
    struct MyAnimationState
    {
      RgbwColor RgbStartingColor;
      RgbwColor RgbEndingColor;
      uint8_t   IndexPixel;   // general purpose variable used to store pixel index
    };
    RgbwColor rgb_led_color; // RGBW Led color
  #else
    NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod> rgb_led(RGB_LED_COUNT, RGB_LED_PIN);
    // what is stored for state is specific to the need, in this case, the colors.
    // basically what ever you need inside the animation update function
    struct MyAnimationState
    {
      RgbColor  RgbStartingColor;
      RgbColor  RgbEndingColor;
      uint8_t   IndexPixel;   // general purpose variable used to store pixel index
    };
    RgbColor rgb_led_color; // RGB Led color
  #endif

  // one entry per pixel to match the animation timing manager
  NeoPixelAnimator animations(1); 
  MyAnimationState animationState[1];

#endif




// WEB HANDLER IMPLEMENTATION
class SPIFFSEditor: public AsyncWebHandler {
  private:
    String _username;
    String _password;
    bool _uploadAuthenticated;
  public:
    SPIFFSEditor(String username=String(), String password=String()):_username(username),_password(password),_uploadAuthenticated(false){}
    bool canHandle(AsyncWebServerRequest *request){
      if(request->method() == HTTP_GET && request->url() == "/edit" && (SPIFFS.exists("/edit.htm") || SPIFFS.exists("/edit.htm.gz")))
        return true;
      else if(request->method() == HTTP_GET && request->url() == "/list")
        return true;
      else if(request->method() == HTTP_GET && (request->url().endsWith("/") || SPIFFS.exists(request->url()) || (!request->hasParam("download") && SPIFFS.exists(request->url()+".gz"))))
        return true;
      else if(request->method() == HTTP_POST && request->url() == "/edit")
        return true;
      else if(request->method() == HTTP_DELETE && request->url() == "/edit")
        return true;
      else if(request->method() == HTTP_PUT && request->url() == "/edit")
        return true;
      return false;
    }

    void handleRequest(AsyncWebServerRequest *request){
      if(_username.length() && (request->method() != HTTP_GET || request->url() == "/edit" || request->url() == "/list") && !request->authenticate(_username.c_str(),_password.c_str()))
        return request->requestAuthentication();

      if(request->method() == HTTP_GET && request->url() == "/edit"){
        request->send(SPIFFS, "/edit.htm");
      } else if(request->method() == HTTP_GET && request->url() == "/list"){
        if(request->hasParam("dir")){
          String path = request->getParam("dir")->value();
          Dir dir = SPIFFS.openDir(path);
          path = String();
          String output = "[";
          while(dir.next()){
            File entry = dir.openFile("r");
            if (output != "[") output += ',';
            bool isDir = false;
            output += "{\"type\":\"";
            output += (isDir)?"dir":"file";
            output += "\",\"name\":\"";
            output += String(entry.name()).substring(1);
            output += "\"}";
            entry.close();
          }
          output += "]";
          request->send(200, "text/json", output);
          output = String();
        }
        else
          request->send(400);
      } else if(request->method() == HTTP_GET){
        String path = request->url();
        if(path.endsWith("/"))
          path += "index.htm";
        request->send(SPIFFS, path, String(), request->hasParam("download"));
      } else if(request->method() == HTTP_DELETE){
        if(request->hasParam("path", true)){
          ESP.wdtDisable(); SPIFFS.remove(request->getParam("path", true)->value()); ESP.wdtEnable(10);
          request->send(200, "", "DELETE: "+request->getParam("path", true)->value());
        } else
          request->send(404);
      } else if(request->method() == HTTP_POST){
        if(request->hasParam("data", true, true) && SPIFFS.exists(request->getParam("data", true, true)->value()))
          request->send(200, "", "UPLOADED: "+request->getParam("data", true, true)->value());
        else
          request->send(500);
      } else if(request->method() == HTTP_PUT){
        if(request->hasParam("path", true)){
          String filename = request->getParam("path", true)->value();
          if(SPIFFS.exists(filename)){
            request->send(200);
          } else {
            File f = SPIFFS.open(filename, "w");
            if(f){
              f.write(0x00);
              f.close();
              request->send(200, "", "CREATE: "+filename);
            } else {
              request->send(500);
            }
          }
        } else
          request->send(400);
      }
    }

    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if(!index){
        if(!_username.length() || request->authenticate(_username.c_str(),_password.c_str()))
          _uploadAuthenticated = true;
        request->_tempFile = SPIFFS.open(filename, "w");
      }
      if(_uploadAuthenticated && request->_tempFile && len){
        ESP.wdtDisable(); request->_tempFile.write(data,len); ESP.wdtEnable(10);
      }
      if(_uploadAuthenticated && final)
        if(request->_tempFile) request->_tempFile.close();
    }
};

extern "C" void system_set_os_print(uint8 onoff);
extern "C" void ets_install_putc1(void* routine);

//Use the internal hardware buffer
static void _u0_putc(char c){
  while(((U0S >> USTXC) & 0x7F) == 0x7F);
  U0F = c;
}

const char* ssid = "*******";
const char* password = "*******";
const char* http_username = "admin";
const char* http_password = "admin";

String inputString = "";
bool serialSwapped = false;

// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// State Machine for WebSocket Client;
_ws_client ws_client[MAX_WS_CLIENT]; 

#ifndef RGB_LED_PIN
#define LedRGBFadeAnimUpdate(p) {}
#define LedRGBAnimate(p) {}
#define LedRGBON(h) {}
#define LedRGBOFF() {}
#else
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
  float progress = NeoEase::QuadraticInOut(param.progress);

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
    animationState[0].RgbStartingColor = rgb_led.GetPixelColor(0);
    animationState[0].RgbEndingColor = rgb_led_effect_state==0 ? rgb_led_color:RgbColor(0);
    animations.StartAnimation(0, param, LedRGBFadeAnimUpdate);

    // toggle to the next effect state
    rgb_led_effect_state = (rgb_led_effect_state + 1) % 2;
  }
}

/* ======================================================================
Function: LedRGBON
Purpose : Set RGB LED strip color, but does not lit it
Input   : Hue (0..360)
Output  : - 
Comments: 
====================================================================== */
void LedRGBON (uint16_t hue)
{
  // Convert to neoPixel API values
  // H (is color from 0..360) should be between 0.0 and 1.0
  // S is saturation keep it to 1
  // L is brightness should be between 0.0 and 0.5
  RgbColor target = HslColor( hue / 360.0f, 1.0f, 0.5f );
  rgb_led_color = target;
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

  // clear the strip
  rgb_led.SetPixelColor(0, RgbColor(0));
  rgb_led.Show();  
}
#endif


/* ======================================================================
Function: execCommand
Purpose : translate and execute command
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

  // Custom command to talk to device
  if (!strcmp_P(msg,PSTR("ping"))) {
    if (client)
      client->printf_P(PSTR("received your [[b;cyan;]ping], here is my [[b;cyan;]pong]"));

  } else if (!strcmp_P(msg,PSTR("swap"))) {
    Serial.swap();
    serialSwapped != serialSwapped;
    if (client)
      client->printf_P(PSTR("Swapped UART pins, now using [[b;green;]RX-GPIO%d TX-GPIO%d]"),
                              serialSwapped?13:3,serialSwapped?15:1);

  } else if (l>=6 && !strncmp_P(msg,PSTR("baud "), 5) ) {
    int v= atoi(&msg[5]);
    if (v==115200 || v==19200 || v==57600 || v==9600) {
      Serial.flush(); 
      delay(10);    
      Serial.end();
      delay(10);    
      Serial.begin(v);
      if (client)
        client->printf_P(PSTR("Serial Baud Rate is now [[b;green;]%d] bps"), v);
    } else {
      if (client) {
        client->printf_P(PSTR("[[b;red;]Error: Invalid Baud Rate %d]"), v);
        client->printf_P(PSTR("Valid baud rate are 115200, 57600, 19200 or 9600"));
      }
    }

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
              if (str.charAt(0)!='#') {

                if (str.charAt(0)=='!') {
                  // Call ourselve to execute interne, command
                  execCommand(client, (char*) (str.c_str())+1);
                } else {
                  // send content to Serial
                  Serial.print(str);
                  Serial.print("\r\n");

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

  // no delay in client (websocket)
  // ----------------------------
  } else if (client==NULL && l>=7 && !strncmp_P(msg,PSTR("delay "), 6) ) {
    uint16_t v= atoi(&msg[6]);
    if (v>=0 && v<=60000 ) {
      //Serial.printf("delay(%d)\r\n",v);
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
    //os_printf("Text '%s'", msg);
    // Send text to serial
    Serial.print(msg);
    Serial.print("\r\n");
  }
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
    os_printf("ws[%s][%u] connect\n", server->url(), client->id());

    for (index=0; index<MAX_WS_CLIENT ; index++) {
      if (ws_client[index].id == 0 ) {
        ws_client[index].id = client->id();
        ws_client[index].state = CLIENT_ACTIVE;
        os_printf("added #%u at index[%d]\n", client->id(), index);
        client->printf("[[b;green;]Hello Client #%u, added you at index %d]", client->id(), index);
        client->ping();
        break; // Exit for loop
      }
    }
    if (index>=MAX_WS_CLIENT) {
      os_printf("not added, table is full");
      client->printf("[[b;red;]Sorry client #%u, Max client limit %d reached]", client->id(), MAX_WS_CLIENT);
      client->ping();
    }

  } else if(type == WS_EVT_DISCONNECT){
    os_printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    for (uint8_t i=0; i<MAX_WS_CLIENT ; i++) {
      if (ws_client[i].id == client->id() ) {
        ws_client[i].id = 0;
        ws_client[i].state = CLIENT_NONE;
        os_printf("freed[%d]\n", i);
        break; // Exit for loop
      }
    }
  } else if(type == WS_EVT_ERROR){
    os_printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    os_printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
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

    os_printf("ws[%s][%u] message %s\n", server->url(), client->id(), msg);

    // Search if it's a known client
    for (index=0; index<MAX_WS_CLIENT ; index++) {
      if (ws_client[index].id == client->id() ) {
        os_printf("known[%d] '%s'\n", index, msg);
        os_printf("client #%d info state=%d\n", client->id(), ws_client[index].state);

        // Received text message
        if (info->opcode == WS_TEXT) {
          execCommand(client, msg);
        } else {
          os_printf("Binary 0x:%s", msg);
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
void setup(){
  char host[17];

  // Set Hostname for OTA and network
  sprintf_P(host, PSTR("WS2Serial_%06X"), ESP.getChipId());

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.print(F("\r\nBooting "));
  Serial.println(host);
  SPIFFS.begin();
  WiFi.mode(WIFI_STA);

  // No empty sketch SSID, try connect 
  if (*ssid!='*' && *password!='*' ) {
    Serial.printf("connecting to %s with psk %s\r\n", ssid, password );
    WiFi.begin(ssid, password);
  } else {
    // empty sketch SSID, try autoconnect with SDK saved credentials
    Serial.println(F("No SSID/PSK defined in sketch\r\nConnecting with SDK ones if any"));
  }

  // Loop until connected
  while ( WiFi.status() !=WL_CONNECTED ) {
    LedRGBON(COLOR_ORANGE_YELLOW);
    LedRGBAnimate(500);
    yield(); 
  }

  Serial.print(F("I'm network device named "));
  Serial.println(host);

  ArduinoOTA.setHostname(host);
  ArduinoOTA.begin();
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.serveStatic("/fs", SPIFFS, "/");
  server.serveStatic("/",   SPIFFS, "/index.htm", "max-age=86400");
  server.serveStatic("/",   SPIFFS, "/",          "max-age=86400"); 

  server.addHandler(new SPIFFSEditor(http_username,http_password));

  server.onNotFound([](AsyncWebServerRequest *request){
    os_printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      os_printf("GET");
    else if(request->method() == HTTP_POST)
      os_printf("POST");
    else if(request->method() == HTTP_DELETE)
      os_printf("DELETE");
    else if(request->method() == HTTP_PUT)
      os_printf("PUT");
    else if(request->method() == HTTP_PATCH)
      os_printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      os_printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      os_printf("OPTIONS");
    else
      os_printf("UNKNOWN");

    os_printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      os_printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      os_printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      os_printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        os_printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        os_printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        os_printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      os_printf("UploadStart: %s\n", filename.c_str());
    os_printf("%s", (const char*)data);
    if(final)
      os_printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      os_printf("BodyStart: %u\n", total);
    os_printf("%s", (const char*)data);
    if(index + len == total)
      os_printf("BodyEnd: %u\n", total);
  });

  // Set on board led GPIO to outout
  pinMode(BUILTIN_LED, OUTPUT);

  // Set Breathing to magenta
  LedRGBON(COLOR_MAGENTA);

  // Run startup script if any
  char cmd[] = "read /startup.ini";
  execCommand(NULL, cmd);

  // Start Server
  server.begin();
  Serial.print(F("Started HTTP://"));
  Serial.print(WiFi.localIP());
  Serial.print(F(" and WS://"));
  Serial.print(WiFi.localIP());
  Serial.println(F("/ws"));

  // Set Breathing to Green
  LedRGBON(COLOR_GREEN);
}

/* ======================================================================
Function: loop
Purpose : infinite loop main code
Input   : -
Output  : - 
Comments: -
====================================================================== */
void loop() {
  bool      led_state ; 
  uint16_t fade_speed;

  // Got one serial char ?
  if (Serial.available()) {
    // Read it and store it in buffer
    char inChar = (char)Serial.read();
    inputString += inChar;

    // New line char ?
    if (inChar == '\n') {
      // Send to all client
      ws.textAll(inputString);
      inputString = "";
    }
  }

  // Led blink management 
  if (WiFi.status()==WL_CONNECTED) {
    led_state = ((millis() % 1000) < 200) ? LOW:HIGH; // Connected long blink 200ms on each second
    fade_speed = 1000;
  } else {
    led_state = ((millis() % 333) < 111) ? LOW:HIGH;// AP Mode or client failed quick blink 111ms on each 1/3sec
    fade_speed = 333;
  }
  // Led management
  digitalWrite(BUILTIN_LED, led_state);

  // Handle remote Wifi Updates
  ArduinoOTA.handle();

  // RGB LED Animation
  LedRGBAnimate(fade_speed);
}
