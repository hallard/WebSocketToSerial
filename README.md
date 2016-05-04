Transparent TCP Network to Serial Proxy using WebSocket for ESP8266
===================================================================

This is a pure transparent bridge between Wifi and serial using any ESP8266 device. It's very useful for debugging or talking to remote serial device that have no network connection.

I'm using it on WeMos target, you can find more information on WeMos on their [site][1], it's really well documented.
I now use WeMos boards instead of NodeMCU's one because they're just smaller, features remains the same, but I also suspect WeMos regulator far better quality than the one used on NodeMCU that are just fake of originals AMS117 3V3.

This project is mainly based on excellent me-no-dev [ESPAsyncWebServer][4] library and great [JQuery Terminal][3] done by Jakub Jankiewicz.

Documentation
=============

Once uploaded SPIFFS data (web page) you can connect with a browser to http://ip_of_esp8266 and start playing with it.
The main index.htm web page include a full javascript terminal so you can type command and receive response.

The main web page can also be hosted anywhere and it's not mandatory to have it on the device (except if device and your computer have no access on Internet). I've published the fully fonctionnal WEB page from github so you can access it from [here][5] and then connect to your device on wich you flashed the firmware.

Some commands will be interpreted by the target (ESP8266) and not passed to serial, so you can interact with ESP8266 doing some variable stuff.

Terminal Commands:
--------------------------
- connect : connect do target device
- help : show help

Commands once connected to remote device:
-----------------------------------------
- swap : swap ESP8266 UART pin between GPIO1/GPIO3 with GPIO13/GPIO14
- ping : typing ping on terminal and ESP8266 will send back pong
- !close : close connection

Known Issues/Missing Features:
------------------------------
- More configuration features (UART speed/configuration)

Misc
----
See news and other projects on my [blog][2] 
 
[1]: http://www.wemos.cc/
[2]: https://hallard.me
[3]: http://terminal.jcubic.pl/
[4]: https://github.com/me-no-dev/ESPAsyncWebServer
[5]: https://cdn.rawgit.com/hallard/WebSocketToSerial/master/webdev/index.htm

