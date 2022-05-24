#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>

WiFiMulti WiFiMulti;
SocketIOclient socketIO;

// Select the IP address according to your local network
// IPAddress clientIP(192, 168, 1, 112);

// Select the IP address according to your local network
//IPAddress serverIP(192, 168, 1, 221);
IPAddress serverIP(198, 251, 65, 118);//198.251.65.118
uint16_t serverPort = 5000;

int status = WL_IDLE_STATUS;

int socketIOconnect = true;

String comando = "";
int comando_by_socketio = false;
String Jsonurlnew = "";
int result_command = 0;
String resultcommand = "";
// StaticJsonDocument<156> doc;
DynamicJsonDocument docu(1024);

void socketIOEvent(const socketIOmessageType_t &type, uint8_t *payload, const size_t &length)
{
  switch (type)
  {
  case sIOtype_DISCONNECT:
    Serial.println("[IOc] Disconnected");
    socketIOconnect = false;
    // esp_task_wdt_reset();
    break;

  case sIOtype_CONNECT:
    Serial.print("[IOc] Connected to url: ");
    Serial.println((char *)payload);
    socketIOconnect = true;
    // join default namespace (no auto join in Socket.IO V3)
    socketIO.send(sIOtype_CONNECT, "/");

    break;

  case sIOtype_EVENT:
    Serial.print("[IOc] Get event: ");
    Serial.println((char *)payload);

    Jsonurlnew = String((char *)payload);
    Jsonurlnew.replace("[", "{");
    Jsonurlnew.replace(",", ":");
    Jsonurlnew.replace("]", "}");
    // Serial.println(Jsonurlnew);

    deserializeJson(docu, Jsonurlnew);
    comando = docu["chat message"].as<String>();
    Serial.println("Comando:" + comando);

    // strncpy(resultcommand,comando,2);//Copy to hour_form in theory to extract 5 chars
    resultcommand = comando.substring(0, comando.indexOf("--"));
    Serial.println("ComandoR:" + resultcommand);
    result_command = comando.indexOf("--");
    if (result_command >= 0)
    {
      // strcpy(comando,resultcommand);
      comando_by_socketio = true;
      Serial.println("COMANDO RECIVIDO:" + resultcommand);
    }

    // Serial.println(urlnew);
    break;

  case sIOtype_ACK:
    Serial.print("[IOc] Get ack: ");
    Serial.println(length);

    // hexdump(payload, length);

    break;

  case sIOtype_ERROR:
    Serial.print("[IOc] Get error: ");
    Serial.println(length);

    // hexdump(payload, length);

    break;

  case sIOtype_BINARY_EVENT:
    Serial.print("[IOc] Get binary: ");
    Serial.println(length);

    // hexdump(payload, length);

    break;

  case sIOtype_BINARY_ACK:
    Serial.print("[IOc] Get binary ack: ");
    Serial.println(length);

    // hexdump(payload, length);

    break;

  case sIOtype_PING:
    Serial.println("[IOc] Get PING");

    break;

  case sIOtype_PONG:
    Serial.println("[IOc] Get PONG");

    break;

  default:
    break;
  }
}

void socketio_monitor(String mensaje)
{
  // creat JSON message for Socket.IO (event)
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();

  // add evnet name
  // Hint: socket.on('event_name', ....
  array.add("event_name");

  // add payload (parameters) for the event
  JsonObject param1 = array.createNestedObject();
  param1["id"] = "JuanPabloII";
  // param1["now"]     = (uint32_t) now;
  param1["ESP32->"] = mensaje;

  // JSON to String (serializion)
  String output;
  serializeJson(doc, output);

  // Send event
  socketIO.sendEVENT(output);
}
