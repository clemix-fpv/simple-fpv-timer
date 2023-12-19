// SPDX-License-Identifier: GPL-3.0+

#include <Arduino.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "logging.hpp"
#include "osd.hpp"
#include "config.hpp"


const int slaveSelectPin = SS; // Setup data pins for rx5808 comms
const int spiDataPin = MOSI;
const int spiClockPin = SCK;
const int rssiPin = 34;

const int wifi_channel = 1;
const char *wifi_ssid = "simple-FPV-timer";
const char *wifi_hostname = "simple-FPV-timer";
const uint8_t zero_mac[6] = {0,0,0,0,0,0};


// This seems to need to be global, as per this page,
// otherwise we get errors about invalid peer:
// https://rntlap.com/question/espnow-peer-interface-is-invalid/
esp_now_peer_info_t peerInfo;
OSD osd;
Config cfg;


struct lap_s {
    int id;
    int rssi;
    unsigned long duration_ms;
    unsigned long abs_time_ms;
};

struct player_s {
#define MAX_NAME_LEN 32
#define MAX_LAPS 16
    char name[MAX_NAME_LEN];
    struct lap_s laps[MAX_LAPS];
    int next_idx;
    IPAddress ipaddr;
};

// This contains all needed data to determine lap time
struct lap_counter_s {
    int rssi_raw; 
    int rssi_smoothed;

    int rssi_peak;
    int rssi_enter;
    int rssi_leave;

    bool drone_in_gate;
    int in_gate_peak_rssi;
    unsigned long in_gate_peak_millis;

    bool in_calib_mode;
    int in_calib_lap_count;

#define MAX_PLAYER 8
    int num_player;
    struct player_s players[MAX_PLAYER];

} lc;

const uint8_t DNS_PORT = 53; 
DNSServer dnsServer;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws/rssi");


struct player_s *get_or_create_player(IPAddress *ip, const char *name)
{
    struct player_s *player;
    for (int i=1; i < MAX_PLAYER; i++) {
        if (lc.players[i].ipaddr == *ip){
            return &lc.players[i];
        }
    }

    for (int i=1; i < MAX_PLAYER; i++) {
        if (strlen(lc.players[i].name) != 0)
            continue;

        player = &lc.players[i];
        strncpy(player->name, name, MAX_NAME_LEN);
        player->name[MAX_NAME_LEN-1] = 0;
        player->ipaddr = *ip;
        return player;
    }
    return NULL;
}

struct lap_s* set_player_lap(struct player_s *player, 
        int id, int rssi, unsigned long duration, unsigned long abs_time)
{
    struct lap_s *lap;

    lap = &player->laps[player->next_idx % MAX_LAPS];
    player->next_idx++;

    DBGLN("ADD Lap 4 player:%s id:%d next_idx:%d", player->name, id, player->next_idx);

    lap->id = id > 0 ? id : player->next_idx;
    lap->rssi = rssi;
    lap->duration_ms = duration;
    lap->abs_time_ms = abs_time;

    return lap;

}


struct lap_s* set_player_lap(IPAddress *ip, const char *name, int id, int rssi, unsigned long duration, unsigned long abs_time)
{
    int i;
    struct player_s *player = NULL;
    player = get_or_create_player(ip, name);

    if (player == NULL) {
        DBGLN("Unable to add new player: %s", name);
        return NULL;
    }

    return set_player_lap(player, id, rssi, duration, abs_time);
}

// Calculate rx5808 register hex value for given frequency in MHz
uint16_t freqMhzToRegVal(uint16_t freqInMhz) {
    uint16_t tf, N, A;
    tf = (freqInMhz - 479) / 2;
    N = tf / 32;
    A = tf % 32;
    return (N << 7) + A;
}

// Functions for the rx5808 module
void SERIAL_SENDBIT1() {
    digitalWrite(spiClockPin, LOW);
    delayMicroseconds(300);
    digitalWrite(spiDataPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(spiClockPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(spiClockPin, LOW);
    delayMicroseconds(300);
}
void SERIAL_SENDBIT0() {
    digitalWrite(spiClockPin, LOW);
    delayMicroseconds(300);
    digitalWrite(spiDataPin, LOW);
    delayMicroseconds(300);
    digitalWrite(spiClockPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(spiClockPin, LOW);
    delayMicroseconds(300);
}
void SERIAL_ENABLE_LOW() {
    delayMicroseconds(100);
    digitalWrite(slaveSelectPin, LOW);
    delayMicroseconds(100);
}
void SERIAL_ENABLE_HIGH() {
    delayMicroseconds(100);
    digitalWrite(slaveSelectPin, HIGH);
    delayMicroseconds(100);
}

// Set the frequency given on the rx5808 module
void setRxModule(int frequency) {
    uint8_t i;

    // Get the hex value to send to the rx module
    uint16_t vtxHex = freqMhzToRegVal(frequency);

    DBGLN("Setup rx5808 frequency to: %d => 0x%04.hX", frequency, vtxHex);

    // bit bash out 25 bits of data / Order: A0-3, !R/W, D0-D19 / A0=0, A1=0,
    // A2=0, A3=1, RW=0, D0-19=0
    SERIAL_ENABLE_HIGH();
    delay(2);
    SERIAL_ENABLE_LOW();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT1();
    SERIAL_SENDBIT0();

    for (i = 20; i > 0; i--)
        SERIAL_SENDBIT0(); // Remaining zeros

    SERIAL_ENABLE_HIGH(); // Clock the data in
    delay(2);
    SERIAL_ENABLE_LOW();

    // Second is the channel data from the lookup table, 20 bytes of register data
    // are sent, but the MSB 4 bits are zeros register address = 0x1, write,
    // data0-15=vtxHex data15-19=0x0
    SERIAL_ENABLE_HIGH();
    SERIAL_ENABLE_LOW();

    SERIAL_SENDBIT1(); // Register 0x1
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();

    SERIAL_SENDBIT1(); // Write to register

    // D0-D15, note: loop runs backwards as more efficent on AVR
    for (i = 16; i > 0; i--) {
        if (vtxHex & 0x1) { // Is bit high or low?
            SERIAL_SENDBIT1();
        } else {
            SERIAL_SENDBIT0();
        }
        vtxHex >>= 1; // Shift bits along to check the next one
    }

    for (i = 4; i > 0; i--) // Remaining D16-D19
        SERIAL_SENDBIT0();

    SERIAL_ENABLE_HIGH(); // Finished clocking data in
    delay(2);

    digitalWrite(slaveSelectPin, LOW);
    digitalWrite(spiClockPin, LOW);
    digitalWrite(spiDataPin, LOW);

    DBGLN("rx5808 set.");
}

// Read the RSSI value for the current channel
int rssiRead() { 
    return analogRead(rssiPin); 
}

void http_send_api_data_to(const String &api_method, const String &msg, const IPAddress &ipaddr)
{
    WiFiClient client;
    HTTPClient http;

    String serverPath = String("http://") + ipaddr.toString() + "/api/v1/" + api_method;
    http.begin(serverPath.c_str());
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(msg.c_str());
    DBGLN("Send message to %s => HTTPResponse code: %d", 
            serverPath.c_str(), httpResponseCode);
    DBGLN("  %s", msg.c_str());
    http.end();
}

void http_send_api_data(const String &api_method, const String &msg)
{
    http_send_api_data_to(api_method, msg, WiFi.gatewayIP());
}

void init_wifi() {

    if (memcmp(zero_mac, cfg.eeprom.elrs_uid, 6) != 0) {
        // MAC must be unicast, so unset multicase bit
        cfg.eeprom.elrs_uid[0] = cfg.eeprom.elrs_uid[0] & ~0x01;

        // TODO is this really needed!!
        WiFi.mode(WIFI_STA);
        WiFi.begin("network-name", "pass-to-network", 1);
        WiFi.disconnect();

        DBGLN("Set softMAC to bind bytes: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                cfg.eeprom.elrs_uid[0],
                cfg.eeprom.elrs_uid[1],
                cfg.eeprom.elrs_uid[2],
                cfg.eeprom.elrs_uid[3],
                cfg.eeprom.elrs_uid[4],
                cfg.eeprom.elrs_uid[5]
                );
        // Soft-set the MAC address to the passphrase UID for binding
        esp_wifi_set_mac(WIFI_IF_STA, cfg.eeprom.elrs_uid);
    }

    if (cfg.eeprom.wifi_mode == CFG_WIFI_STA) {
        WiFi.mode(WIFI_STA);
        if (strlen(cfg.eeprom.passphrase) > 0) {
            WiFi.begin(cfg.eeprom.ssid, cfg.eeprom.passphrase);
        } else {
            WiFi.begin(cfg.eeprom.ssid);
        }

        //check wi-fi is connected to wi-fi network
        DBGLN("Wait for WIFI_STA connection to: %s", cfg.eeprom.ssid);
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            DBG(".");
            if (millis() - start  > 1000 * 60 *  1) {
                DBG("Switch back to AP MODE!");
                cfg.eeprom.wifi_mode = CFG_WIFI_AP;
                cfg.save();
                delay(1000);
                ESP.restart();
            }
        }
        IPAddress ip;
        ip = WiFi.localIP();
        DBGLN("LocalIP: %s", ip.toString().c_str());
        ip = WiFi.gatewayIP();
        DBGLN("Gateway: %s", ip.toString().c_str());

        if (memcmp(zero_mac, cfg.eeprom.elrs_uid, 6) != 0) {

            if (esp_now_init() != 0) {
                DBGLN("Error initializing ESP-NOW");
            }

            memcpy(peerInfo.peer_addr, cfg.eeprom.elrs_uid, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (esp_now_add_peer(&peerInfo) != ESP_OK) {
                DBGLN("ESP-NOW failed to add peer");
                return;
            }
            DBGLN("ESP-NOW setup complete");
        }

        http_send_api_data("connect", String("{\"player\": \"")+cfg.eeprom.player_name+String("\" }")) ;

    } else {

        WiFi.mode(WIFI_AP_STA);

        IPAddress local_IP(10,0,0,1);
        IPAddress gateway(10,0,0,1);
        IPAddress subnet(255,255,255,0);
        WiFi.softAPConfig(local_IP, gateway, subnet);

        if (strlen(cfg.eeprom.ssid) == 0) {
            snprintf(cfg.eeprom.ssid, 32, "%s-%02lx", wifi_ssid, random(256));
            cfg.save();
        }

        bool result;
        if (strlen(cfg.eeprom.passphrase) > 0) {
            result = WiFi.softAP(cfg.eeprom.ssid, cfg.eeprom.passphrase, wifi_channel, 0);
        } else {
            result = WiFi.softAP(cfg.eeprom.ssid);
        }

        if (!result) {
            DBGLN("AP Config failed.");
        } else {
            DBGLN("AP Config Success. Broadcasting with AP: %s/%s", cfg.eeprom.ssid, cfg.eeprom.passphrase);
        }

        if (memcmp(zero_mac, cfg.eeprom.elrs_uid, 6) != 0) {
            WiFi.disconnect();

            if (esp_now_init() != 0) {
                DBGLN("Error initializing ESP-NOW");
            }

            memcpy(peerInfo.peer_addr, cfg.eeprom.elrs_uid, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (esp_now_add_peer(&peerInfo) != ESP_OK) {
                DBGLN("ESP-NOW failed to add peer");
                return;
            }

            DBGLN("ESP-NOW setup complete");
        }
    }
}


boolean is_ip(String str) {
    for (size_t i = 0; i < str.length(); i++) {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9')) {
            return false;
        }
    }
    return true;
}

String toStringIp(IPAddress ip) {
    String res = "";
    for (int i = 0; i < 3; i++) {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}


static bool web_captive_portal(AsyncWebServerRequest *request)
{
    DBGLN("Host: %s ", String(request->host()).c_str());
    if (!is_ip(request->host()) && request->host() != (String(wifi_hostname) + ".local"))
    {
        DBGLN("Request redirected to captive portal");
        String url = String("http://") + toStringIp(request->client()->localIP());
        request->redirect(url);
        DBGLN(url.c_str());
        return true;
    }
    return false;
}


void web_handle_root(AsyncWebServerRequest *request)
{
    DBGLN("HandleRoot");
    if (web_captive_portal(request)) { 
        return;
    }

    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html.gz", "text/html");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    return;
#if 0
    String p;
    p += F(
            "<html><head></head><body>"
            "<h1>HELLO WORLD!!</h1>");
    p += F("</body></html>");
    DBGLN("HandleRoot send :%s", p.c_str());

    DBGLN("FOO");
    //  request->send(200,"text/html", p);
    //return;

    AsyncWebServerResponse *response;
    response = request->beginResponse(200, "text/html", p);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
#endif
}

void web_handle_static_gz_spiffs(AsyncWebServerRequest *request)
{
    String file = request->url();
    String type = "text/html";
    if (file.endsWith(".css"))
        type = "text/css";
    else if (file.endsWith(".js"))
        type = "text/javascript";

    file += ".gz";
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, file.c_str(), type.c_str());
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void web_handle_static_spiffs(AsyncWebServerRequest *request)
{
    String file = request->url();
    String type = "text/html";
    if (file.endsWith(".css"))
        type = "text/css";
    else if (file.endsWith(".js"))
        type = "text/javascript";
    else if (file.endsWith(".ogg"))
        type = "audio/ogg";

    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, file.c_str(), type.c_str());
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
}


void web_handle_not_found(AsyncWebServerRequest *request)
{
    DBGLN("handleNotFound");
    if (web_captive_portal(request)) { 
        return;
    }
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += request->url();
    message += F("\nMethod: ");
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += request->args();
    message += F("\n");

    for (uint8_t i = 0; i < request->args(); i++) {
        message += String(F(" ")) + request->argName(i) + F(": ") + request->arg(i) + F("\n");
    }

    DBGLN("HandleNotFound send: %s", message.c_str());
    AsyncWebServerResponse *response;
    response = request->beginResponse(404, "text/html", message);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
}

String lap2String(struct lap_s *lap)
{
    return String("{ \"id\":" + String(lap->id) + 
                ",\"duration\":") + String(lap->duration_ms) + 
                ",\"rssi\": " + String(lap->rssi) +
                ",\"abs_time\": " + String(lap->abs_time_ms) +
                "}";

}

void web_handle_GET_api_settings(AsyncWebServerRequest *request)
{
    int i,j;
    struct lap_s *lap;
    struct player_s *player;

    String json;
    json += "{";
    json += "\"config\":";
    json += config2json(&cfg.running).c_str();
    json += ",";
    json += "\"status\": {";
    json += String("\"rssi_smoothed\":") + String(lc.rssi_smoothed) + ",";
    json += String("\"rssi_raw\":") + String(lc.rssi_raw) + ",";
    json += String("\"rssi_peak\":") + String(lc.rssi_peak) + ",";
    json += String("\"rssi_enter\":") + String(lc.rssi_enter) + ",";
    json += String("\"rssi_leave\":") + String(lc.rssi_leave) + ",";
    json += String("\"drone_in_gate\":") + String(lc.drone_in_gate? "true": "false") + ",";
    json += String("\"in_calib_mode\":") + String(lc.in_calib_mode ? "true": "false") + ",";
    json += String("\"in_calib_lap_count\":") + String(lc.in_calib_lap_count) + ",";
    json += String("\"players\":[");

    for (i=0; i < MAX_PLAYER; i++) {
        player = &lc.players[i];
        if (i > 0 && strlen(player->name) == 0)
            continue;
        if (i > 0)
            json += ",";
        json += String(" { \"name\": \"") + String(player->name) + "\", ";
        json += String(" \"ipaddr\": \"") + String(player->ipaddr.toString()) + "\", ";
        json += "\"laps\":[";
        j = max(player->next_idx - MAX_LAPS, 0);
        for(; j < player->next_idx; j++){
            lap = &player->laps[j % MAX_LAPS];
            json += lap2String(lap);
            if (j < player->next_idx - 1)
                json += ",";
        }
        json += "]}";
    }
    json += "]";

    json += "}"; /* end status */
    json += "}";

    request->send(200, "text/json", json);
}

void request_send_ok(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void web_handle_POST_api_settings(AsyncWebServerRequest *request)
{

    AsyncWebParameter *p = NULL;

    for (int i = 0; i < request->params(); i++) {
        const char *name = request->getParam(i)->name().c_str(); 
        const char *value = request->getParam(i)->value().c_str();
        if (strcmp(name, "osd_format") == 0 && strlen(value) == 0){
            cfg.setParam(name, Config::DEFAULT_OSD_FORMAT);
        } else {
            cfg.setParam(name, value);
        }
    }

    // TODO load new config
    request_send_ok(request);
}

void web_handle_api_lap(AsyncWebServerRequest *request, JsonVariant &json) 
{
    JsonObject jsonObj = json.as<JsonObject>();
    const char *name;
    struct lap_s lap;
    memset(&lap, 0, sizeof(lap));
    IPAddress ip = request->client()->remoteIP();

    DBGLN("Got api/v1/lap from %s ", ip.toString().c_str());

    for (JsonPair kv : jsonObj) {
        if (kv.key() == "player"){
            name = kv.value().as<const char*>();
        } else if (kv.key() == "lap") {
            JsonObject jlap = kv.value().as<JsonObject>();
            if (    jlap["id"].isNull() ||
                    jlap["duration"].isNull() ||
                    jlap["rssi"].isNull()
                    )
                goto error;
            set_player_lap(
                    &ip,
                    name,
                    jlap["id"].as<int>(),
                    jlap["rssi"].as<int>(),
                    jlap["duration"].as<unsigned long>(),
                    millis()
                    );
        }
    }

    request_send_ok(request);
    return;
error:
    request->send(400, "text/json", "{ \"status\":\"error\"}");

}

void web_handle_api_connect(AsyncWebServerRequest *request, JsonVariant &json) 
{
    JsonObject jsonObj = json.as<JsonObject>();
    IPAddress ip = request->client()->remoteIP();

    if (!jsonObj["player"].isNull()){
        if (get_or_create_player(&ip, jsonObj["player"].as<const char*>())) {
            request_send_ok(request);
            return;
        }
    }
    
    request->send(500, "text/json", "{ \"status\":\"error\"}");
}

void web_handle_api_osd_msg(AsyncWebServerRequest *request, JsonVariant &json) 
{

    if (memcmp(zero_mac, cfg.eeprom.elrs_uid, 6) != 0) {
        JsonObject jsonObj = json.as<JsonObject>();
        const char *method = 
            jsonObj["method"].isNull() ? "display_text" : jsonObj["method"].as<const char *>();

        if (!strcmp("display_text", method) && 
                !jsonObj["text"].isNull() &&
                !jsonObj["x"].isNull() &&
                !jsonObj["y"].isNull() ){
            char b[64];
            snprintf(b, sizeof(b)-1, "%s", jsonObj["text"].as<const char*>());
            b[sizeof(b)-1] = 0;
            osd.display_text(jsonObj["x"].as<uint16_t>(), jsonObj["y"].as<uint16_t>(), b);
            request_send_ok(request);

        } else if(!strcmp("clear", method)){
            osd.send_clear();
            request_send_ok(request);

        } else if(!strcmp("display", method)){
            osd.send_display();
            request_send_ok(request);

        } else if(!strcmp("set_text", method)  && 
                !jsonObj["text"].isNull() &&
                !jsonObj["x"].isNull() &&
                !jsonObj["y"].isNull() ){
            char b[64];
            snprintf(b, sizeof(b)-1, "%s", jsonObj["text"].as<const char*>());
            b[sizeof(b)-1] = 0;
            osd.send_text(jsonObj["x"].as<uint16_t>(), jsonObj["y"].as<uint16_t>(), b);
            request_send_ok(request);

        } else if (!strcmp("test_format", method) &&
                !jsonObj["format"].isNull() ){
            char b[128];
            sprintf(b, "{\"status\":\"ok\", \"msg\":\"");
            int len = strlen(b);
            if (!osd.eval_format(jsonObj["format"].as<const char*>(), 
                        23, 1337, 666, b+len, sizeof(b)-len)){
                request->send(400, "text/json", 
                        "{ \"status\":\"error\", \"msg\":\"Invalid OSD message format!\"}");

            }else {
                len = strlen(b);
                snprintf(b+len, sizeof(b)-len, "\"}");
                request->send(200, "text/json", b);
            }
        } else {
            request->send(400, "text/json", "{ \"status\":\"error\", \"msg\":\"Unknown method\"}");
        }
    } else {
        request->send(400, "text/json", "{ \"status\":\"error\", \"msg\":\"Elrs uid not configured - maybe you need to save first!\"}");
    }
}

void web_handle_api_start_calibration(AsyncWebServerRequest *request)
{
    lc.in_calib_mode = true;
    lc.rssi_peak = 0;
    lc.rssi_enter = 0;
    lc.rssi_leave = 0;
    lc.drone_in_gate = 0;

    request_send_ok(request);
}

void web_handle_api_clear_laps(AsyncWebServerRequest *request)
{
    struct player_s *player;
    for (int i=0; i < MAX_PLAYER; i++) {
        player = &lc.players[i];
        memset(player->laps, 0, sizeof(player->laps));
        player->next_idx = 0;
        if (player->ipaddr != 0 && strlen(player->name) > 0){
            http_send_api_data_to("clear_laps", "", player->ipaddr);
        }
    }

    request_send_ok(request);
}

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
 void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
        //      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


void setup_server()
{
    const char *static_gz_files[] = {
        "/bootstrap.min.css",
        "/bootstrap.bundle.min.js",
        "/jquery.min.js",
        "/index.js",
        "/howler.min.js",
        "/index.html",
        "/uPlot.iife.min.js",
        "/uPlot.min.css",
        NULL
    };
    const char *static_files[] = {
        "/round.ogg",
        NULL
    };
    const char **f;

    if(!SPIFFS.begin()){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    /* Setup the DNS server redirecting all the domains to the apIP */
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());


    /* Setup the web server */
    server.on("/", web_handle_root);
    server.on("/generate_204", web_handle_root);

    for (f = static_gz_files; *f; f++)
        server.on(*f, web_handle_static_gz_spiffs);
    for (f = static_files; *f; f++)
        server.on(*f, web_handle_static_spiffs);

    server.on("/api/v1/settings", HTTP_GET, web_handle_GET_api_settings);
    server.on("/api/v1/settings", HTTP_POST, web_handle_POST_api_settings);
    server.on("/api/v1/start_calibration", web_handle_api_start_calibration);
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/v1/lap", web_handle_api_lap));
    server.on("/api/v1/clear_laps", web_handle_api_clear_laps);
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/v1/connect", web_handle_api_connect));
    server.addHandler(new AsyncCallbackJsonWebHandler("/api/v1/osd_msg", web_handle_api_osd_msg));
    server.onNotFound(web_handle_not_found);

    // init websockets
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);

    server.begin(); // Web server start
    DBGLN("HTTP server started");
}

void setup_lap_counter_stats()
{
    memset(&lc, 0, sizeof(lc));
    sprintf(lc.players[0].name,"%s", cfg.eeprom.player_name);
    lc.rssi_peak = cfg.eeprom.rssi_peak;
    lc.rssi_enter = lc.rssi_peak * (cfg.eeprom.rssi_offset_enter / 100.0f);
    lc.rssi_leave = lc.rssi_peak * (cfg.eeprom.rssi_offset_leave / 100.0f);
}

void setup()
{
    delay(3000);
    Serial.begin(115200);
    while (!Serial){}; // Wait for the Serial port to initialise
    DBGLN("START NOW!");
   
    cfg.setup();
    cfg.load();
    cfg.dump();

    // RX5808 comms.
    // SPI.begin();
    pinMode(slaveSelectPin, OUTPUT);
    pinMode(spiDataPin, OUTPUT);
    pinMode(spiClockPin, OUTPUT);

    digitalWrite(slaveSelectPin, HIGH);

    setRxModule(cfg.eeprom.freq);

    init_wifi();
    setup_server();

    cfg.setRunningCfg();

    osd.set_x(cfg.running.osd_x);
    osd.set_y(cfg.running.osd_y);
    osd.set_format(cfg.running.osd_format);
    osd.set_peer(cfg.running.elrs_uid);

    setup_lap_counter_stats();
    DBGLN("Setup done!");
}

struct lap_s * get_fastes_lap(struct player_s *player)
{

    struct lap_s *fastes = NULL, *lap;
    
    int i = max(player->next_idx - MAX_LAPS, 0);
    for(; i < player->next_idx; i++){
        lap = &player->laps[i % MAX_LAPS];
        if (fastes == NULL) {
            fastes = lap;
        } else {
            if (fastes->duration_ms > lap->duration_ms)
                fastes = lap;
        }
    }
    return fastes;
}

void on_drone_passed(int rssi, unsigned long abs_time_ms)
{
    static unsigned long last_lap_time = 0;

    if (lc.in_calib_mode) {
        lc.in_calib_lap_count ++;
        if (memcmp(zero_mac, cfg.eeprom.elrs_uid, 6) != 0) {
            char b[64];
            sprintf(b, "calib: %d/%d rssi:%d", lc.in_calib_lap_count,
                    cfg.running.calib_max_lap_count, lc.rssi_peak);
            osd.display_text(cfg.running.osd_x, cfg.running.osd_y, b);
        }
        if (lc.in_calib_lap_count >= cfg.running.calib_max_lap_count) {
            lc.in_calib_mode = false;
            cfg.eeprom.rssi_peak = lc.rssi_peak; 
        }
    } else {
        if (last_lap_time > 0) {
            struct lap_s *fastes = get_fastes_lap(&lc.players[0]);
            long fastes_duration = fastes ? fastes->duration_ms : 0;
            fastes = NULL;

            struct lap_s *lap = set_player_lap(&lc.players[0], 
                    -1, rssi, 
                    abs_time_ms - last_lap_time, 
                    abs_time_ms);

            DBGLN("LAP[%d]: %ldms rssi:%d", lap->id, lap->duration_ms, lap->rssi);
            
            if (memcmp(zero_mac, cfg.eeprom.elrs_uid, 6) != 0) {
                // SEND TIME TO GOGGLE
                long diff = (long)lap->duration_ms - fastes_duration;
                osd.send_lap(lap->id, lap->duration_ms, diff);
            }

            if (cfg.eeprom.wifi_mode == CFG_WIFI_STA && WiFi.status() == WL_CONNECTED) {
                http_send_api_data("lap", String("{\"player\":\"") + 
                        String(cfg.running.player_name) +
                        String("\",") +
                        String("\"lap\":") + lap2String(lap) + String("}") );
            }
        }
        last_lap_time = abs_time_ms;
    }
}

struct timer_s {
    unsigned long start;
    unsigned long duration;
    unsigned long end; /*  calculated end in milliseconds */
};

void timer_start(struct timer_s *t, unsigned long duration_ms)
{
    t->start = millis();
    t->duration = duration_ms;
    t->end = t->start + t->duration;
}

bool timer_over(struct timer_s *t, unsigned long *over_shoot)
{
    unsigned long ms = millis();
    if (t->end <= ms) {
        if (over_shoot)
            *over_shoot = ms - t->end;
        return true;
    }
    return false;
}

void loop()
{
    static unsigned long loop_cnt = 0;
    const int min_rssi_peak = 400;
    static struct timer_s timer_block_enter;
    static struct timer_s timer_hz_info = {.start = 0, .duration = 0, .end = 0};
    static struct timer_s timer_ws_send = {.start = 0, .duration = 0, .end = 0};
    unsigned long over_shoot;

    loop_cnt++;
    if (timer_over(&timer_hz_info, &over_shoot)) {
        DBGLN("%.3fMHz %ld free-bytes rssi:%d", 
                loop_cnt/(1000.0-over_shoot), esp_get_free_heap_size(), lc.rssi_smoothed);
        loop_cnt = 0;
        timer_start(&timer_hz_info, 1000-over_shoot);
    }
    
    if (timer_over(&timer_ws_send, &over_shoot)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"t\":%d,\"r\":%d,\"s\":%d,\"i\":%s}",
                millis(), lc.rssi_raw, lc.rssi_smoothed, lc.drone_in_gate ? "true": "false");

        ws.textAll(buf);
        timer_start(&timer_ws_send, 100);
    }

    dnsServer.processNextRequest();

    if (cfg.changed()) {
        cfg.save();
        delay(1000);
        ESP.restart();
    }

    lc.rssi_raw = rssiRead();

    float filter = cfg.running.rssi_filter / 100.0f;
    if (filter < 0.01) filter = 0.01;
    lc.rssi_smoothed = (filter * lc.rssi_raw) + ((1.0f - filter) * lc.rssi_smoothed);

    if( lc.in_calib_mode) {
        if (lc.rssi_smoothed > lc.rssi_peak && 
                lc.rssi_smoothed > cfg.running.calib_min_rssi_peak) {
            lc.rssi_peak = lc.rssi_smoothed;
            lc.rssi_enter = lc.rssi_peak * (cfg.eeprom.rssi_offset_enter / 100.0f);
            lc.rssi_leave = lc.rssi_peak * (cfg.eeprom.rssi_offset_leave / 100.0f);
            DBGLN("NEW rssi-PEAK: %d enter:%d leave:%d", 
                    lc.rssi_peak, lc.rssi_enter, lc.rssi_leave);

            lc.drone_in_gate = false;
            timer_start(&timer_block_enter, 1000);
        }
    }

    if (!lc.rssi_enter || !lc.rssi_leave)
        return;

    if (lc.rssi_enter < lc.rssi_smoothed && 
            timer_over(&timer_block_enter, NULL) &&
            !lc.drone_in_gate) {
        DBGLN("Drone enter gate! rssi: %d", lc.rssi_smoothed);
        timer_start(&timer_block_enter, 2000);
        lc.drone_in_gate = true;
        lc.in_gate_peak_rssi = lc.rssi_smoothed;
        lc.in_gate_peak_millis = millis();

    } else if (lc.drone_in_gate && 
            timer_over(&timer_block_enter, NULL) && 
            lc.rssi_leave > lc.rssi_smoothed) {
        lc.drone_in_gate = false;
        on_drone_passed(lc.in_gate_peak_rssi, lc.in_gate_peak_millis);
        timer_start(&timer_block_enter, 2000);

    } else if (lc.drone_in_gate) {
        if (lc.in_gate_peak_rssi < lc.rssi_smoothed) {
            lc.in_gate_peak_rssi = lc.rssi_smoothed;
            lc.in_gate_peak_millis = millis();
        }
    }
}
