#include <SoftwareSerial.h>
#include <String.h>
#include <CapacitiveSensor.h>
#include<Arduino.h>



const char SMS_NUMBER[] = "+918904697414";  
const unsigned long MIN_MS_BETWEEN_SMS = 60000;
const boolean SMS_ENABLED = true;
const boolean SEND_SMS_AT_STARTUP = true;
const boolean CAP_SENSOR_ENABLED = true; 

const long CAP_LOW_THRESHOLD = 1200; 
const long CAP_HIGH_THRESHOLD = 3000; 

#define LIGHT_SENSOR A0
#define PIR_POWER 3
#define PIR_SENSOR 2
#define PIR_GROUND 4
#define CONTACT_SENSOR 10
#define CAPACITIVE_SEND 12
#define CAPACITIVE_RECEIVE 11

String message = String(""); 
unsigned long last_sms_time = 0;
boolean pir_reading = LOW;
boolean contact_reading;
boolean capacitive_reading = false;
long raw_capacitive_reading;
CapacitiveSensor cap_sensor = CapacitiveSensor(CAPACITIVE_RECEIVE, CAPACITIVE_SEND);

SoftwareSerial GPRS(7, 8);

void setup()
{
  
  Serial.begin(9600);   
  Serial.println(F("Welcome to Cellular Sensor Sentinel")); 
  
  pinMode(LIGHT_SENSOR, INPUT_PULLUP);
  pinMode(PIR_GROUND, OUTPUT);
  pinMode(PIR_POWER, OUTPUT);
  pinMode(PIR_SENSOR, INPUT);
  digitalWrite(PIR_GROUND, LOW);
  digitalWrite(PIR_POWER, HIGH);
  
  pinMode(CONTACT_SENSOR, INPUT_PULLUP);
  contact_reading = digitalRead(CONTACT_SENSOR);
  
  GPRS.begin(9600);
  
  turnOnCellModule();
  
  enableNetworkTime(); 

  if (CAP_SENSOR_ENABLED)
  {  
    raw_capacitive_reading = cap_sensor.capacitiveSensor(30);
    if (raw_capacitive_reading < CAP_LOW_THRESHOLD)
    {
      capacitive_reading = false;
    } else if (raw_capacitive_reading > CAP_HIGH_THRESHOLD)
    {
      capacitive_reading = true;
    }
  }
  
  
  while (millis() < 60000)
  {
    //do nothing
  }
  
  if (SEND_SMS_AT_STARTUP && SMS_ENABLED)
  {
    startTextMessage();
    GPRS.println(F("The Sensor Sentinel is Activited!! Your home is secured."));
    endTextMessage();
  }
  Serial.println(F("The Sensor Sentinel is ready."));
}

void loop()
{
  //This function runs over and over after setup().
  drainSoftwareSerial(true);
  
  if (digitalRead(PIR_SENSOR) != pir_reading)//pir sensor
  {
    pir_reading = !pir_reading;
    if (pir_reading)
    {
      Serial.println(F("PIR just activated."));
      message = String("PIR activated!! Warning! Security Breach :");
      sendTextMessage();
    } else
    {
      Serial.println(F("PIR deactivated."));
    }
  }
  
  if (digitalRead(CONTACT_SENSOR) != contact_reading)//contact sensor
  {
    contact_reading = !contact_reading;
    if (!contact_reading)
    {
      Serial.println(F("Contact switch just pressed."));
      message = String("Contact pressed Warning! Security Breach:");
      sendTextMessage();
    } else
    {
      Serial.println(F("Contact switch just released."));
      message = String("Contact released:");
      sendTextMessage();    
    }
  }
  
  if (CAP_SENSOR_ENABLED)
  {
    raw_capacitive_reading = cap_sensor.capacitiveSensor(30);
    Serial.print("Raw capacitive reading: ");
    Serial.println(raw_capacitive_reading);
    
    if (capacitive_reading && raw_capacitive_reading < CAP_LOW_THRESHOLD)
    {
      capacitive_reading = !capacitive_reading;
      Serial.println("Cap sensor deactivated.");
      message = String("Cap sensor deactivated.");
      sendTextMessage();
    } else if (!capacitive_reading && raw_capacitive_reading > CAP_HIGH_THRESHOLD)
    {
      capacitive_reading = !capacitive_reading;
      Serial.println(F("Cap sensor activated."));
      message = String("Cap sensor activated Warning! Security Breach.");
      sendTextMessage();
    }
  }
    
  delay(100);
}

void drainSoftwareSerial(boolean printToSerial)
{
  char b;
  while (GPRS.available())
  {
    b = GPRS.read();
    if (printToSerial)
    {
      Serial.print(b);
    }
  }
}

boolean toggleAndCheck()
{
  
  togglePower(); 
  drainSoftwareSerial(true); 
  delay(50); 
  GPRS.print("AT\r"); 
  delay(250); 
  if (GPRS.available())   
  {
    drainSoftwareSerial(false);
    delay(200);
    return false; 
  }
  return true;
}

void turnOnCellModule()
{
  
  while (toggleAndCheck()) 
  {
    Serial.println(F("Trying again."));
  }
}

void togglePower() 
{
  Serial.println(F("Powering Up GSM Shield"));
  pinMode(9, OUTPUT); 
  digitalWrite(9, LOW);
  delay(1000);
  digitalWrite(9, HIGH);
  delay(2000);
  digitalWrite(9, LOW);
  delay(3000);
  Serial.println(F("GSM Shield Powered Up"));
}


void enableNetworkTime()
{
  GPRS.print("AT+CLTS=1\r");//for enable time
  delay(1000);
  drainSoftwareSerial(true);
}


void getTime()
{

  drainSoftwareSerial(true); 

  GPRS.print("AT+CCLK?\r");//to get time
  delay(1000);
  char b;
  boolean inside_quotes = false;
  while (GPRS.available())
  {
    b = GPRS.read();
    
    if (b == '"')
    {
      inside_quotes = !inside_quotes;  
    }
    
    if (inside_quotes && b != '"')
    {
      message += b;
    } 
  } 
}

void startTextMessage()
{
  GPRS.print("AT+CMGF=1\r");    //Send the SMS in text mode
  delay(100);
  GPRS.print("AT+CMGS= \"");  //to send text
  GPRS.print(SMS_NUMBER);
  GPRS.println("\"");
  delay(100);
}

void endTextMessage()
{
  delay(100);
  GPRS.println((char)26); //control-z
  delay(100);
  GPRS.println();
}
 
void sendTextMessage()
{
  
  boolean send_sms = false;
  if (!SMS_ENABLED)
  {
    Serial.println(F("SMS disabled due to setting."));
  } else
  {
    if (last_sms_time > 0 && millis() < last_sms_time + MIN_MS_BETWEEN_SMS)
    {
      Serial.println(F("SMS blocked since we already sent one recently."));
    } else
      send_sms = true;
  }
  
  annotateMessage(); 
  
  if (send_sms)
  {
    startTextMessage();
    GPRS.print(message);
    endTextMessage();
    
    last_sms_time = millis();
    Serial.print(F("Actually sent the following SMS to "));
  } else
  {
    Serial.print(F("Didn't actually send this SMS to "));
  }
      Serial.print(SMS_NUMBER);
      Serial.print(F(":"));
      Serial.println(message);
}

void annotateMessage()
{
  //This function appends some sensor readings to the current text.
  
  message += " Light: ";
  message += analogRead(LIGHT_SENSOR);
  
  message += " Contact: ";
  if (digitalRead(CONTACT_SENSOR))
  {
    message += "released";
  } else
  {
    message += "pressed";
  }
  
  if (CAP_SENSOR_ENABLED)
  {
    message += " Capacitive: ";
    message += cap_sensor.capacitiveSensor(30);
  }
  
  message += " (sent at ";
  getTime();
  message += ")";
}
