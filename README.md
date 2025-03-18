# simple-fpv-timer
A simple FPV timer based on ESP32 + RX5808.
It is inspired by https://github.com/qdrk/fpvsim-timer and is using the same hardware setup to connect the RX5808. In addition we use GPIO2 to manage the WS2812 LED stribes.

<img src="https://github.com/clemix-fpv/simple-fpv-timer/blob/main/doc/images/circuit.png?raw=true" width="400px" />

This project is still under development!

# Features:
 * stand alone FPV timer
 * integrated HTTP frontend
 * show laps / best laps / best of players
 * display lap time in HDzero goggles OSD
 * audio signal on lap count
 * live signal strange view
 * race view, multiple FPV pilots in one frontend (each pilot needs it's own hardware)
 * WS2812 LED stripe support
 * Different "Game modes":
    * Race mode (measure your fastest laps)
    * CTF (Capture the flag), fly near LED-Stripes and "collect" them with your Team
    * Spectrum, simple RSSI values of multiple channels. Is using one RX5808 to hop from channel to channel and capture signal strength.

# Installation

This project is build with https://platformio.org/ with espidf Framework for ESP32. Once you have setup your development environment and your ESP32 is connected, run:
```
cd src
pio run -t menuconfig
pio run -t upload
pio device monitor
```
By default the esp32 will run an open AccessPoint and using a SSID like `simple-FPV-timer-XX`, where `XX` is some random generated string.
After connecting to the AP, the captive portal webpage should automatically be opened otherwise use `http://192.168.4.1` as address in your browser.

# Screenshots

<img src="https://github.com/clemix-fpv/simple-fpv-timer/blob/main/doc/images/webui_mobile_lap_view.png?raw=true" width="300px" />
<img src="https://github.com/clemix-fpv/simple-fpv-timer/blob/main/doc/images/webui_mobile_player_view.png?raw=true" width="300px" />
<img src="https://github.com/clemix-fpv/simple-fpv-timer/blob/main/doc/images/webui_mobile_config_view.png?raw=true" width="300px" />
<img src="https://github.com/clemix-fpv/simple-fpv-timer/blob/main/doc/images/webui_mobile_debug_rssi_graph.png?raw=true" width="300px" />
<img src="https://github.com/clemix-fpv/simple-fpv-timer/blob/main/doc/images/hdz_osd_timer.png?raw=true" width="90%" />

