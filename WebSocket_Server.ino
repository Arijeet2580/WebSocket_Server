#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#else 
#error "Board Not found
#endif

#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);//HTTP Protocol Port
#include <WebSocketsServer.h>
WebSocketsServer websockets(81); //TCP Protocol/WebSocket Port
//JSON parser
#include <ArduinoJson.h>
//Timer Library
#include <Ticker.h>
Ticker timer;
#include <DHT.h>
#define DHTPIN 4 //Digital Pin Connected to DHT Sensor
#define DHTTYPE DHT22

DHT dht(DHTPIN,DHTTYPE);

#define led 15
void notFound(AsyncWebServerRequest *request){
  request->send(404,"text/plain","404: PAGE NOT FOUND");
}
void websocketEvent(uint8_t num,WStype_t type, uint8_t *payload,size_t length)
{
  switch(type){
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected from server \n",num);
      break;
    case WStype_CONNECTED:{
      IPAddress ip = websockets.remoteIP(num);
      Serial.printf("[%u] Connected from %d,%d,%d,%d url: %s\n",num,ip[0],ip[1],ip[2],ip[3],payload);

      //send message to client
      websockets.sendTXT(num, "Connected to Server");
    }
    break;
    case WStype_TEXT:
      Serial.printf("[%u] get text: %s\n",num,payload);
      String message = String((char *)(payload));
      Serial.println(message);
      DynamicJsonDocument doc(200);
      //Deserialise the Data
      DeserializationError error = deserializeJson(doc,message);
      if(error){
        Serial.print("Failed to deserialise");
        Serial.println(error.c_str());
        return ;
      }
      //Parse the Data
      int LEDStatus=doc["LED"];
      digitalWrite(led,LEDStatus);
  }
}
char webpage[] PROGMEM =R"=====( 
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DASHBOARD</title>
    <style>
        body{
            display: flex;
            justify-content: center;
            height: 100vh;
            margin:10px;
            font-family: 'Courier New', Courier, monospace;
        }
        .container{
          text-align: center;
        }
        .circle{
            width: 100px;
            height: 100px;
            margin: 20px;
            margin-left: 35px;
            border-color: black;
            background-color:rgb(0, 128, 2);
            border-radius: 50%;
            margin-bottom: 20px;
        }
        .buttons button{
            margin: 0 10px;
            padding: 5px 10px;
            font-size: 16px;
            cursor: pointer;
            border-radius: 30px;
            font-family: 'Courier New', Courier, monospace;
            font-weight: bolder;
        }
    </style>
    <script>
      var connection = new WebSocket('ws://'+location.hostname+':81');
      var button_status = 0;
      function button_on(){
        button_status=1;
        circle.style.backgroundColor =rgb(128,0,0);
        send_data();
      }
      function button_off(){
        button_status=0;
        circle.style.backgroundColor =rgb(0, 128, 2);
        send_data();
      }
      function send_data(){
        var data = '{"LED":'+button_status+'}';
        connection.send(data);
      }
      connection.onmessage = function(event){
        var fulldata=event.data;
        console.log(fulldata);
        var data =JSON.parse(fulldata);
        temp_data=data.temp;
        hum_data=data.hum;
        Document.getElementByID("temp_meter").value=temp_data;
        Document.getElementByID("temp_value").innerHTML=temp_data; 
        Document.getElementByID("hum_meter").value=hum_data;
        Document.getElementByID("hum_value").innerHTML=hum_data
      }
    </script>    
  </head>
  <body>
    <div class="container">
        <h1>DASHBOARD</h1>
        <div class="circle"></div>
        <div class="buttons">
            <button onclick="button_on()">ON</button>
            <button onclick="button_off()">OFF</button>
        </div>
        <h2>Temperature</h2>
        <meter value="50" min="0" max="100" id="temp_meter"></meter>
        <p id="temp_value">50<span>°C</span></p>
        <h2>Humidity</h2>
        <meter value="50" min="0" max="100" id="hum_meter"></meter>
        <p id="hum_value">50<span>%</span></p>
    </div>
  </body>
</html>
)=====";
void setup()
{
  WiFi.softAP("Esp32","");
  Serial.print("IP:");
  Serial.println(WiFi.softAPIP());

  if(MDNS.begin("ESP")){
    Serial.println("MDNS responder Started");
    Serial.println("Your Local Domain Name: esp.local");
  }
  /*
    WE WILL BE USING SEND_P AS WE ARE USING WEBPAGE CHARACTER ARRAY AS PAYLOAD WHICH IS INSIDE FLASH 
    MEMORY DEFINED BY PROGMEM 
  */

  //HOMEPAGE
  server.on("/",[](AsyncWebServerRequest *request){
    request->send_P(200,"text/html",webpage);
  });
  //LED IS ON STATUS
  server.on("/led/on",[](AsyncWebServerRequest *request){
    digitalWrite(led,HIGH);
    request->send_P(200,"text/html",webpage);
  });
  //LED IS OFF status
  server.on("/led/off",[](AsyncWebServerRequest *request){
    digitalWrite(led,LOW);
    request->send_P(200,"text/html",webpage);
  });

  server.onNotFound(notFound);
  server.begin();
  websockets.begin();
  websockets.onEvent(websocketEvent);
  timer.attach(2,send_Sensor); //Calls the send_Sensor() function every 2seconds
}
void loop()
{
  websockets.loop();
}
String JSON_Data;
void send_Sensor(){
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if(isnan(t) ||isnan(h)){
    Serial.println("Failed to read DHT Sensor");
    return ;
  }
  JSON_Data ="{\"temp\":";
  JSON_Data +=t;
  JSON_Data +=",\"hum\":";
  JSON_Data +=h;
  JSON_Data +="}";
  Serial.println(JSON_Data);
  websockets.broadcastTXT(JSON_Data);
}