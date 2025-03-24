

# Directory structure

 * `/tools`: Helper utilities like a http development server, reads data from `/src/data_src`

 * **Gui (javascript)**
   * `/src/js`: Typescript root with helper scripts for esbuild which transpile the typescript code to javascript and place it in `src/data_src/app.js`.
     ```
     cd ./src/src/js &&
     esbuild src/app.ts --bundle --outfile=../data_src/app.js --minify --watch --target=esnext --sourcemap
     ```
   * `/src/js/src`: Typescript sources for `/src/data_src`.
     * `app.ts`: The main script for ESP32 GUI.
     * `ctrld.ts`: The main script for standalone server.
     * `SimpleFpvTimer.ts`: Common sft gui logic.
     * `TimeSync.ts`: Used to sync time between ESP32 and GUI(web-browser)
     * `src/src/js/src/ctf/*`: CTF (capture the flag) specific GUI code.
     * `src/src/js/src/race/*`: Race mode specific GUI code.
     * `src/src/js/src/spectrum/*`: Spectrum analyzer specific GUI code.

 * **Standalone server**
 * `/src/server/ctrl-server.py`: Standalone python server to control various ESP32 SFT's.
   The javascript code is generated via `src/js/ctrl.ts` using ebuild:
   ```
   cd src/src/js &&
   esbuild src/ctrld.ts --bundle --outfile=../server/www/ctrld.js --minify --watch --target=esnext --sourcemap
   ```

 * **ESP32 firmware**
   * `/src/src/`: Contains all C-Code which will be flashed to the ESP32.
     * `main.c`:  entrypoint
     * `config.[ch]`: Configuration (load/save)
     * `captdns.[ch]`: Captivportal code (https://github.com/cornelis-61/esp32_Captdns),
        don't forget to by a beer for Jeroen Domburg!
     * `gui.[ch]`: This is the HTTP task. It handles incoming HTTPRequest and also holds
       the list of connected web-sockets.
         * `/src/src/data_src`: Contains the HTML/Javascript code which will be embedded into the
          firmware via `/src/src/static_files.h`, which is generated via `prepare_data_folder.py`
     * `jsmn.h, json.[ch]`: JSON encoding/decoding library
     * `osd.[ch]`: Lib to communicate with HDZero Goggles via ELRS backpack (ESP-Now)
       * `msp.[ch]`: The MSP package and utility functions for marshalling/de-marshalling
     * `rx5808.[ch]`: Lib to handle the rx5808 via SPI and reading RSSI via an ADC port
     * `simple_fpv_timer.[ch]`: Game logic, process SFT events and trigger communication
     * `task_led.[ch]`: The LED task, process SFT_LED events and control the ws2812 led stripes.
        * `led.[ch]`: LED (ws2812) wrapper for the led_strip component (`src/components/led_strip`)
     * `task_rssi.[ch]`: The only task on CPU 1, dedicated to continuously read the RSSI value from rx5808.
       Does only process and emit SFT events for communication with other components.
     * `timer.[ch]`: Simple legacy timer helper
     * `wifi.[ch]`: WIFI configuration helper
