/************************************* BIBLIOTECAS **************************************/
//#include "ETH.h"
#if (ESP8266)
  #include <ESP8266WiFi.h>
#elif (ESP32)
  #include <WiFi.h>
#endif
#include <ArduinoJson.h>
#include "socket-io-lib.h"
#include "FS.h"
#include "SD_MMC.h"
#include <Wire.h>         // for I2C with RTC module
#include "RTClib.h"       //to show time
#include <AsyncHTTPRequest_Generic.h>           // https://github.com/khoih-prog/AsyncHTTPRequest_Generic

#include <esp_task_wdt.h>
#include "update_firmware.h"

//3 seconds WDT
#define WDT_TIMEOUT 3
/************************************** PIN OUT *****************************************/
#define BARRERA_ENTRADA 32
#define BARRERA_SALIDA  33
/************************************** VARIABLES ***************************************/
//Serial
struct SerialBuffer{
  volatile bool isNewMessage;
  volatile char tam;  //Tamaño del mensaje
  char message[64];
}serialBuffer = {false , 0};
//Archivos
#define FILE_PENSIONADOS     "/pensionados.txt"
#define FILE_SOPORTE         "/soporte.txt"
#define FILE_EVENTOS         "/eventos.txt"
#define FILE_ESTACIONAMIENTO "/estacionamiento.txt"
#define FILE_PASSBACK        "/passback.txt"
#define FILE_NIP_EVENT       "/nipEvent.txt"
//#define FILE_NIP             "/nip.txt"

//Async Request
AsyncHTTPRequest AccesoRequest;
AsyncHTTPRequest NipRequest;
AsyncHTTPRequest GetDataNubeRequest;
AsyncHTTPRequest LogRequest;


//Reloj
RTC_DS3231 rtc;

//Serial 2
HardwareSerial Serial3(2); //Para que no marque error por los demas seriales

/*****************************************************/

//const char host[] = "accesa.me";
const char host[] = "c68d9ab.online-server.cloud";
const char page[] = "POST /citas/php/dataloggerESP32.php HTTP/1.1\r\n";
const int port = 80;
bool requestAvailable = true;

#define SSID "RED ACCESA"
#define PASSWORD "037E32E7"

//Timer
hw_timer_t *timer = NULL; //Puntero null
hw_timer_t *timer1 = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
static char tempRele[2] = {0,0};

//Variables para resetear wifi/socket
volatile bool eth_connected = false;
volatile bool serverWaitReset = false;
char device[20];
//Variables para monitoriar funcionamiento
bool changeState = true;         //Cambio estado

//Tiempo
char cadTime[25];
char cadTimeAux[25];

//Datalogger
char evento[64];

//Dato nube
const char TIME_GET_DATA_NUBE = 3; //Segundos
volatile int tempNube = 0, tempNubeWait;                  //Temporizador nube
volatile bool isWaitNube = false, flagEventosTCP = false;
char dataNubeCont = 0;
char dataNube[128];
char dataNubeCmd[128];

//Crear Archivos de guardado
char idEstacionamiento[5] = "999";
bool isPassback = true;
char numAccessBoard[3] = "1";
//#define CREATE_INFO_CARD

//Envío periódico de eventos para log
static char enviopermitido = 0;

/***************************************************************************************/
void IRAM_ATTR timer1_Interrupt() {
  portENTER_CRITICAL(&timerMux);
    flagEventosTCP = true;
    tempNube++;
  portEXIT_CRITICAL(&timerMux);
  if( isWaitNube ) {
    portENTER_CRITICAL( &timerMux );
      tempNubeWait++;
    portEXIT_CRITICAL( &timerMux );
  }
}

void IRAM_ATTR timerInterrupt(){
  static int tempServer = 0;

  //Apagar el rele deseado
  if(digitalRead(BARRERA_ENTRADA) == HIGH){
    if(++tempRele[0] >= 5*1)
      digitalWrite(BARRERA_ENTRADA, LOW);  
  }
  if(digitalRead(BARRERA_SALIDA) == HIGH){
    if(++tempRele[1] >= 5*1)
      digitalWrite(BARRERA_SALIDA, LOW);  
  }
  
  //Detecta una conexion larga de desconexion
  if(serverWaitReset){
    //Mostrar datos
    if(tempServer % 4 == 0){
      Serial.print("Temporizado de reset: ");
      Serial.println(tempServer>>2);  
    }
    //Realizar reboot
    if(++tempServer >= 4*15){  //Cada minuto restablece el sistema
      ets_printf("ESP 32 Reboot Server\n");
      esp_restart();
    }  
  }else if(tempServer != 0){
    Serial.print("RRR reset: ");
    tempServer = 0;
  }
  
  //Reiniciar timer
  timerWrite(timer, 0);    //reset timer (feed watchdog)
  timerAlarmEnable(timer); //enable interrupt
}
/***************************************************************************************/
/********************************* ARDUINO INIT ****************************************/
/***************************************************************************************/
void setup(){
  //Configurar pines
  pinMode(BARRERA_ENTRADA, OUTPUT);
  pinMode(BARRERA_SALIDA, OUTPUT);
  digitalWrite(BARRERA_ENTRADA, LOW);
  digitalWrite(BARRERA_SALIDA, LOW);
  //Configura el serial
  Serial.begin(115200);                        //Serial Arduino
  Serial3.begin(115200, SERIAL_8N1, 36, 3);  //Baudios:115200, CONFIG, RX:16, TX:17
  //Configura el ethernet
  //WiFi.onEvent(WiFiEvent);
  //ETH.begin();
   WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi SSID: "); 
  Serial.println(SSID);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
 
  Serial.print("\nConnected to the WiFi network IP ESP32:");
  Serial.println(WiFi.localIP());

////////////////////////////////socket.io//////////////////////////////////////////////////////
  // setup

  //WiFiMulti.addAP(SSID, PASSWORD);

  // setReconnectInterval to 10s, new from v2.5.1 to avoid flooding server. Default is 0.5s
  socketIO.setReconnectInterval(10000);

  socketIO.setExtraHeaders("Authorization: 1234567891");

  // server address, port and URL
  // void begin(IPAddress host, uint16_t port, String url = "/socket.io/?EIO=4", String protocol = "arduino");
  // To use default EIO=4 from v2.5.1
  socketIO.begin(serverIP, serverPort);

  // event handler
  socketIO.onEvent(socketIOEvent);

  /////////////////////////////////////////////////////////
  /**************************NOMBRES PARA FUNCION DE ACTUALIZACION DE FIRMWARE*******************/
  mDashBegin(DEVICE_PASSWORD);
  Serial.println("Se inicio proceso de busqueda de firmware nuevo...");
  /////////////////////////////////////////////////////////
 
  //Inicializar SD CARD
  if(!SD_MMC.begin()){
    Serial.println("Card Mount Failed");
    return;
  }
  //Verificar la tarjeta
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD_MMC card attached");
    while(true);
  }
  //Mostrar tipo de SD card
  Serial.print("SD_MMC Card Type: ");
  if(cardType == CARD_MMC)
    Serial.println("MMC");
  else if(cardType == CARD_SD)
    Serial.println("SDSC");
  else if(cardType == CARD_SDHC)
    Serial.println("SDHC");
  else
    Serial.println("UNKNOWN");
  
  //Configura el reloj
  if (!Wire.begin(13, 16, 25000)){ //SDA, SCL, CLOCK
    Serial.println("Couldn't find RTC");
    return;
  }
  
  //Timer - .25 seg
  timer = timerBegin(0, 80, true);                  //timer 0, div 80 - 80MHz frecuencia Oscilador - 
  timerAttachInterrupt(timer, &timerInterrupt, true);  //attach callback
  timerAlarmWrite(timer, 0.250 * 1e6, false);           //Cada x segundos
  timerWrite(timer, 0);    //reset timer (feed watchdog)
  //Inicializar timer
  timerAlarmEnable(timer); //enable interrupt
  //Timer - 1 seg
  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &timer1_Interrupt, true);
  timerAlarmWrite(timer1, 1e6, true);
  timerAlarmEnable(timer1);
  //Crear archivos para las pensiones y nips
  #ifdef CREATE_INFO_CARD
  delay(1000);
  //Ajustar el reloj
  //rtc.adjust(DateTime(__DATE__, __TIME__));
  //Creamos directorio tarjetas
  sd_writeFile(SD_MMC, FILE_PENSIONADOS, "", FILE_WRITE);
  //Creamos tarjetas de soporte
  sd_writeFile(SD_MMC, FILE_SOPORTE, "00636958\n", FILE_WRITE);
  //Creamos archivo de eventos
  sd_writeFile(SD_MMC, FILE_EVENTOS, "", FILE_WRITE);
  //Creamos el id del estacionamiento
  sd_writeFile(SD_MMC, FILE_ESTACIONAMIENTO, "000", FILE_WRITE);
  //Creamos el passback on/off
  sd_writeFile(SD_MMC, FILE_PASSBACK, "1", FILE_WRITE);
  //Creamos directorio de nips
  sd_writeFile(SD_MMC, FILE_NIP_EVENT, "", FILE_WRITE);
  //sd_createDir(SD_MMC, "/Nips");
  //sd_writeFile(SD_MMC, FILE_NIP, "NIP\tVIG\n", FILE_WRITE);
  
  
  #endif
  //Mostrar datos de la SD
  sd_listDir(SD_MMC, "/", 0);
  //Tamaño de la tarjeta
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
  //Variables
  device[0] = device[19] = 0;
  dataNube[0] = dataNube[127] = 0;
  enviopermitido = 0;
  //Passback
  sd_readFilePassback();
  //Cargar el id estacionamiento
  sd_readFileEstacionamiento();

  //Configuracion solicitud asincrona
  AccesoRequest.setDebug( true );
  AccesoRequest.onReadyStateChange( AccesoCB );
  NipRequest.setDebug( true );
  NipRequest.onReadyStateChange( NipCB );
  GetDataNubeRequest.setDebug( true );
  GetDataNubeRequest.onReadyStateChange( GetDataNubeCB );
  
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  
}
/***************************************************************************************/
/********************************* ARDUINO LOOP ****************************************/
/***************************************************************************************/
void loop(){
  static int temp = 65535;
  socketIO.loop();
  serialEventData();
  getDataNube();
    if( eth_connected == 0 ){  //==1
      if(serialBuffer.isNewMessage){
        Serial.print("RS232: ");
        Serial.println( serialBuffer.message );
        char *message = serialBuffer.message;
        if( !strncmp("MUX", message, 3) ){
          if(!strncmp("-NIP:", &message[4], 5)){ //MUXA-NIP:2585
            if(strlen(&message[9]) == 4){  //Validamos el nip recepcionado
              String json;
              StaticJsonDocument<64> doc;
              doc["data"] = message;
              doc["estacionamiento"] = atoi( idEstacionamiento );
              serializeJson( doc, json );
              Serial.println("Dato a enviar");
              Serial.println(json);
              sendNipRequest( json );
              socketio_monitor( json );
              //envio_log();
            }else{  //Nip incompleto
            }
          }else if( !strncmp( "-IDX:", &message[4], 5 ) ){//Lectura de tarjeta/tag
            if( requestAvailable ){
              requestAvailable = false;
              String json; 
              StaticJsonDocument<64> doc;
              doc["data"] = message;
              doc["estacionamiento"] = atoi(idEstacionamiento);
              serializeJson( doc, json );
              Serial.println("Dato a enviar: ");
              Serial.println( json );
              sendAccesoRequest( json ); 
              socketio_monitor( json );
              //envio_log();
            }
          }
          
        }else{
          Serial.println("Comando desconocido");
          socketio_monitor( "Comando desconocido" );
        }
        serialBuffer.message[0] = 0;
        serialBuffer.isNewMessage = false;
        
        
      }
    }else{
      if(serialBuffer.isNewMessage){
        Serial.print("RS232: ");
        Serial.println(serialBuffer.message);
        //Realizar accion del comando
        if(control_acceso(serialBuffer.message, evento)){
          strcat(evento, "\n");  //Add
          envio_log();
        }else{
          Serial.println("Comando desconocido");
        }
        //Limpiar buffer
        serialBuffer.message[0] = 0;
        serialBuffer.isNewMessage = false;
      }
    }
    if (flagEventosTCP) {
     // Serial.println("Resetting WDT...");
      esp_task_wdt_reset();
      portENTER_CRITICAL(&timerMux);
        flagEventosTCP = false;
      portEXIT_CRITICAL(&timerMux);
    }

  //Tiempos
  delay(1);
  //Cada 250ms cambiar actividad de funcionamiento para wdt
  if(temp >= 250){
    if(!changeState)
      changeState = true;
  }
  
    //Mostrar actividad del esp32
  if(++temp >= 5000){
    temp = 0;
    //Enviamos por tcp
    eventos_tcp();
    //Vaciamos el archivo 
  }
  
}
/***************************************************************************************/
/***********************************  ACCESOS  *****************************************/
/***************************************************************************************/
void getDataNube(){
  static char estado = 0;
  char response[32], cad[32];
  char pos;
  char nameFile[64];
  
  //Si no hay internet sal
  if(!eth_connected){
    estado = 0;
    return;
  }
  //Si la solicitud de tiempo ha llegado mando solicitud
  if(tempNube >= TIME_GET_DATA_NUBE){
    if(enviarDatoAsync("DATA_GET_NEW")){
      //Mostrar datos obtenido
      if(strlen(dataNube)){
        Serial.print("Server response: ");
        Serial.println(dataNube);
         socketio_monitor( dataNube );
        //Validar comando
        strcpy(response, "leido");
        char *pch = strchr(dataNube, ',');
        if(pch != NULL){
          strncpy(dataNubeCmd ,dataNube, pch - dataNube);
          dataNubeCmd[pch - dataNube] = 0;
          Serial.print("ID: ");
          Serial.println(dataNubeCmd); 
          Serial.print("Data: ");
          Serial.println(&dataNube[pch - dataNube + 1]); 
          
          //Procesar comando
          pos = pch - dataNube + 1;
          if(!strncmp(&dataNube[pos], "SETDATE=", sizeof("SETDATE=")-1)){
            DateTime fecha = DateTime(&dataNube[pos+sizeof("SETDATE=")-1]);
            //Serial.println(&dataNube[pos+sizeof("SETDATE=")-1]);
            rtc.adjust(fecha);
            strcpy(response, "modificado");
          }else if(!strncmp(&dataNube[pos], "GETDATE", sizeof("GETDATE")-1)){
            getDate(false);  //Obtengo la hora del evento
            strcpy(response, cadTime);
          }else if(!strncmp(&dataNube[pos], "PENREG=", sizeof("PENREG=")-1)){
            dataNube[pos+sizeof("PENREG=")-1 + 8] = '\t';
            dataNube[pos+sizeof("PENREG=")-1 + 10] = '\t';
            strcat(dataNube, "\n");
            if(sd_writeFile(SD_MMC, FILE_PENSIONADOS, &dataNube[pos+sizeof("PENREG=")-1], FILE_APPEND))
              strcpy(response, "registrado");
            else
              strcpy(response, "errorFile");
          }else if(!strncmp(&dataNube[pos], "PENVIGRST=", sizeof("PENVIGRST=")-1)){
            sd_updateFilePensionados("", true, dataNube[pos+sizeof("PENVIGRST=")-1]);
          }else if(!strncmp(&dataNube[pos], "PENVIG=", sizeof("PENVIG=")-1)){
            strncpy(cad, &dataNube[pos+sizeof("PENVIG=")-1], 8);
            sd_updateFilePensionados(cad, true, dataNube[pos+sizeof("PENVIG=")-1+9]);
          }else if(!strncmp(&dataNube[pos], "PENPASRST=", sizeof("PENPASRST=")-1)){
            sd_updateFilePensionados("", false, dataNube[pos+sizeof("PENPASRST=")-1]);
          }else if(!strncmp(&dataNube[pos], "PENPAS=", sizeof("PENPAS=")-1)){
            strncpy(cad, &dataNube[pos+sizeof("PENPAS=")-1], 8);
            sd_updateFilePensionados(cad, false, dataNube[pos+sizeof("PENVIG=")-1+9]);
          }else if(!strncmp(&dataNube[pos], "SOPREG=", sizeof("SOPREG=")-1)){
            strcat(dataNube, "\n");
            if(sd_writeFile(SD_MMC, FILE_SOPORTE, &dataNube[pos+sizeof("SOPREG=")-1], FILE_APPEND))
              strcpy(response, "registrado");
            else
              strcpy(response, "errorFile");
          }else if(!strncmp(&dataNube[pos], "PASSET=", sizeof("PASSET=")-1)){
            isPassback = (dataNube[pos+sizeof("PASSET=")-1] == '1')? true:false;
            //Guardar passback
            sd_writeFile(SD_MMC, FILE_PASSBACK, isPassback?"1":"0", FILE_WRITE);
            //Mostrar estado
            if(isPassback)
              strcpy(response, "passbackON");
            else
              strcpy(response, "passbackOFF");
          }else if(!strncmp(&dataNube[pos], "OPEN=", sizeof("OPEN=")-1)){
            char barrera = dataNube[pos+sizeof("OPEN=")-1];
            if(barrera == '1'){
              strcpy(response, "apertura");
              tempRele[0] = 0;
              digitalWrite(BARRERA_ENTRADA, HIGH);  
            }else if(barrera == '2'){
              strcpy(response, "apertura");
              tempRele[1] = 0;
              digitalWrite(BARRERA_SALIDA, HIGH);  
            }else{
              strcpy(response, "invalid");  
            }
          }else if(!strncmp(&dataNube[pos], "RESET", sizeof("RESET")-1)){
            Serial.println("Reset...");
            ets_printf("ESP 32 Reboot Server\n");
            esp_restart();
          }else if(!strncmp(&dataNube[pos], "NIPEVENTREG=", sizeof("NIPEVENTREG=")-1)){
            strcat(dataNube, "\n");
            dataNube[pos+sizeof("NIPEVENTREG=")-1 + 4] = dataNube[pos+sizeof("NIPEVENTREG=")-1 + 6] = '\t';
            dataNube[pos+sizeof("NIPEVENTREG=")-1 + 8] = '\t';
            if(sd_writeFile(SD_MMC, FILE_NIP_EVENT, &dataNube[pos+sizeof("NIPEVENTREG=")-1], FILE_APPEND))
              strcpy(response, "registrado");
            else
              strcpy(response, "errorFile");
          }
          else{
            strcpy(response, "desconocido");
            Serial.println("Server command unknown");
          }
          //Responder no es necesario la respuesta
          strcpy(dataNubeCmd, "DATA_SET_ID={\"id\":");
          strncat(dataNubeCmd, dataNube, pch - dataNube);
          strcat(dataNubeCmd, ",\"estatus\":\"");
          strcat(dataNubeCmd, response);
          strcat(dataNubeCmd, "\"}");
          enviarDato(dataNubeCmd, 500, false);
        }
      }
      //Reiniciar cuenta
      portENTER_CRITICAL(&timerMux);
        tempNube = 0;
      portEXIT_CRITICAL(&timerMux);
    }else{  //Fallo de conexion
      //Reiniciar cuenta  
      portENTER_CRITICAL(&timerMux);
        tempNube = 0;
      portEXIT_CRITICAL(&timerMux);
    }
  }
}
/***************************************************************************************/
void getDate(bool formatComplet){
  const char daysWeek[] = "DLMIJVS";
  DateTime now = rtc.now();
  //Convertir cadena en tiempo
  if(formatComplet)
    sprintf(cadTime, "%.1i&%.4i-%.2i-%.2i %.2i:%.2i:%.2i", now.dayOfTheWeek(), now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  else
    sprintf(cadTime, "%.4i%.2i%.2i%.2i%.2i%.2i", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
}
/***************************************************************************************/
void eventos_tcp(){
  static unsigned long pos;
  static char estados = 0;
  static bool resetFile;
  char bufferData[64], cont;
  static File file;
  //estados = 0;
  //Si no hay tcp no vacio los datos
  if(!eth_connected){  //Obtener info
    //if(eth_connected){  //Obtener info
    if(estados != 0){
      Serial.println("eth-1");
      estados = 0;
      file.close();  
    }
    //return; /////////////////////////////////////////////////////////////////////
  }
  if(enviopermitido==1){
    if(estados != 0){
      Serial.println("eth-2");
      estados = 0;
      enviopermitido = 0;
      file.close();  
    }
    }
  //Realizar el envio
  Serial.println("tratando de enviar eventos para log...");
  socketio_monitor("tratando de enviar eventos para log");
  if(estados == 0){
    file = SD_MMC.open(FILE_EVENTOS);
    if(!file)
      Serial.println("No se puede abrir el archivo EVENTOS");
    else
      estados++;
    //Reinicializar variables
    pos = 0;
    resetFile = false;
    Serial.println("Abriendo archivo Eventos->");
    Serial.println(estados,DEC);
  }else if(estados == 1){ //Posicionar el archivo en la posicion 
    Serial.println("Evaluar disponibilidad de archivo:");
    if(file.available()){
      resetFile = true;
      cont = 0;
      while(file.available()){
        bufferData[cont] = file.read();
        if(bufferData[cont] == '\n') {
          bufferData[cont] = 0;
          Serial.print("Enviar HTTP: ");    
          Serial.println(bufferData);
          //Suponer que hace el post
          enviarDato(bufferData, 100, false);
          pos += cont+1;
          //Esparamos al siguiente envio cada x segundos  
          break;  
        }
        cont = clamp_shift(++cont, 0, sizeof(bufferData)-1);
      }
    }else if(resetFile){ //Archivo completo de lectura
      resetFile = false;
      file.close();                                        //Cierro
      //sd_deleteFile(SD_MMC, FILE_EVENTOS);                 //Eliminar 
      sd_writeFile(SD_MMC, FILE_EVENTOS, "", FILE_WRITE);  //Blank file
      estados = 0;
      enviopermitido = 0;
      Serial.println("File eventos clear!");
    }
  }
}
/***************************************************************************************/
bool control_acceso(char *message, char *response){
  char buff[128], tamBuff;
  char modeAccess, numAccess, door[5];
  //Tarjeta
  char cad[16];
  int nip;
  long card;
  int line;
  unsigned long posFile;

  //Inicializar la respuesta
  response[0] = 0;
  getDate(true);  //Obtengo la hora del evento
  strcpy(cadTimeAux, cadTime);
  Serial.print("Evento ocurrido a las: ");
  Serial.println(cadTime);
  getDate(false);  //Obtengo la hora del evento
  
  //Validar segun la cadena  
  if(!strncmp("MUX", message, 3)){
    if(!strncmp("-NIP:", &message[4], 5)){ //MUXA-NIP:2585
      if(strlen(&message[9]) == 4){  //Validamos el nip recepcionado
        //Obtener dato
        nip = String(&message[9]).toInt();
        modeAccess = (message[3] == 'A' || message[3] == 'C')?'I':'O';
        numAccess = (tolower(message[3])-'a') + 1; //Acceso referencia
        numAccess = (modeAccess == 'I')? (numAccess+1)/2: numAccess/2; 
        //Obtener el acceso
        door[0] = modeAccess == 'I'? 'E':'S';
        //door[1] = numAccess+'0';
        door[1] = 0;
        strcat(door, numAccessBoard);
        //Buscar el nip
        Serial.println("Buscando nip ...");
        socketio_monitor("Buscando nip");
        char acceso = sd_updateFileNipEvent(&message[9], modeAccess);
        //Mostrar la validacion
        if(acceso == 'D')
          Serial.println("Nip desconocido");
        else if(acceso == 'A')
          Serial.println("Nip acceptado");
        else if(acceso == 'P')
          Serial.println("Nip passback");
        else if(acceso == 'C')
          Serial.println("Nip caducado");
        //Crear palabra de control
        strcpy(response, "NIPEVENT:");
        strcat(response, &message[9]);
        strcat(response, ",-,");  //Acceso+Passback+Desconocida
        response[14] = acceso;
        strcat(response, cadTime);
        strcat(response, ",");
        strcat(response, door);
      }else{  //Nip incompleto
        return false;  
      }
    }else if(!strncmp("-IDX:", &message[4], 5)){ //MUXA-IDX:0073FA62
      if(strlen(&message[9]) == 8){
        //Obtener dato
        card = strtol(&message[9], NULL, HEX);
        modeAccess = (message[3] == 'A' || message[3] == 'C')?'I':'O';
        numAccess = (tolower(message[3])-'a') + 1; //Acceso referencia  
        numAccess = (modeAccess == 'I')? (numAccess+1)/2: numAccess/2; 
        //Obtener el acceso
        door[0] = modeAccess == 'I'? 'E':'S';
        //door[1] = numAccess+'0';
        door[1] = 0;
        strcat(door, numAccessBoard);
        //Buscar tarjeta de pensionado
        Serial.println("Buscando tarjeta...");
        socketio_monitor("Buscando tarjeta");
        /*********************** Buscar tarjeta soporte **********************/
        if(sd_searchCardSoporte(SD_MMC, FILE_SOPORTE, &message[9])){
          //Validamos el acceso
          if(modeAccess == 'I'){
            tempRele[0] = 0;
            digitalWrite(BARRERA_ENTRADA, HIGH);  
          }else{
            tempRele[1] = 0;
            digitalWrite(BARRERA_SALIDA, HIGH);  
          }
          //Mostrar tipo de evento
          Serial.println("Tarjeta de soporte");
          socketio_monitor("Tarjeta de soporte");
          strcpy(response, "SOPORTE:");
          strcat(response, &message[9]);
          strcat(response, ",A,");  //Acceso+Passback+Vigencia+Desconocida
          strcat(response, cadTime);
          strcat(response, ",");
          strcat(response, door);
          return true;
        }
        /*********************** Buscar tarjeta pensionado **********************/
        File file = SD_MMC.open(FILE_PENSIONADOS);
        if(!file){
          Serial.println("No se pudo abrir archivo de lectura");
          socketio_monitor("No se pudo abrir archivo de lectura");
          return false;
        }
        //Buscar archivo
        tamBuff = 0;
        posFile = 0;
        line = 0;
        //Consultar datos
        while(file.available()){
          buff[tamBuff] = file.read();
          //Proteger rango permisible
          if(buff[tamBuff] == '\n'){  //Linea detectada 
            line++;
            //Obtener mensaje
            buff[tamBuff] = 0;
            tamBuff = 0;  //Reiniciar conteo
            //Validar tarjeta
            if(!strncmp(buff, &message[9], 8)){
              Serial.print("Pensionado ");
              socketio_monitor("Pensionado");
              Serial.print(line);
              Serial.print(": ");
              Serial.println(buff);  //Pinta el pensionado encontrado
              //Validar vigencia
              if(buff[9] == '1'){ //Vigencia
                //Sobreescribir passback
                posFile += 8+1+2; //Card(8)+Tab(1)+Vig(1)+Tab(1)
                file.seek(posFile); 
                Serial.print("Passback: ");
                socketio_monitor("Passback");
                Serial.write(file.read());
                Serial.println();
                if(buff[11] != modeAccess || !isPassback){  //Passback
                  //Validamos el acceso
                  if(modeAccess == 'I'){
                    tempRele[0] = 0;
                    digitalWrite(BARRERA_ENTRADA, HIGH);  
                  }else{
                    tempRele[1] = 0;
                    digitalWrite(BARRERA_SALIDA, HIGH);  
                  }
                  //Mensaje de accion
                  Serial.println("Acceso aceptado pensionado");    
                  socketio_monitor("Acceso aceptado pensionado");
                  strcpy(response, "PENSIONADO:");
                  strcat(response, &message[9]);
                  strcat(response, ",A,");  //Acceso+Passback+Vigencia+Desconocida
                  strcat(response, cadTime);
                  strcat(response, ",");
                  strcat(response, door);
                  //Actualiza passback
                  sd_updateFilePensionados(&message[9], false, modeAccess);
                }else{  //Passback activo
                  //Mensaje de accion
                  Serial.println("Passback activo");
                  socketio_monitor("Passback activo");
                  strcpy(response, "PENSIONADO:");
                  strcat(response, &message[9]);
                  strcat(response, ",P,");  //Acceso+Passback+Vigencia+Desconocida
                  strcat(response, cadTime);
                  strcat(response, ",");
                  strcat(response, door);
                }
              }else{  //Vigencia terminada
                //Mensaje de accion
                Serial.println("Vigencia terminada");  
                socketio_monitor("Vigencia terminada");
                strcpy(response, "PENSIONADO:");
                strcat(response, &message[9]);
                strcat(response, ",V,");  //Acceso+Passback+Vigencia+Desconocida
                strcat(response, cadTime);
                strcat(response, ",");
                strcat(response, door);
              }
              //Usuario encontrado y validado
              file.close(); //Cerrar archivo
              return true;
            }
            //Buscar nueva linea 
            posFile = file.position();
            continue;
          }
          tamBuff = clamp_shift(++tamBuff, 0, sizeof(buff)); //Proteger buffer
        }
        file.close(); //Cerrar archivo
        //Tarjeta desconocida
        Serial.println("Tarjeta desconocida");  
        socketio_monitor("Tarjeta desconocida");
        strcpy(response, "PENSIONADO:");
        strcat(response, &message[9]);
        strcat(response, ",D,");  //Acceso+Passback+Vigencia+Desconocida
        strcat(response, cadTime);
        strcat(response, ",");
        strcat(response, door);
      }else{
        strcpy(response, "MUX:ERROR_LECTURA");
        Serial.println("Error en la lectura");  
        socketio_monitor("Error en la lectura");
      }
    }else{ //Comando desconocido
      strcpy(response, "ERROR:");
      strcat(response, message);
      Serial.println("Error en la lectura");    
      return true;
    }
    //Comando aceptado
    return true;
  }
  //Comando desconocido
  return false;
}
/***************************************************************************************/
int clamp_shift(int value, int minValue, int maxValue){
  if(value > maxValue)
    value = minValue;
  else if(value < minValue)
    value = maxValue;

  return value;
}
/******************************************************************************************/
bool enviarDato(char *message, int maxTimeResponse, bool saveResponse){
  char count = 0, dato;
  bool showResponse = false;
  

  WiFiClient client;
  if (!client.connect(host, port)) {
    Serial.println("#Error conexion fallida normal");
    //serverWaitReset = true;
    return false;
  }
  //Validar el dato: 
  client.printf(page);
  client.printf("Host: %s\r\n", host);
  client.printf("Content-Type: application/x-www-form-urlencoded\r\n");
  int sizePost = strlen("idEstacionamiento=")+strlen(idEstacionamiento);
  sizePost += strlen("&evento=")+strlen(message);
  sizePost += strlen("&numAccess=")+strlen(numAccessBoard);
  client.printf("Content-Length: %d\r\n", sizePost); 
  client.printf("Connection: keep-alive\r\n");
  client.printf("\r\n");
  client.printf("idEstacionamiento=");
  client.printf(idEstacionamiento);  //device macaddress
  client.printf("&evento=");
  client.printf(message);
  client.printf("&numAccess=");
  client.printf(numAccessBoard);

  //Varificar si responde la pagina
  unsigned long timeout = millis();
  while(!client.available()){
    if(millis() - timeout > maxTimeResponse) {
      Serial.println("# Client Timeout");
      return false;
    }
  }
  //Mostrar los datos leidos, solo respuesta del sevidor
  dataNubeCont = 0;
  while(client.available()){
    dato = client.read();
    //Reflejo de los datos
    if(dato == '\r' || dato =='\n'){
      if(++count == 4 && !showResponse){
        if(!saveResponse)
          Serial.print("RESPUESTA SERVIDOR: ");
        showResponse = true;
        continue;
      }
    }else{
      count = 0;
    }
    //Mostrar y procesar respuesta
    if(showResponse){
      dataNube[dataNubeCont] = dato;
      if(!saveResponse)
        Serial.write(dato);
      dataNubeCont = clamp_shift(++dataNubeCont, 0, sizeof(dataNube)-1);
    }
  }
  dataNube[dataNubeCont] = 0; //Fin
  //Salto de linea
  if(!saveResponse)
    Serial.println("");
  //Cerrar conexion
  //Serial.println("Closing connection\n");
  client.stop();
  //Conexion valida
  return true;
}
/******************************************************************************************/
  bool enviarDatoAsync(char *message){
  char count = 0, dato;
  String datoenviar_request = "";
  bool showResponse = false;
  static byte machine = 0;
  static WiFiClient client;
  
  datoenviar_request = "idEstacionamiento=505\r&evento=DATA_GET_NEW&numAcces=1\r"; 
  sendGetDataNubeRequest( datoenviar_request );
           
  return true;
}
/***************************************************************************************/
/***********************************  ETHERNET  ****************************************/
/***************************************************************************************/
void WiFiEvent(WiFiEvent_t event){
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      //ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      serverWaitReset = true;
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
     // Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
     // Serial.print(ETH.localIP());
     // if (ETH.fullDuplex()) {
      //  Serial.print(", FULL_DUPLEX");
     // }
      Serial.print(", ");
     // Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      //Conectado cable de ETH
      eth_connected = true;
      serverWaitReset = false;
      //strcpy(device, ETH.macAddress());
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      //device[0] = 0;
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      //device[0] = 0;
      Serial.println("ETH Stopped");
      //Serial.println("Se necesita nuevo arranque.");
      eth_connected = false;
      break;
    default:
      break;
  }
}
/***************************************************************************************/
/***********************************  SERIAL  ******************************************/
/***************************************************************************************/
void serialEventData() {
  //Obtener los datos
  while(Serial3.available()) {
    char recieve = Serial3.read();
    //Saltar caracter
    if (recieve == '\n') 
      continue;
    //Recepcion del dato
    if(recieve == '\r'){
      serialBuffer.message[serialBuffer.tam] = '\0';
      serialBuffer.tam = 0;
      serialBuffer.isNewMessage = true;
      continue;
    }
    //Proteccion overflow
    if(serialBuffer.tam > (sizeof(serialBuffer.message) - 1)) { //Overflow
      serialBuffer.tam = 0;
      strcpy(serialBuffer.message, "Overflow");
      serialBuffer.isNewMessage = true;
    }
    //Apilar los datos
    serialBuffer.message[serialBuffer.tam++] = recieve;
  }
}

void serialEventDataPrincipal() {
  static char cont = 0, buff[60], offset;
  //Obtener los datos
  while(Serial.available()) {
    char recieve = Serial.read();
    //Saltar caracter
    if (recieve == '\n') 
      continue;
    //Llenar los datos
    buff[cont++] = recieve;
    cont = (cont < sizeof(buff)-1)?cont:0;  //Limitar
    //Recepcion del dato
    if(recieve == '\r'){
      buff[cont] = '\0';
      cont = 0;
      Serial.print("Comando Serial: ");
      Serial.println(buff);
      //Recibir comando
      if(!strncmp(buff, "FILE:", sizeof("FILE:")-1)){
        offset = sizeof("FILE:")-1;
        Serial.print("File show:");
        Serial.println(&buff[offset]);
        //Mostrar file
        sd_readFile(SD_MMC ,&buff[offset]);
      }    
    }
  }
}
/***************************************************************************************/
/***********************************  SD CARD  *****************************************/
/***************************************************************************************/
void sd_listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);
  //Archivo raiz
  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }
  //Verificar los archivos
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels)
        sd_listDir(fs, file.name(), levels -1);
    }else{
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
/***************************************************************************************/
void sd_createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path))
    Serial.println("Dir created");
  else
    Serial.println("mkdir failed");
}
/***************************************************************************************/
void sd_removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path))
    Serial.println("Dir removed");
  else
    Serial.println("rmdir failed");
}
/***************************************************************************************/
void sd_readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  //
  Serial.print("Read from file: ");
  while(file.available())
    Serial.write(file.read());
}
/***************************************************************************************/
void sd_readFileEstacionamiento(){
  char cont = 0;
  char datos[16];
  char variable = 0;
  //Leer archivo
  File file = SD_MMC.open(FILE_ESTACIONAMIENTO);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  //Obtener datos
  //Serial.print("ID estacionamiento: ");
  while(file.available()){
    datos[cont] = file.read();
    //Validar dato entra cada linea
    if(datos[cont] == '\n'){
      datos[cont] = 0; //fin
      variable++;
      //Consultar
      if(variable == 1){
        strcpy(idEstacionamiento, datos);
        Serial.print("ID Estacionamiento: ");
        Serial.println(idEstacionamiento);
      }else if(variable == 2){
        strcpy(numAccessBoard, datos);
        Serial.print("Numero de Acceso: ");
        Serial.println(numAccessBoard);
      }
      //Reiniciar contador
      cont = 0;
    }else{
      cont++;  
    }
  }
  Serial.println("");
  file.close();
}
/***************************************************************************************/
void sd_readFilePassback(){
  char cont = 0;
  File file = SD_MMC.open(FILE_PASSBACK);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  //Obtener passback
  Serial.print("SYSTEM PASSBACK: ");
  if(file.available())
    isPassback = file.read() == '1' ? true: false;
  if(isPassback)
    Serial.println("ON");
  else
    Serial.println("OFF");
  file.close();
}
/***************************************************************************************/
bool sd_searchCardSoporte(fs::FS &fs, const char * path, char *dataSearch){
  char datos[16];
  char cont = 0;
  //Leer archivo
  File file = fs.open(path);
  if(!file){
    Serial.print("Error al leer el archivo: ");
    Serial.println(path);
    return false;
  }
  //Buscar texto
  while(file.available()){
    datos[cont] = file.read();
    //Validar dato entra cada linea
    if(datos[cont] == '\n'){
      datos[cont] = 0; //fin
      if(!strcmp(datos, dataSearch))
        return true;
      //Validar la siguiente tarjeta
      cont = 0;
      continue;
    }
    //Incrementar punter
    cont = clamp_shift(++cont, 0, sizeof(datos)-1);
  }

  return false;
}
/***************************************************************************************/
bool sd_writeFile(fs::FS &fs, const char * path, const char * message, const char *type){
  Serial.printf("Writing file: %s\n", path);
  
  File file = fs.open(path, type);  //FILE_READ, FILE_WRITE, FILE_APPEND
  if(!file){
    Serial.println("Failed to open file for writing");
    return false;
  }
  //Mostrar estatus del archivo
  if(file.print(message)){
    Serial.println("File written");
    return true;
  }else{
    Serial.println("Write failed");
    return false;
  }
}
/***************************************************************************************/
void sd_updateFilePensionados(char *pensionado, bool vigOrPass, char value){
  char datos[16];
  char cont = 0;
  
  Serial.printf("Update file: %s\n", FILE_PENSIONADOS);
  //Eliminar archivos temporales previos
  sd_deleteFile(SD_MMC, "/pensionadosTemp.txt");
  //Abrir archivos
  File filePensionados = SD_MMC.open(FILE_PENSIONADOS);
  File fileTemp = SD_MMC.open("/pensionadosTemp.txt", FILE_APPEND); //Reescribe el archivo
  
  if(!filePensionados || !fileTemp){
    Serial.println("Error al actualizar archivos");
    return;
  }
  
  //Buscar texto
  while(filePensionados.available()){
    datos[cont] = filePensionados.read();
    //Validar dato entra cada linea
    if(datos[cont] == '\n'){
      cont = clamp_shift(++cont, 0, sizeof(datos)-1);
      datos[cont] = 0; //fin
      if(!strncmp(datos, pensionado, 8) || strlen(pensionado) == 0){
        if(vigOrPass)
          datos[9] = value;
        else
          datos[11] = value;
        fileTemp.print(datos);
      }else{
        fileTemp.print(datos);
      }
      //Validar la siguiente tarjeta
      cont = 0;
      continue;
    }
    //Incrementar punter
    cont = clamp_shift(++cont, 0, sizeof(datos)-1);
  }
  //Cerrar
  filePensionados.close();
  fileTemp.close();
  //Eliminar y renombrar
  sd_deleteFile(SD_MMC, FILE_PENSIONADOS);
  sd_renameFile(SD_MMC, "/pensionadosTemp.txt", FILE_PENSIONADOS);
  Serial.println("Archivo pensionados actualizado");
}
/***************************************************************************************/
byte sd_updateFileNipEvent(char *nip, char modeAccess){
  char datos[64];
  char cont = 0;
  char access = 'D';

  //Actualizar archivo
  Serial.printf("Upload file: %s\n", FILE_NIP_EVENT);
  //Eliminar archivos temporales previos
  sd_deleteFile(SD_MMC, "/tempNipEvent.txt");
  //Abrir archivos
  File fileNipEvent = SD_MMC.open(FILE_NIP_EVENT);
  File fileTemp = SD_MMC.open("/tempNipEvent.txt", FILE_APPEND); //Reescribe el archivo
  
  if(!fileNipEvent || !fileTemp){
    Serial.println("Error al actualizar archivos");
    return 'E';
  }
  
  //Buscar texto
  while(fileNipEvent.available()){
    datos[cont] = fileNipEvent.read();
    //Validar dato entra cada linea
    if(datos[cont] == '\n'){
      cont = clamp_shift(++cont, 0, sizeof(datos)-1);
      datos[cont] = 0; //fin
      datos[63] = 0; //fin
      cont = 0;
      //Eliminar nip con fecha de otro dia
      if(strncmp(&cadTimeAux[2], &datos[9], 10)){
        Serial.print("NIP CADUCADO DATE: ");  
        Serial.println(datos);
        continue;
      }
      //Validar nip
      if(!strncmp(datos, nip, 4)){
        //Mostramos el dato
        Serial.print("NIP: ");
        Serial.println(datos);
        //Validar que pueda salir
        if(modeAccess == 'I')
          if(datos[5] != 'I'){
            datos[5] = modeAccess;
            access = 'A';
            tempRele[0] = 0;
            digitalWrite(BARRERA_ENTRADA, HIGH);  
          }else
            access = 'P';
        else{
          if(datos[7] != 'O'){
            datos[7] = modeAccess;
            access = 'A';
            tempRele[1] = 0;
            digitalWrite(BARRERA_SALIDA, HIGH);  
          }else
            access = 'P';
        }
        //Reescribir si aun hay acesso disponibles
        if(datos[5] != 'I' || datos[7] != 'O')
          fileTemp.print(datos);
        else if(access != 'A')
          access = 'C';
      }else{
        fileTemp.print(datos);
      }
      //Validar la siguiente tarjeta
      continue;
    }
    //Incrementar punter
    cont = clamp_shift(++cont, 0, sizeof(datos)-1);
  }
  //Cerrar
  fileNipEvent.close();
  fileTemp.close();
  //Eliminar y renombrar
  sd_deleteFile(SD_MMC, FILE_NIP_EVENT);
  sd_renameFile(SD_MMC, "/tempNipEvent.txt", FILE_NIP_EVENT);
  Serial.println("Archivo nipEvent actualizado");

  return access;
}
/***************************************************************************************/
void sd_renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if(fs.rename(path1, path2))
    Serial.println("File renamed");
  else
    Serial.println("Rename failed");
}
/***************************************************************************************/
void sd_deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path))
    Serial.println("File deleted");
  else
    Serial.println("Delete failed");
}
/***************************************************************************************/
bool sd_updateFile(fs::FS &fs, const char *path, int row, char *value){
  const char *pathTemp = "/tempFileUpdate.txt";
  char datos[60];
  char cont = 0;
  char rowAct = 0;

  //Actualizar archivo
  Serial.printf("Upload file: %s\n", path);
  //Eliminar archivos temporales previos
  sd_deleteFile(SD_MMC, pathTemp);
  //Abrir archivos
  File fileUpdate = SD_MMC.open(path);
  File fileTemp = SD_MMC.open(pathTemp, FILE_APPEND); //Reescribe el archivo
  
  if(!fileUpdate || !fileTemp){
    Serial.println("Error al actualizar archivos");
    return false;
  }
  
  //Buscar texto
  while(fileUpdate.available()){
    datos[cont] = fileUpdate.read();
    //Validar dato entra cada linea
    if(datos[cont] == '\n'){
      cont = clamp_shift(++cont, 0, sizeof(datos)-1);
      datos[cont] = 0; //fin
      
      if(++rowAct == row){
        Serial.print("File Update content past:");  
        Serial.println(datos);
        Serial.print("File Update content actual:");  
        Serial.println(value);
        fileTemp.print(value);
      }else{
        fileTemp.print(datos);
      }
      //Reiniciar contador
      cont = 0;
      continue;
    }
    //Incrementar punter
    cont = clamp_shift(++cont, 0, sizeof(datos)-1);
  }
  //Cerrar
  fileUpdate.close();
  fileTemp.close();
  //Eliminar y renombrar
  sd_deleteFile(SD_MMC, path);
  sd_renameFile(SD_MMC, pathTemp, path);
  Serial.println("Archivo actualizado");

  return true;
}
/***************************************************************************************/
void envio_log(){
 //Procesar comando
    Serial.print("RS232: ");
    Serial.println(serialBuffer.message);
    //Realizar accion del comando
    if(control_acceso(serialBuffer.message, evento)){
      strcat(evento, "\n");  //Add 
      //Reportar evento
        Serial.print("Evento: ");
        Serial.println(evento);
        //Reportar al sistema
        sd_writeFile(SD_MMC, FILE_EVENTOS, evento, FILE_APPEND); 
    }else{
      Serial.println("Comando desconocido");
    }
    //Limpiar buffer
    serialBuffer.message[0] = 0;
    serialBuffer.isNewMessage = false;
    enviopermitido = 1;
}
/***************************************************************************************/
