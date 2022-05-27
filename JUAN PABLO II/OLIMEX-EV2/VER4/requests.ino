/***************************************************************************************/
String daysOfTheWeek[7] = { "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado" };
String monthsNames[12] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo",  "Junio", "Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre" };

void printDate(DateTime date)
{
   Serial.print(date.year(), DEC);
   Serial.print('/');
   Serial.print(date.month(), DEC);
   Serial.print('/');
   Serial.print(date.day(), DEC);
   Serial.print(" (");
   Serial.print(daysOfTheWeek[date.dayOfTheWeek()]);
   Serial.print(") ");
   Serial.print(date.hour(), DEC);
   Serial.print(':');
   Serial.print(date.minute(), DEC);
   Serial.print(':');
   Serial.print(date.second(), DEC);
   Serial.println();
}

/***************************************************************************************/
bool sendAccesoRequest( String message ){
  static bool requestOpenResult;

  if( AccesoRequest.readyState() == readyStateUnsent || AccesoRequest.readyState() == readyStateDone ){
    requestOpenResult = AccesoRequest.open("POST", "http://143.198.230.37/api/access");
    AccesoRequest.setReqHeader( "content-type", "application/json" );
    if( requestOpenResult ){
      AccesoRequest.send( message );
    }else{
      Serial.println( "Can't send bad conteo request" );
    }
  }else{
    Serial.println( "Can't send conteo request" );
    ESP.restart();
  }
}
/***************************************************************************************/
void AccesoCB( void* optParm, AsyncHTTPRequest* request, int readyState ){
  String payload = "";
  
  if( readyState == readyStateDone ){
    Serial.print("Código de respuesta HTTP: ");
    int responseHttpCode = request->responseHTTPcode();
    Serial.println( responseHttpCode );
    payload = request->responseText();
    Serial.println("\nConteo**************************************");
    Serial.println(payload);
    Serial.println("Conteo****************************************");
    if( responseHttpCode >= 200 && responseHttpCode < 300 ){
      StaticJsonDocument<64> doc;
      deserializeJson(doc, payload);
      int access = doc["access"];
      String barrera = doc["barrera"];
      if( access && barrera == "entrada" ){
         tempRele[0] = 0;
         digitalWrite( BARRERA_ENTRADA, HIGH );
      }else if( access && barrera == "salida" ){
        tempRele[1] = 0;
        digitalWrite( BARRERA_SALIDA, HIGH );
      }
      envio_log();
    }
    requestAvailable = true;
    request->setDebug(false);
  }
}
/***************************************************************************************/
bool sendNipRequest( String message ){
  static bool requestOpenResult;

  if( NipRequest.readyState() == readyStateUnsent || NipRequest.readyState() == readyStateDone ){
    requestOpenResult = NipRequest.open("POST", "http://143.198.230.37/api/access/nip");
    NipRequest.setReqHeader( "content-type", "application/json" );
    if( requestOpenResult ){
      NipRequest.send( message );
    }else{
      Serial.println( "Can't send bad conteo request" );
    }
  }else{
    Serial.println( "Can't send conteo request" );
    ESP.restart();
  }
}
/***************************************************************************************/
void NipCB( void* optParm, AsyncHTTPRequest* request, int readyState ){
  String payload = "";
  
  if( readyState == readyStateDone ){
    Serial.print("Código de respuesta HTTP: ");
    int responseHttpCode = request->responseHTTPcode();
    Serial.println( responseHttpCode );
    payload = request->responseText();
    Serial.println("\nConteo**************************************");
    Serial.println(payload);
    Serial.println("Conteo****************************************");
    if( responseHttpCode >= 200 && responseHttpCode < 300 ){
      StaticJsonDocument<64> doc;
      deserializeJson(doc, payload);
      int access = doc["access"];
      String barrera = doc["barrera"];
      if( access && barrera == "entrada" ){
         tempRele[0] = 0;
         digitalWrite( BARRERA_ENTRADA, HIGH );
      }else if( access && barrera == "salida" ){
        tempRele[1] = 0;
        digitalWrite( BARRERA_SALIDA, HIGH );
      }
      envio_log();
    }
    request->setDebug(false);
  }
}

/***************************************************************************************/
bool sendGetDataNubeRequest( String message ){
  static bool requestOpenResult;
  const char hostapi[]= "c68d9ab.online-server.cloud/DesarrolloWeb-CitasAccesa/php/dataloggerESP32.php";

  if( GetDataNubeRequest.readyState() == readyStateUnsent || GetDataNubeRequest.readyState() == readyStateDone ){
    requestOpenResult = GetDataNubeRequest.open("POST", hostapi);
    GetDataNubeRequest.setReqHeader( "content-type", " application/x-www-form-urlencoded" );
    if( requestOpenResult ){
      GetDataNubeRequest.send( message );
    }else{
      Serial.println( "Can't send bad conteo request" );
    }
  }else{
    Serial.println( "Can't send conteo request" );
    ESP.restart();
  }
}
/***************************************************************************************/
void GetDataNubeCB( void* optParm, AsyncHTTPRequest* request, int readyState ){
  String payload = "";
  char dataNubeCont = 0;
  char dataNube[128];
  char dataNubeCmd[128];
  static char estado = 0;
  char response[32], cad[32];
  char pos;
  char nameFile[64];
  String datoFecha = "";
   dataNube[0] = dataNube[127] = 0;
  
  if( readyState == readyStateDone ){
   // Serial.print("Código de respuesta HTTP: ");
    int responseHttpCode = request->responseHTTPcode();
   // Serial.println( responseHttpCode );
    payload = request->responseText();
    //Serial.println("\nConteo**************************************");
   // Serial.println(payload);
   // Serial.println("Conteo****************************************");
  //////////////////////////////////////////////////////////////////////////

  payload.toCharArray(dataNube, payload.length()+1);
    // Serial.printf(dataNube);
  ///////////////////////////////////////////////////////////////////////// 
    if( responseHttpCode >= 200 && responseHttpCode < 300 ){
      
      //Mostrar datos obtenido
      if(strlen(dataNube)){
        //Validar comando
        strcpy(response, "leido");
        char *pch = strchr(dataNube, ',');
        if(pch != NULL){
          strncpy(dataNubeCmd ,dataNube, pch-dataNube);
          dataNubeCmd[pch-dataNube] = 0;
          Serial.print("ID: ");
          Serial.println(dataNubeCmd); 
          Serial.print("Data: ");
          Serial.println(&dataNube[pch-dataNube+1]); 
          //Procesar comando
          pos = pch-dataNube+1;
          if(!strncmp(&dataNube[pos], "SETDATE=", sizeof("SETDATE=")-1)){
            Serial.println(&dataNube[pos+sizeof("SETDATE=")-2]);
            //Serial.println(&dataNube.toString());
            DateTime fecha = DateTime(&dataNube[pos+sizeof("SETDATE=")-2]);
            rtc.adjust(fecha);
            DateTime now = rtc.now();
            printDate(now);
            strcpy(response, "modificado");
          }else if(!strncmp(&dataNube[pos], "GETDATE", sizeof("GETDATE")-1)){
            getDate(false);  //Obtengo la hora del evento
            //Serial.println(getDate(false));
            strcpy(response, cadTime);
            Serial.println(cadTime);
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
          strncat(dataNubeCmd, dataNube, pch-dataNube);
          strcat(dataNubeCmd, ",\"estatus\":\"");
          strcat(dataNubeCmd, response);
          strcat(dataNubeCmd, "\"}");
          enviarDato(dataNubeCmd, 500, false);
        }
      }
      
     
    }
    requestAvailable = true;
    request->setDebug(false);
  }
}
