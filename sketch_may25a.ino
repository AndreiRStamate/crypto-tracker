// MD_MAX72XX library can be found at https://github.com/MajicDesigns/MD_MAX72XX

#include <ESP8266WiFi.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <sstream>
#include <string>

const std::unordered_map<std::string, std::string> tag_to_id{
  { "BTC", "bitcoin" },
  { "ADA", "cardano" },
  { "LUNA", "terra-luna" },
  { "XRP", "ripple" }
};

const std::unordered_map<std::string, std::string> tag_to_name{
  { "BTC", "Bitcoin" },
  { "ADA", "Cardano" },
  { "LUNA", "Terra" },
  { "XRP", "Ripple" }
};

struct cryptocurrency {
  std::string id;
  std::string tag;
  std::string name;
  float price;
  float change_24h;
};


// Turn on debug statements to the serial output
#define  DEBUG  1

#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// coingecko's fingerprint
const char *fingerprint  = "33 C5 7B 69 E6 3B 76 5C 39 3D F1 19 3B 17 68 B8 1B 0A 1F D9";

String payload = "{}";


#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define CLK_PIN   D5 // or SCK
#define DATA_PIN  D7 // or MOSI
#define CS_PIN    D8 // D10 pe placuta

// SOFTWARE SPI
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// WiFi login parameters - network name and password
const char* ssid = "5G Antenna";
const char* password = "CCPIPATRUCC42";

WiFiUDP ntpUDP;
// You may change the NTP server address to one of your choosing below.
NTPClient timeClient(ntpUDP, "0.uk.pool.ntp.org", 3600*3, 5 * 60 * 1000);


// WiFi Server object and parameters
WiFiServer server(80);

// Scrolling parameters
uint8_t frameDelay = 25;  // default frame delay value
textEffect_t  scrollEffect = PA_SCROLL_LEFT;

// Global message buffers shared by Wifi and Scrolling functions
#define BUF_SIZE  128
char curMessage[BUF_SIZE];
char newMessage[BUF_SIZE];
bool newMessageAvailable = false;

const char WebResponse[] = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";

const char WebPage[] =
"<!DOCTYPE html>" \
"<html>" \
"<head>" \
"<title>Crypto tracker</title>" \
"<h3>" \
"Ids are available at https://docs.google.com/spreadsheets/d/1wTTuxXt8n9q7C4NDXqQpI3wpKu1_5bGVmP9Xz0XGSyU/edit#gid=0"
"</h3>" \
"<script>" \
"strLine = \"\";" \

"function SendData()" \
"{" \
"  nocache = \"/&nocache=\" + Math.random() * 1000000;" \
"  var request = new XMLHttpRequest();" \
"  strLine = \"&MSG=\" + document.getElementById(\"data_form\").Message.value;" \
"  strLine = strLine + \"/&SD=\" + document.getElementById(\"data_form\").ScrollType.value;" \
"  strLine = strLine + \"/&I=\" + document.getElementById(\"data_form\").Invert.value;" \
"  strLine = strLine + \"/&SP=\" + document.getElementById(\"data_form\").Speed.value;" \
"  request.open(\"GET\", strLine + nocache, false);" \
"  request.send(null);" \
"}" \
"</script>" \
"</head>" \

"<body>" \
"<p><b>Crypto Tracker 1.0.0</b></p>" \

"<form id=\"data_form\" name=\"frmText\">" \
"<label>List your comma separated crypto currencies (ex: bitcoin,ripple): <br><input type=\"text\" name=\"Message\" maxlength=\"255\"></label>" \
"<br><br>" \
"<input type = \"radio\" name = \"Invert\" value = \"0\" checked> Normal" \
"<input type = \"radio\" name = \"Invert\" value = \"1\"> Inverse" \
"<br>" \
"<input type = \"radio\" name = \"ScrollType\" value = \"L\" checked> Left Scroll" \
"<input type = \"radio\" name = \"ScrollType\" value = \"R\"> Right Scroll" \
"<br><br>" \
"<label>Speed:<br>Fast<input type=\"range\" name=\"Speed\"min=\"10\" max=\"200\">Slow"\
"<br>" \
"</form>" \
"<br>" \
"<input type=\"submit\" value=\"Send Data\" onclick=\"SendData()\">" \
"</body>" \
"</html>";

std::vector<cryptocurrency> tokens;

std::string root = "https://api.coingecko.com/api/v3/simple/price";
std::string CG_Dynamic_URL;

const char *err2Str(wl_status_t code)
{
  switch (code)
  {
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("CONNECT_FAILED"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}

uint8_t htoi(char c)
{
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'A') && (c <= 'F')) return(c - 'A' + 0xa);
  return(0);
}

std::string id(std::string tag) {
  std::string aux = tag;
  for (int i = 0; i < aux.size(); i++) aux[i] = isalpha(aux[i]) ? toupper(aux[i]) : aux[i];
  
  if (tag_to_id.find(aux) == tag_to_id.end()) {
    return tag;
  }
  return tag_to_id.at(aux);
}

std::string name(std::string tag) {
  std::string aux = tag;
  for (int i = 0; i < aux.size(); i++) aux[i] = isalpha(aux[i]) ? toupper(aux[i]) : aux[i];
  
  if (tag_to_name.find(aux) == tag_to_name.end()) {
    return tag;
  }
  return tag_to_name.at(aux);
}

void getData(char *szMesg, uint16_t len)
// Message may contain data for:
// New text (/&MSG=)
// Scroll direction (/&SD=)
// Invert (/&I=)
// Speed (/&SP=)
{
  char *pStart, *pEnd;      // pointer to start and end of text

  // check text message
  pStart = strstr(szMesg, "/&MSG=");
  if (pStart != NULL)
  {
    char *psz = newMessage;

    pStart += 6;  // skip to start of data
    pEnd = strstr(pStart, "/&");

    if (pEnd != NULL)
    {
      while (pStart != pEnd)
      {
        if ((*pStart == '%') && isxdigit(*(pStart + 1)))
        {
          // replace %xx hex code with the ASCII character
          char c = 0;
          pStart++;
          c += (htoi(*pStart++) << 4);
          c += htoi(*pStart++);
          *psz++ = c;
        }
        else
          *psz++ = *pStart++;
      }

      *psz = '\0'; // terminate the string

      std::string input(newMessage);
      std::istringstream ss(input);
      std::string tag;

      tokens.clear();
      tokens.reserve(5);
      while(std::getline(ss, tag, ',')) {
        tokens.push_back({id(tag), tag, name(tag), 0, 0});
      }
      
      //PRINTS("\nTOKENS");
      for (auto t : tokens) {
        PRINT("\nToken2: ", t.name.c_str());
      }
      
      newMessageAvailable = (strlen(newMessage) != 0);
      PRINT("\nNew Msg: ", newMessage);
      
      handleGecko();
    }
  }

  // check scroll direction
  pStart = strstr(szMesg, "/&SD=");
  if (pStart != NULL)
  {
    pStart += 5;  // skip to start of data

    //PRINT("\nScroll direction: ", *pStart);
    if (*pStart == 'R') {
      scrollEffect = PA_SCROLL_RIGHT;
    } else if (*pStart == 'L') {
      scrollEffect = PA_SCROLL_LEFT;
    }
    P.setTextEffect(scrollEffect, scrollEffect);
    P.displayReset();
  }

  // check invert
  pStart = strstr(szMesg, "/&I=");
  if (pStart != NULL)
  {
    pStart += 4;  // skip to start of data

//    PRINT("\nInvert mode: ", *pStart);
    P.setInvert(*pStart == '1');
  }

  // check speed
  pStart = strstr(szMesg, "/&SP=");
  if (pStart != NULL)
  {
    pStart += 5;  // skip to start of data

    int16_t speed = atoi(pStart);
//    PRINT("\nSpeed: ", P.getSpeed());
    P.setSpeed(speed);
    frameDelay = speed;
  }

  
}

void handleWiFi(void)
{
  static enum { S_IDLE, S_WAIT_CONN, S_READ, S_EXTRACT, S_RESPONSE, S_DISCONN } state = S_IDLE;
  static char szBuf[1024];
  static uint16_t idxBuf = 0;
  static WiFiClient client;
  static uint32_t timeStart;

  switch (state)
  {
  case S_IDLE:   // initialise
    //PRINTS("\nS_IDLE");
    idxBuf = 0;
    state = S_WAIT_CONN;
    break;

  case S_WAIT_CONN:   // waiting for connection
  {
    client = server.available();
    if (!client) break;
    if (!client.connected()) break;

    timeStart = millis();
    state = S_READ;
  }
  break;

  case S_READ: // get the first line of data
    //PRINTS("\nS_READ ");

    while (client.available())
    {
      char c = client.read();

      if ((c == '\r') || (c == '\n'))
      {
        szBuf[idxBuf] = '\0';
        client.flush();
        //PRINT("\nRecv: ", szBuf);
        state = S_EXTRACT;
      }
      else
        szBuf[idxBuf++] = (char)c;
    }
    if (millis() - timeStart > 1000)
    {
      //PRINTS("\nWait timeout");
      state = S_DISCONN;
    }
    break;

  case S_EXTRACT: // extract data
    //PRINTS("\nS_EXTRACT");
    // Extract the string from the message if there is one
    getData(szBuf, BUF_SIZE);
    state = S_RESPONSE;
    break;

  case S_RESPONSE: // send the response to the client
    //PRINTS("\nS_RESPONSE");
    // Return the response to the client (web page)
    client.print(WebResponse);
    client.print(WebPage);
    state = S_DISCONN;
    break;

  case S_DISCONN: // disconnect client
    //PRINTS("\nS_DISCONN");
    client.flush();
    client.stop();
    state = S_IDLE;
    break;

  default:  state = S_IDLE;
  }
}

void handleGecko(void) {
  // Check WiFi Status
  if (1){
    if (WiFi.status() == WL_CONNECTED)
    {
      // Instanciate Secure HTTP communication
      WiFiClientSecure client;
      client.setFingerprint(fingerprint);
      HTTPClient http;  //Object of class HTTPClient
  
      delay (3000);
      payload = "{}";
  
      // Send Coingecko query URL
      CG_Dynamic_URL = root;
      CG_Dynamic_URL += "?ids=";
      for (auto t : tokens) {
        CG_Dynamic_URL += t.id;
        CG_Dynamic_URL += "%2C";
      }
      CG_Dynamic_URL.pop_back();
      CG_Dynamic_URL.pop_back();
      CG_Dynamic_URL.pop_back();
      CG_Dynamic_URL += "&vs_currencies=usd&include_24hr_change=true";
      
      http.begin(client, CG_Dynamic_URL.c_str());
      Serial.println();
      Serial.print("Coingecko URL - ");
      Serial.println(CG_Dynamic_URL.c_str());
  
      // Get response code in order to decide if API determined request as valid.
      int httpCode = http.GET();
      delay(100);
      Serial.print("Site Response : ");
      Serial.println(httpCode);
      delay(100);
      // Check the returning code
      if (httpCode > 0) {
        // Get the request response payload (Price data)
        payload = http.getString();
  
        // Parsing
        Serial.print("Json String - ");
        Serial.println(payload);
  
        StaticJsonDocument<256> doc;
  
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          delay(5000);
          return;
        }
  
        for (int i = 0; i < tokens.size(); i++) {
          JsonObject token = doc[tokens[i].id];
          tokens[i].price = token["usd"];
          tokens[i].change_24h = token["usd_24h_change"];
        }
  
        std::string aux;
        for (auto [id, tag, name, price, price_change] : tokens) {
          aux += name;
          aux += ": ";
          
          char pricebuff[32];
          sprintf(pricebuff, "%.3f", price);
          aux += std::string(pricebuff);
          aux += " ";
          
          sprintf(pricebuff, "%.3f", price_change);
          aux += std::string(pricebuff);
          aux += "% ";

          
        }
        Serial.println(aux.c_str());
        strcpy(curMessage, aux.c_str());
      }
      http.end();   //Close connection
    }
  }
}

void setup()
{
  Serial.begin(19200);
  
//  PRINTS("\n[MD_Parola WiFi Message Display]\nType a message for the scrolling display from your internet browser");
  
  P.begin();
  
  P.setIntensity(0);
  
  P.displayClear();
  P.displaySuspend(false);

  P.displayScroll(curMessage, PA_LEFT, scrollEffect, frameDelay);

  curMessage[0] = newMessage[0] = '\0';

  // Connect to and initialise WiFi network
//  PRINT("\nConnecting to ", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    //PRINT("\n", err2Str(WiFi.status()));
    delay(500);
  }
  
  PRINTS("\nWiFi connected");

  // Start the server
  server.begin();
  PRINTS("\nServer started");

  // Set up first message as the IP address
  sprintf(curMessage, "%03d:%03d:%03d:%03d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  PRINT("\nAssigned IP ", curMessage);

  timeClient.begin();
  timeClient.update();
}

void loop()
{
  if (timeClient.update()) { // runs every 5 minutes
    handleGecko();
  }
  
  handleWiFi();
  
  if (P.displayAnimate())
  {
    P.displayReset();
  }
}
