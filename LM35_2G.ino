/*
 * Code to conect a LM35 Sensor with a TTGO T-Call V1.3 ESP32 SIM800L 240Mhz
 * Data from LM35 Sensor publish ThingSpeak
 * Part of this code is from Random Tutorials: https://randomnerdtutorials.com/esp32-sim800l-publish-data-to-cloud/
 * 
 */
// Your GPRS credentials (leave empty, if not needed)
const char apn[]      = "internet.itelcel.com"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "webgprs"; // GPRS User
const char gprsPass[] = "webgprs2002"; // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = ""; 

// Server details
// The server variable can be just a domain name or it can have a subdomain. It depends on the service you are using
const char server[] = "thingspeak.com"; // domain name: example.com, maker.ifttt.com, etc
const char resource[] = "https://api.thingspeak.com/update?api_key=C***************";         // resource path, for example: /post-data.php
const int  port = 80;                             // server port number

// Keep this API Key value to be compatible with the PHP code provided in the project page. 
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key 
String apiKeyValue = "C***************";

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              18
#define I2C_SCL              19

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#define ADC_VREF_mV    3300.0 // in miliVolt
#define ADC_RESOLUTION 4096.0
#define PIN_LM35       36 // ESP32 pin GIOP36 (ADC0) connected to LM35

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

//Variables para la temperatura
int adcVal = 0;
float miliVolt = 0;
float tempC;
float tempF;
float tempX;
long MarcaTiempo = 0;

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#include <Wire.h>
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

// TinyGSM Client for Internet connection
TinyGsmClient client(modem);

#define uS_TO_S_FACTOR 1000000UL   /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  3600        /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour */

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

String Respuesta;


void setup() {
  // Set serial monitor debugging window baud rate to 115200
  SerialMon.begin(115200);

  // Start I2C communication
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000ul);

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }

}

void loop() {
  sendData();
}

 void readSensor(){
  MarcaTiempo = millis();
  // read the ADC value from LM35 sensor
  adcVal = analogRead(PIN_LM35);
  // convert the ADC value to voltage in miliVolt
  miliVolt = adcVal * (ADC_VREF_mV / ADC_RESOLUTION);
  // convert the voltage to the temperature in °C
  tempC = miliVolt / 10;
  // convert the °C to °F
  tempF = tempC * 9 / 5 + 32;


  // print the temperature in the Serial Monitor:
  Serial.print("Temperature: ");
  Serial.print(tempC);   // print the temperature in °C
  Serial.print("°C");
  Serial.print("  ||  "); // separator between °C and °F
  Serial.print(tempF);   // print the temperature in °F
  Serial.println("°F");
}


void sendData(){
    SerialMon.print("Connecting to APN: ");
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println(" fail");
    }
    else {
      SerialMon.println(" OK");
      
      SerialMon.print("Connecting to ");
      SerialMon.print(server);
      if (!client.connect(server, port)) {
        SerialMon.println(" fail");
      }
      else {
        SerialMon.println(" OK");
        readSensor();
    
        // Making an HTTP POST request
        SerialMon.println("Performing HTTP POST request...");
        // Prepare your HTTP POST request data (Temperature in Celsius degrees)
        String httpRequestData = "api_key=" + apiKeyValue + "&field1=" + tempC
                               + "&field2=" + tempF + "";
        client.print(String("POST ") + resource + " HTTP/1.1\r\n");
        client.print(String("Host: ") + server + "\r\n");
        client.println("Connection: close");
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.print("Content-Length: ");
        client.println(httpRequestData.length());
        client.println();
        client.println(httpRequestData);

        unsigned long timeout = millis();
        while (client.connected() && millis() - timeout < 10000L) {
          while (client.available()) {
            char c = client.read();
            Respuesta += c;
            SerialMon.print(Respuesta);
            timeout = millis();
          }
        }
        SerialMon.println();
    
        // Close client and disconnect
        client.stop();
        SerialMon.println(F("Server disconnected"));
        modem.gprsDisconnect();
        SerialMon.println(F("GPRS disconnected"));
      }
    }
}
