Transparent TCP Network to Serial Proxy using WebSocket for ESP8266
===================================================================

This is a pure transparent bridge between Wifi and serial using any ESP8266 device. It's very useful for debugging or talking to remote serial device that have no network connection.

I'm using it on WeMos target, you can find more information on WeMos on their [site][1], it's really well documented.
I now use WeMos boards instead of NodeMCU's one because they're just smaller, features remains the same, but I also suspect WeMos regulator far better quality than the one used on NodeMCU that are just fake of originals AMS117 3V3.

This project is mainly based on excellent @me-no-dev [ESPAsyncWebServer][4] library and great [JQuery Terminal][3] done by Jakub Jankiewicz.

Documentation
=============

Once uploaded SPIFFS data (web page) you can connect with a browser to http://ip_of_esp8266 and start playing with it.
The main `index.htm` web page include a full javascript terminal so you can type command and receive response.

The main web page can also be hosted anywhere and it's not mandatory to have it on the device (except if device and your computer have no access on Internet). I've published the fully fonctionnal WEB page from github so you can access it from [here][9] and then connect to your device on wich you flashed the firmware.

Some commands will be interpreted by the target (ESP8266) and not passed to serial, so you can interact with ESP8266 doing some variable stuff.

Test web page without ESP8266
-----------------------------

You need to have [nodejs][7] and some dependencies installed.

[webdev][10] folder is the development folder to test and validate web pages. It's used to avoid flashing the device on each modification.
All source files are located in this folder the ESP8266 `data` folder (containing web pages) is filled by a nodejs script launched from [webdev][10] folder. This repo contain in [data][13] lasted files so if you don't change any file, you can upload to SPIFFS as is.

To test web pages, go to a command line, go into [webdev][10] folder and issue a:    
`node web_server.js`     
then connect your browser to htpp://localhost:8080 you can them modidy and test source files such index.htm
    
Once all is okay issue a:    
`node create_spiffs.js`     
this will gzip file and put them into [data][13] folder, after that you can upload from Arduino IDE to device SPIFFS

See comments in both [create_spiffs.js][11] and [web_server.js][11] files, it's also indicated dependencies needed by nodejs.

Terminal Commands:
------------------
- connect : connect do target device
- help : show help

Commands once connected to remote device:
-----------------------------------------
- `!close` or CTRL-D : close connection
- `swap` swap ESP8266 UART pin between GPIO1/GPIO3 with GPIO15/GPIO13
- `ping` typing ping on terminal and ESP8266 will send back pong
- `?` or `help` show help
- `heap` show ESP8266 free RAM
- `whoami` show WebSocket client # we are
- `who` show all WebSocket clients connected
- `fw` show firmware date/time
- `baud n` set ESP8266 serial baud rate to n (to be compatble with device driven)
- `reset p` reset gpio pin number p
- `ls` list SPIFFS files
- `read file` execute SPIFFS file command


Every command in file `startup.ini` are executed in setup() you can chain with other files. 

I'm using this sketch to drive Microchip RN2483 Lora module to test LoraWan, see the [boards][8] I used.

For example my `startup.ini` file contains command to read microchip RN2483 config file named `rn2483.txt`

`startup.ini`
```sh
# Startup config file executed once in setup()
# commands prefixed by ! are executed by ESP
# all others passed to serial module

# Chain with Microchip Lora rn2483 configuration
!read /rn2483.txt

```

rn2483 configuration file for my [WeMos shield][8] `rn2483.txt`
```shell
# Startup config file for Microchip RN2483
# commands prefixed by ! are executed by ESP all others passed to serial module
# !delay is not executed when connected via browser web terminal (websocket)
# See schematics here https://github.com/hallard/WeMos-RN2483

# Set ESP Module serial speed (RN2483 is 57600)
!baud 57600
!delay 100

# reset RN2483 module (reset pin connected to ESP GPIO15)
!reset 15
!delay 1000

# Light on the LED on GPIO0
sys set pindig GPIO0 1
!delay 250

# Light on the LED on GPIO10
sys set pindig GPIO10 1
!delay 250
```

By the way I integrated the excellent @me-no-dev SPIFFS Web editor so you can direct edit configuration files of SPIFFS going to 
`http://ESP_IP/edit`
Your computer need to be connected to Internet (so may be your ESP8266 device) and authenticated for this feature, default login/pass are in the sketch (admin/admin)

See all in action    
http://cdn.rawgit.com/hallard/WebSocketToSerial/master/webdev/index.htm

Known Issues/Missing Features:
------------------------------
- More configuration features 
- Configuration file for SSID/PASSWORD and login/pass for http admin access

Dependencies
------------
- Arduino [ESP8266][6]
- @me-no-dev [ESPAsyncWebServer][4] library
- @me-no-dev [ESPAsyncTCP][5] library 
- [nodejs][7] for web pages development test 

Misc
----
See news and other projects on my [blog][2] 
 
[1]: http://www.wemos.cc/
[2]: https://hallard.me
[3]: http://terminal.jcubic.pl/
[4]: https://github.com/me-no-dev/ESPAsyncWebServer
[5]: https://github.com/me-no-dev/ESPAsyncTCP
[6]: https://github.com/esp8266/Arduino/blob/master/README.md
[7]: https://nodejs.org/
[8]: https://github.com/hallard/WeMos-RN2483/blob/master/README.md
[9]: http://cdn.rawgit.com/hallard/WebSocketToSerial/master/webdev/index.htm
[10]: https://github.com/hallard/WebSocketToSerial/tree/master/webdev
[11]: https://github.com/hallard/WebSocketToSerial/blob/master/webdev/create_spiffs.js
[12]: https://github.com/hallard/WebSocketToSerial/blob/master/webdev/web_server.js
[13]: https://github.com/hallard/WebSocketToSerial/tree/master/data
