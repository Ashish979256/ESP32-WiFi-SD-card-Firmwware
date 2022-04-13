// Load Wi-Fi library
#include <WiFi.h>
#include <TimeLib.h>
#include <SensorModbusMaster.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include "FS.h"
#include <SPIFFSIniFile.h>
#include <string.h>
#include <Arduino.h>
#define FORMAT_SPIFFS_IF_FAILED true
#define modbusEn -1
#define modbusPort Serial1
#define modbus_Rx 16
#define modbus_Tx 17
#define DI1 14                // digital1 gpio is 14
#define DI2 15                // digital2 gpio is 14
#define DI3 13                // digital3 gpio is 13
#define DI4 12                // digital4 gpio is 12
#define AI1 35                // Analog1 gpio is 35
#define AI2 36                // Analog2 gpio is 36
#define AI3 39                // Analog3 gpio is 39
#define sdCardSS  5           // SD card chip select pin is GPIO 5
modbusMaster master;
WiFiClient client;
RTC_DS1307 rtc;
File myFile;
struct manufacturerDetails
{
  //define manufacturer configuration here
  char serial_no[20];        //– serial number assigned to the datalogger  
  char firmware_version[20]; //- Firmware version uploaded to this device
  //char server_address[100]  //- Server address to send data
  }mfc;
  
struct controlFlag
{
  //define control flag here
  bool is_active;           //- flag to disable device remotely
  bool is_sdcard;            //- flag to disable/enable record saving in SD card
  bool is_rms;               //- flag to disbale/enable rms feature
  bool is_dgsync;            //- flag to disable/enable dg sync feature
  bool is_zeroexport;        //- flag to disable zero export feature
  bool is_gsm;               //- flag to enable disable internet using gsm
  bool is_wifi;              //- flag to enabledisable internet using wifi
  bool is_ethernet;          //- falg to enable disable internet using ethernet
  }flag;

struct dataLoggerDetails
{
  //define logger specific configuration here
  bool      is_configured=0;    //flag to check if device is configured
  uint16_t  plant_id;       //the site at which the datalogger will be installed
  uint8_t   logger_id;        //datalogger number, default:1
  uint8_t   device_count;     //No of devices going to be connected to the datalogger
  uint16_t  read_interval;   //the time interval to record/upload data 
  }logger;
struct clientDeviceDetails
{
  //define devices specific configuaration here
  uint8_t   device_index;      //– index number for location 
  uint8_t   device_type;   //– inverter, wms, mfm etc.
  uint8_t   fun_code;     //– 0x01, 0x03, 0x04
  uint8_t   device_id;        //– min: 1 and max: 255
  uint8_t   slave_id;          //– min: 1 and max: 255
  uint8_t   bin_count;         //– min:1 and max:255 
  char      bin_start [30];  //– value: 0-255
  char      bin_length [30];   //– value: 0-29
  char      ip_address[20];
  char      port[20];
  char      protocol[10];
  }device[8];


struct ModbusDetails
{
  uint16_t  baud_rate =9600;  //default is 9600 for modbus
  uint16_t  poll_rate =500;   //default is 500 for modbus
  uint16_t  timeout   =1000;  //default is 1000 for modbus
  uint8_t   parity    = 0;    //0-None,1-odd,2-even
  }modbus;



struct networkDetails
{
  bool dhcp_enable;
  char ssid[30];
  char password[20];
  char mac_wifi[20];
  char ip_wifi[20];
  char gateway_wifi[20];
  char subnet_wifi[20];
  char priDNS_wifi[20];
  char secDNS_wifi[20];
  char mac_ether[20];
  char ip_ether[20];
  char gateway_ether[20];
  char subnet_ether[20];
  char priDNS_ether[20];
  char secDNS_ether[20];
}network;
struct trackerDetails
{
  uint16_t y=2021;  //last upload year for the records
  uint8_t m=3;      //last upload month for the records
  uint8_t d=24;     //last upload day for the records
  uint8_t h=0;      //last upload hour for the records
  uint16_t count=0; //last upload count for the records
  }lastUpload;

//--------------------------------------------------------------------------------------------------------------------
char server[] = "www.holmiumtechnologies.com";          // name address for Google (using DNS)
char data_path[]= "/rms/api/insertdata?";               // path to insert device data
char config_path[]= "/rms/api/configuration/sync?";     // path to configure logger using server configuration
char timestamp_path[]="/rms/api/timestamp/sync?";       // path to fetch timestamp from server
//***********************************************************************************************************************
//----------------------------------------------------------------------------------------------------------------------
int redial_interval=60000;                              // code checks for the internet conectivity in every 60 sec .
int timestamp_sync_interval=20000;                      // code checks for the server configuration in  every 60 sec .
int last_redial_millis=0;                               
int last_sync_millis=0;
int last_read_millis=0;
int lastTimeMillis =0;
bool is_correct_config=true;                            // to check configuration saved in device matches to server configuration
bool is_config_mode = false;                            // to detect that the logger is needed to configure
//***********************************************************************************************************************
//-------------------------------------------------------------------------------------------------------------------------
bool flag_net_wifi =false;                // flag used to detect internet using wifi
bool flag_net_GSM = false;               // flag used to detect internet using gsm
bool flag_net_ethernet = false;           // flag used to detect internet using ethernet
bool flag_present_sd = false;             // flag to detect presence of sd card
bool flag_present_wifi = false;             // flag to detect presence of wifi
bool flag_present_ethernet = false;             // flag to detect presence of ethernet
bool flag_present_gsm = false;             // flag to detect presence of gsm
//*********************************************************************************************************************** 
//---------------------------------------------------------------------------------------------------------
bool DAY_FLAG=0,MONTH_FLAG=0,YEAR_FLAG=0,HOUR_FLAG;
uint16_t lastUploadYear,lastUploadCount;
uint8_t lastUploadMonth,lastUploadDay,lastUploadHour;
//***************************************************************************************************
//----------------------------------------------------------------------------------------------------------------------
const char* default_ssid="Holmium Technologies";
const char* default_password="Tenda@HO";
const char* default_ssid_server = "Holmium Local";
const char* default_password_server = "Tenda@HO";
const char* PARAM_INPUT_1 = "testMode";
const char* PARAM_INPUT_2 = "dhcpEnable";
const char* PARAM_INPUT_3 = "ssid";
const char* PARAM_INPUT_4 = "password";
const char* PARAM_INPUT_5 = "ip";
const char* PARAM_INPUT_6 = "gateway";
const char* PARAM_INPUT_7 = "subnet";
const char* PARAM_INPUT_8 = "priDNS";
const char* PARAM_INPUT_9 = "secDNS";
//***********************************************************************************************************************
// this flag is used to verify server configuration with logger configuration
//------------------------------------------------------------------------------------------------------------------------
// function used in firmware
void printErrorMessage( uint8_t e, bool eol = true);       // print error msg during reading configuration files
bool initWiFi();                                           // initiates wifi connection
void printWiFiStatus();                                    // print various parameter like ip ,mac strength of wifi
void initWiFiGSMEtherSD();                                 // use ti initialize the hardware periferals
void reconnectWiFi();                                      // it is used to reconnect wifi to host if wifi is disconnected
int checkForInternet();                                    // To check internet is available in device
void ip2str(IPAddress ip,char *arr_return);                // it converts an ipAddress to char array
void str2ip(char *data,int *arr);                          // it converts an char array to ipAddress

void sendGETDataWiFi(char* url_path, char* data_str,char* response); //send data to server and save the server response
int loadDefaultConfig();                             // loads default configuration if logger is not configured
bool uploadManufacConfig();                           // upload manufacture configuration to server
bool uploadLoggerConfig();                            // upload logger configuration to server
bool uploadDeviceConfig();                            // upload device configuration to server
bool uploadFlagConfig();                              // uploadFlagConfiguration to server
bool uploadNetworkConfig();                           // upload network configuraiton to server
bool uploadConfigProfile();                           // function to verify the configuration on server side      
bool downLoggerConfig();                              // downloads logger configuration from server
bool downDeviceConfig();                              // downloads Device configuration from server
bool downFlagConfig();                                // downloads flag configuration from server
bool downNetworkConfig();                             // downloads network configuration from server
bool downModbusConfig();                              // downloads modbus configuration from server
int downloadConfiguration();                         // downloads all the configuration file 
int checkForserverConfig();                         // to check configuration mode normal or extended 
bool saveLoggerConfig();                              // saves logger configuration in file
bool saveDeviceConfig();                              // saves DeviceConfiguration in file
bool saveFlagConfig();                                // saves Flag configuration in file
bool saveNetworkConfig();                             // saves network configuration in file
bool saveModbusConfig();                              // saves Modbus configuration in file 
void saveConfigProfile();                            //saves all the configuration files 
void readLoggerProfile();                            // reads logger configuration from file
void readFlagProfile();                              // reads Flagconfiguration from file
void readManufacProfile();                           // reads manufacture configuration from file
void readModbusProfile();                            // reads modbus configuration from file
void readDeviceProfile(uint8_t count);               // reads device configuration from file
void readNetworkProfile();                           // reads network configuratoj from file
void displayConfigProfile();                         // reads all the configuration files
bool syncTimeFromServer();                           // syncs timestamp from server
void loadRestartData(char *Data);                    // use to make restart data format according to GSM,WIFI,Ethernet
bool uploadRestartData(char *Data);                 //  upload restart data to server according to gsm/wifi/ethernet flag
int readAndSave();                                  //it read the device data and resonsible to save or to upload
int initRS485(uint16_t baudRate,uint8_t parity);     // initiates modbus port for modbus communication       
int initSlave(uint8_t sid);                          // initiates communication with specific slave id
bool readDeviceData(clientDeviceDetails obj,char* data);   //reads modbus data of a device
int readModbusPacket(uint8_t sid, uint8_t fc, uint16_t start_add, uint16_t len,uint16_t *response);     // reada specific modbus packet
void setTimeRTC();                                  // to set time of rtc
void getTimeRTC();                                  // to get time from rtc
bool initSdCard();                                  // use to initialize sd card
bool saveToFile(char *data,uint16_t len);           // use to save data into sd card
bool saveUploadTracker();                           //use to save upload tracker into sd card
bool syncUploadTracker();                           // use to sync upload tracker from server
bool setUploadTracker();                            //use to check hour ,day ,month ,year flag
int updateUploadTracker();                          //use to update  tracker for uploading  
bool initializeTracker();                           // use to set the tracker in running firmware
int readAndUpload();                                // use to read data from sd card and upload to the server
//********************************************************************************************************************************
void setup() {
  Serial.begin(9600);
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      Serial.println("SPIFFS Mount Failed");
      //return;
    }
  pinMode(DI1,INPUT_PULLUP);
  displayConfigProfile();
  if (logger.is_configured==0)
    loadDefaultConfig();
  initWiFiGSMEtherSD();
  getTimeRTC();
  char restartData[650];
  if (flag.is_gsm==1)
  {
     loadRestartData(restartData);
     checkForInternet();
  }
  else
  {
    checkForInternet();
    loadRestartData(restartData);  
  }
  
  if(!uploadRestartData(restartData))
   {
    if (flag.is_sdcard==true)
        {
           Serial.println("saving restart request");
           //now set the upload tracker
           if(flag_present_sd==true)
           {
              saveToFile(restartData,sizeof(restartData));
           }
        }
   }
  checkForserverConfig();
  //uploadConfigProfile();
  initRS485(modbus.baud_rate,modbus.parity);
  if (syncTimeFromServer())
  {
    setTimeRTC();
  }
  initializeTracker();
  last_sync_millis=millis();
  last_redial_millis=millis();
  last_read_millis=millis();
}
void loop(){
  if (flag.is_active==1)
  {
    if (is_correct_config==true)
    {
      if ((millis()-last_redial_millis)>=redial_interval)
      {
        reconnectWiFi();
        last_redial_millis=millis();
      }
      if ((millis()-last_sync_millis)>=timestamp_sync_interval)
      {
        
        checkForserverConfig();
        syncTimeFromServer();
        initializeTracker();
        last_sync_millis=millis();
      }
      if ((millis()-last_read_millis)>=(logger.read_interval*1000))
      {
        if(flag.is_rms)
        {
          Serial.println("Reading devices");
          readAndSave();
          last_read_millis=millis();
        }
      }
      else
      {
        if(millis()-lastTimeMillis>=20000)
        {
          lastTimeMillis=millis();
          Serial.print("Clock:");
          Serial.print(hour());
          Serial.print(":");
          Serial.print(minute());
          Serial.print(":");
          Serial.println(second());
          Serial.println(F("Time Left to read device:"));        
          byte m=(byte)(((logger.read_interval-(millis()-last_read_millis))/1000)/60);
          byte s=(byte)(((logger.read_interval-(millis()-last_read_millis))/1000)%60);
          Serial.print(m);Serial.print(F(":"));
          if(s<10)
          Serial.print(F("0"));
          Serial.println(s);        
        }
        if(flag.is_sdcard==true)
        {
          //Serial.println(F("Uploading file data"));
          if(flag_present_sd==true)
            readAndUpload(); 
        }      
      }  
    }
  } 
}
void printErrorMessage( uint8_t e,bool eol )
{
  switch (e) {
  case SPIFFSIniFile::errorNoError:
    Serial.print("no error");
    break;
  case SPIFFSIniFile::errorFileNotFound:
    Serial.print("file not found");
    break;
  case SPIFFSIniFile::errorFileNotOpen:
    Serial.print("file not open");
    break;
  case SPIFFSIniFile::errorBufferTooSmall:
    Serial.print("buffer too small");
    break;
  case SPIFFSIniFile::errorSeekError:
    Serial.print("seek error");
    break;
  case SPIFFSIniFile::errorSectionNotFound:
    Serial.print("section not found");
    break;
  case SPIFFSIniFile::errorKeyNotFound:
    Serial.print("key not found");
    break;
  case SPIFFSIniFile::errorEndOfFile:
    Serial.print("end of file");
    break;
  case SPIFFSIniFile::errorUnknownError:
    Serial.print("unknown error");
    break;
  default:
    Serial.print("unknown error value");
    break;
  }
  //if (eol)
    //Serial.println();
}
void checkConfigMode()
{
  int count=0;
  while (digitalRead(DI1)==HIGH)
  {
    count++;
    Serial.print(".");
    delay(1000);
    if (count==10)
    {
      Serial.println("Entered in Config Mode");
      while(1);
    }
  }
}

bool initWiFi()
{
  int fail_count=0;
  String hostname = "HolmiumLogger";
  WiFi.mode(WIFI_STA);
  Serial.println(WiFi.getHostname());
  WiFi.setHostname(hostname.c_str()); //define hostname
  Serial.print("new host name =");
  Serial.println(WiFi.getHostname());
  IPAddress ip;
  Serial.print("dhcp enable flag=");
  Serial.println(network.dhcp_enable);
  Serial.println(network.ssid);
  Serial.println(network.password);
  //Serial.println(typeof(network.dhcp_enable));
  if (network.dhcp_enable==false)
  {
    int arr_ip[4];
    int arr_gateway[4];
    int arr_subnet[4];
    int arr_priDNS[4];
    int arr_secDNS[4];
    str2ip(network.ip_wifi,arr_ip);
    str2ip(network.gateway_wifi,arr_gateway);
    str2ip(network.subnet_wifi,arr_subnet);
    str2ip(network.priDNS_wifi,arr_priDNS);
    str2ip(network.secDNS_wifi,arr_priDNS);
    IPAddress local_ip(arr_ip[0],arr_ip[1],arr_ip[2],arr_ip[3]);
    IPAddress gateway(arr_gateway[0],arr_gateway[1],arr_gateway[2],arr_gateway[3]);
    IPAddress subnet(arr_subnet[0],arr_subnet[1],arr_subnet[2],arr_subnet[3]);
    IPAddress priDNS(arr_priDNS[0],arr_priDNS[1],arr_priDNS[2],arr_priDNS[3]);
    IPAddress secDNS(arr_secDNS[0],arr_secDNS[1],arr_secDNS[2],arr_secDNS[3]);
    Serial.println("configuring wifi network ");
    //if (!WiFi.config(ip.fromString(String(network.ip_wifi)), ip.fromString(String(network.gateway_wifi)), ip.fromString(String(network.subnet_wifi)), ip.fromString(String(network.priDNS_wifi)), ip.fromString(String(network.secDNS_wifi)))) {
     if (!WiFi.config(local_ip, gateway, subnet, priDNS, secDNS)) 
    {  
      Serial.println("STA Failed to configure");
    }
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Connecting to..... ");
    Serial.println(network.ssid);
    Serial.println(network.password);
    WiFi.begin(network.ssid, network.password);
    while (WiFi.status() != WL_CONNECTED) {
      
      Serial.print(".");
      fail_count++;
      delay(1000);
      if (fail_count==15)
        break;
    } 
  }
  else
  {
    Serial.println("Connecting with dhcp ");
    WiFi.begin("Holmium Technologies","Tenda@HO" );
    while (WiFi.status() != WL_CONNECTED) {
      fail_count++;
      Serial.print(".");
      delay(1000);
      if (fail_count==15)
        break;
    }
  }
  if(WiFi.status() == WL_CONNECTED)
  {
    printWiFiStatus();
    return true;
  }
  else
  {
    Serial.println("Failed to connect wifi network");
    return false;
  }
}

void str2ip(char *data,int *arr)
{
  Serial.println(data);
  char *pt;
  pt = strtok(data,".");
  arr[0]=atoi(pt);
  //Serial.println(arr[0]);
  pt = strtok(NULL,".");
  arr[1]=atoi(pt);
  //Serial.println(arr[1]);
  pt = strtok(NULL,".");
  arr[2]=atoi(pt);
  //Serial.println(arr[2]);
   pt = strtok(NULL,".");
  arr[3]=atoi(pt);
  //Serial.println(arr[3]);
}
void ip2str(IPAddress ip,char *arr_return)
{
  char arr[10]="";
  strcat(arr_return,itoa(ip[0],arr,10));
  strcat(arr_return,".");
  strcat(arr_return,itoa(ip[1],arr,10));
  strcat(arr_return,".");
  strcat(arr_return,itoa(ip[2],arr,10));
  strcat(arr_return,".");
  strcat(arr_return,itoa(ip[3],arr,10));
  //Serial.print("arr_return =");
  //Serial.println(arr_return);
}
void printWiFiStatus()
{
  Serial.println("printWiFiStatus");
  Serial.print("WiFi ssid = ");
  Serial.println(WiFi.SSID());
  Serial.print("WiFi local ip =");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi gateway =");
  Serial.println(WiFi.gatewayIP());
  Serial.print("WiFi subnet mask =");
  Serial.println(WiFi.subnetMask());
  Serial.print("WiFi mac address = ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi strength =");
  Serial.println(WiFi.RSSI());
}
void initWiFiGSMEtherSD()
{
  if (flag.is_wifi)
  {
    if(initWiFi())
      flag_present_wifi=true;
  }
  if (flag.is_gsm)
    Serial.println("initializing gsm");
  if (flag.is_ethernet)
    Serial.println("initializing ethernet");
  if (flag.is_sdcard)
  {
    Serial.println("initializing sd card");
    if(initSdCard())
      flag_present_sd=true;
  }
}
void reconnectWiFi()
{ Serial.println("reconnectWiFi");
  if (WiFi.status()!=WL_CONNECTED)
  {
    Serial.println("wifi disconnected trying to reconnect wait 20sec");
    WiFi.reconnect();
    delay(20000);
    if(WiFi.status()!=WL_CONNECTED)
      Serial.println("not connected");
    else
      Serial.println("connected");
  }
  else
  Serial.println("wifi already connected");
}

void sendGETDataWiFi(char* url_path, char* data_str,char* response)
{  
   //Serial.print("url path=");
   //Serial.println(url_path);
   Serial.print("Data=");
   Serial.println(data_str);
   Serial.println("\nStarting connection to server...");
   // if you get a connection, report back via serial:
   bool conn=false;
   int count_fail=0;
   do {
    conn = client.connect(server,80);
    if (conn!=1)
      ++count_fail;
    Serial.print("count_fail = ");
    Serial.println(count_fail);
    if (count_fail==5)
      break;
   }while(conn=false);
   if (conn=1) {
      Serial.println("connected to server");
      // Make a HTTP request:
      //client.println("GET /rms/api/insertdata?LD=1,302,0,0,2,0 HTTP/1.1");
      client.println("GET "+String(url_path)+String(data_str)+"\n"+" HTTP/1.1");
      client.println("Host: www.holmiumtechnologies.com");
      client.println("Connection: close");
      client.println();
      while(client.connected()){
        int i=0;
        while (client.available()) {
        
          char c = client.read();
          
          //Serial.write(c);
          if(i<200)
            response[i] = c;
          else
            break;
          i=i+1;
        }
        
     }
    Serial.print(response);
   
    // if the server's disconnected, stop the client:
  
    if (!client.connected()) {
      Serial.println();
  
      Serial.println("disconnecting from server.");
      
      client.stop();
    }
  }
  else
  {
    Serial.println("unable to connect to server");
    client.stop();
  }
}
int checkForInternet()
{
  Serial.println("\n checkForInternet function");
   bool conn=false;
   int count_fail=0;
   if (flag.is_wifi==1)
   {
    Serial.println("checking wifi internet");
     do {
      conn = client.connect(server,80);
      //Serial.print("conn=");
      //Serial.println(conn);
      if (conn!=1)
      {
        ++count_fail;
        Serial.print("count_fail = ");
        Serial.println(count_fail);
        delay(2000);
      }
      else{
        Serial.println("internet available in device");
        flag_net_wifi = true;
        break;
      }
      if (count_fail==5)
      {
        Serial.println("internet not available");
        flag_net_wifi =false;
        break;
      }
     }while(conn==false);
   }
}
/*
void sendPOSTDataWiFi(char* url_path, char* data_str,char* response)
{  
   Serial.print("URL path=");
   Serial.println(url_path);
   Serial.print("Data=");
   Serial.println(data_str);
   Serial.println("\nStarting connection to server...");
   // if you get a connection, report back via serial:
   if (client.connect(server, 80)) {
      Serial.println("connected to server");
      // Make a HTTP request:
      //client.println("GET /rms/api/insertdata?LD=1,302,0,0,2,0 HTTP/1.1");
      client.println("POST " +String(url_path)+"HTTP/1.1");
      client.println("Host: www.holmiumtechnologies.com");
      client.println("User-Agent: esp32/1.0");
      client.println("Content-Type: text/html; charset=us-ascii");
      client.print("Content-Length: 500");
      client.println();
      client.println(data_str);
      while(client.connected()){
      int i=0;
      while (client.available()) {
      
        char c = client.read();
        
        //Serial.write(c);
        response[i] = c;
        i=i+1;
      }
     }
    client.println("Connection: close");
    Serial.print(response);
    // if the server's disconnected, stop the client:
  
    if (!client.connected()) {
      Serial.println();
  
      Serial.println("disconnecting from server.");
      
      client.stop();
    }
  }
}
*/
int loadDefaultConfig()
{
  //manufacturer configuration
  Serial.println(F("Loading Default configuration"));
  strcpy(mfc.serial_no,"HO-K301501");
  strcpy(mfc.firmware_version,"0.9.0");  
  //control flag
  flag.is_active = HIGH;
  flag.is_rms    = HIGH;
  flag.is_sdcard = HIGH;
  flag.is_dgsync = LOW;
  flag.is_zeroexport=LOW;
  flag.is_gsm=LOW;
  flag.is_wifi=HIGH;
  flag.is_ethernet=LOW;
  //define default logger profile
  logger.is_configured=LOW;
  logger.plant_id=302;
  logger.logger_id=1;
  logger.device_count=1;
  logger.read_interval=60;
  //define device profile
  device[0].device_index=0;
  device[0].device_type=50;
  device[0].fun_code=3;
  device[0].device_id=1;
  device[0].slave_id=1;
  device[0].bin_count=2;
  strcpy(device[0].bin_start,"0,25");
  strcpy(device[0].bin_length,"25,25");
  strcpy(device[0].ip_address,"192.168.10.1");
  strcpy(device[0].port,"serial1");
  strcpy(device[0].protocol,"serial");
  //define modbus parameters  
  modbus.baud_rate=9600;
  modbus.poll_rate=250;
  modbus.timeout=1000;
  modbus.parity=0;//0-none,1-odd,2-even
  //define network parameter
  network.dhcp_enable=1;
  return 0;
}

void append(char* data1,int data2, char* delim  )
{
  char int_arr[5];
  itoa(data2,int_arr,10);
  strcat(data1,int_arr);
  strcat(data1,delim);
}
void append(char* data1,int8_t data2, char* delim  )
{
  char int_arr[5];
  itoa(data2,int_arr,10);
  strcat(data1,int_arr);
  strcat(data1,delim);
}
void append(char* data1,char* data2, char *delim  )
{
  strcat(data1,data2);
  strcat(data1,delim);
}

void append(char* data1,bool data2, char *delim  )
{
  char bool_arr[1];
  itoa(data2,bool_arr,10);
  strcat(data1,bool_arr);
  strcat(data1,delim);
}

void append(char *data1,uint8_t data2,char *delim)
{
  char arr[6];
  itoa(data2,arr,10);  
  strcat(data1,arr);
  strcat(data1,delim);
  }
void append(char *data1,uint16_t data2,char *delim)
{
  char arr[6];
  itoa(data2,arr,10);  
  strcat(data1,arr);
  strcat(data1,delim);
  }
void append(char *data1,long data2,char *delim)
{
  char arr[6];
  ltoa(data2,arr,10);  
  strcat(data1,arr);
  strcat(data1,delim);
  }
bool uploadManufacConfig()
{
  Serial.println("uploadManufacConfig function");
  char response[250]="";
  char data[100]="serialNo=";
  strcat(data,mfc.serial_no);
  strcat(data,"&command=MFCV&configuration=");
  strcat(data,mfc.serial_no);
  strcat(data,",");
  strcat(data,mfc.firmware_version);
  if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  if(strstr(response,"response=ERROR")!=NULL)
  {
    Serial.println("manufac profile upload successful");
    return false;
  }
  else
  return true;
}
bool uploadLoggerConfig()
{
  Serial.println("uploadLoggerConfig function");
  char response[250]="";
  char data[100]="serialNo=";
  strcat(data,mfc.serial_no);
  strcat(data,"&command=LSCV&configuration=");
  //append(data,logger.is_configured,",");
  append(data,logger.plant_id,",");
  append(data,logger.logger_id,",");
  append(data,logger.device_count,",");
  append(data,logger.read_interval,"");
  if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  if(strstr(response,"response=ERROR")!=NULL)
  {
    Serial.println("Logger profile upload successful");
    return false;
  }
  else
    return true;
}
bool uploadFlagConfig()
{
  Serial.println("uploadFlagConfig function");
  char response[250];
  char data[100]="serialNo=";
  strcat(data,mfc.serial_no);
  strcat(data,"&command=CFCV&configuration=");
  append(data,flag.is_active,",");
  append(data,flag.is_sdcard,",");
  append(data,flag.is_rms,",");
  append(data,flag.is_dgsync,",");
  append(data,flag.is_zeroexport,",");
   append(data,flag.is_gsm,",");
  append(data,flag.is_wifi,",");
  append(data,flag.is_ethernet,"");
  if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  if(strstr(response,"response=ERROR")!=NULL)
  {
    Serial.println("flag profile upload successful");
    return false;
  }
  else
    return true;  
}
bool uploadModbusConfig()
{
  Serial.println("uploadModbusConfig function");
  char data[200]="serialNo=";
  char response[250]="";
  strcat(data,mfc.serial_no);
  strcat(data,"&command=UPLOADMODBUS&configuration=");
  append(data,modbus.baud_rate,",");
  append(data,modbus.parity,",");
  append(data,modbus.poll_rate,",");
  append(data,modbus.timeout,"");   
  if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  if(strstr(response,"response=ERROR")!=NULL) 
  {
    Serial.println("modbus profile upload successful");
    return false;
  }
  else
    return true; 
}
bool uploadDeviceConfig()
{
  Serial.println("Upload device config function");
  int result=false;
  char response[250]="";
  
  for(int i=0;i<logger.device_count;i++)
  {
    char data[650];
    memset(data,0,650);
    strcat(data,"serialNo=");
    strcat(data,mfc.serial_no);
    strcat(data,"&command=SDCV&deviceIndex=");
    append(data,i,"&configuration=");
    append(data,device[i].device_index,",");
    append(data,device[i].device_type,",");
    append(data,device[i].fun_code,",");
    append(data,device[i].device_id,",");
    append(data,device[i].slave_id,",");
    append(data,device[i].bin_count,",");
    append(data,device[i].bin_start,",");
    append(data,device[i].bin_length,",");
    strcat(data,device[i].ip_address);
    strcat(data,",");
    strcat(data,device[i].port);
    strcat(data,",");
    strcat(data,device[i].protocol);
    if (WiFi.status() == WL_CONNECTED)
      sendGETDataWiFi(config_path , data, response);
    if(strstr(response,"response=ERROR")!=NULL)
    {
      Serial.println("upload device config");
      return false;
    }   
  }
  return result;  
}
bool uploadNetworkConfig()
{
  Serial.println("uploadNetworkConfig function ");
  char response[150];
  char data[100]="serialNo=";
  strcat(data,mfc.serial_no);
  strcat(data,"&command=NSCV&configuration=");
  append(data,network.dhcp_enable,",");
  append(data,network.ssid,",");
  append(data,network.password,",");
  append(data,network.mac_wifi,",");
  append(data,network.ip_wifi,",");
  append(data,network.gateway_wifi,",");
  append(data,network.subnet_wifi,",");
  append(data,network.priDNS_wifi,",");
  append(data,network.secDNS_wifi,",");
  append(data,network.mac_ether,",");
  append(data,network.ip_ether,",");
  append(data,network.gateway_ether,",");
  append(data,network.subnet_ether,",");
  append(data,network.priDNS_ether,",");
  append(data,network.secDNS_ether,",");
  if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  if(strstr(response,"response=ERROR")!=NULL)
  {
    Serial.println("network profile upload failed");
    return false;
  }
  else
    return true;  
}
bool uploadConfigProfile()
{
  if(uploadManufacConfig()&&uploadFlagConfig()&&uploadLoggerConfig()&&uploadDeviceConfig())
    is_correct_config=true;
  else
  is_correct_config=false;  
  return is_correct_config;
}


bool writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return false;
    }
    if(file.print(message)){
        Serial.println("- file written successful");
        return true;
    } else {
        Serial.println("- write failed");
        return false;
    }
    file.close();
}

bool downLoggerConfig()
{
  Serial.println("\ndownLoggerConfig function");
  char command[50]="serialNo=";
  char response[250]="serialNo=HO-K301502&response=1,302,1,1,20\n";  
  //char response[250]="";
  strcat(command,mfc.serial_no);
  strcat(command,"&command=LSC");
  for(int i=0;i<2;i++)
  {
    if (WiFi.status() == WL_CONNECTED)
      //sendGETDataWiFi(config_path , command, response);
    i=2;
    if(strstr(response,mfc.serial_no)!=NULL)
    {
      if(strstr(response,"&response=")!=NULL)
      {
        char *resp;
        resp = strtok(response,"=");
        resp = strtok(NULL,"=");
        char *ret;
        ret = strtok(NULL,"\n");
        char *pt;   
        pt = strtok (ret,",");   
        logger.is_configured=atoi(pt); 
        Serial.print("is configured=");
        Serial.println(logger.is_configured);
        pt = strtok (NULL,",");    
        logger.plant_id=atoi(pt);
        //Serial.println(logger.plant_id);
        pt = strtok (NULL,",");  
        logger.logger_id=atoi(pt);
        //Serial.println(logger.logger_id);
        pt = strtok (NULL,",");  
        logger.device_count=atoi(pt); 
        //Serial.println(logger.device_count);
        pt = strtok (NULL,",");  
        logger.read_interval=atoi(pt);
        //Serial.println(logger.read_interval);
        pt = strtok (NULL,",");  
        if(pt==NULL)
        {
          Serial.println("logger profile download success "); 
          return true;
        } 
      }   
    }
  }
  return false;
}

bool saveLoggerConfig()
{
  Serial.println("saveLoggerConfig function");
  char arr[6];
  char logger_str[150]="[loggerProfile]\n";
  
  strcat(logger_str,"isConfigured=");
  strcat(logger_str,itoa(logger.is_configured,arr,10));
  strcat(logger_str,"\n");
  strcat(logger_str,"plantId=");
  strcat(logger_str,itoa(logger.plant_id,arr,10)); 
  strcat(logger_str,"\n");
  strcat(logger_str,"loggerId="); 
  strcat(logger_str,itoa(logger.logger_id,arr,10));
  strcat(logger_str,"\n");
  strcat(logger_str,"deviceCount=");
  strcat(logger_str,itoa(logger.device_count,arr,10));
  strcat(logger_str,"\n");
  strcat(logger_str,"readInterval=");
  strcat(logger_str,itoa(logger.read_interval,arr,10));
  strcat(logger_str,"\n");
  Serial.println(logger_str);
  if(writeFile(SPIFFS, "/logger.ini", logger_str))
  return true;
  
}

bool downFlagConfig()
{
  Serial.println("\ndownFlagConfig function");
  char response[250]="serialNo=HO-K301502&response=1,1,1,1,0,1,0,0\n";  
  //char response[250]="";
  char command[50]="serialNo=";  
  strcat(command,mfc.serial_no);
  strcat(command,"&command=CFC");
  for(int i=0;i<2;i++)
  {
    if (WiFi.status() == WL_CONNECTED)
    //sendGETDataWiFi(config_path , command, response);
    if(strstr(response,mfc.serial_no)!=NULL)
    {
      i=2;
      if(strstr(response,"&response=")!=NULL)
      {
        char *resp;
        resp = strtok(response,"=");
        resp = strtok(NULL,"=");
        resp = strtok(NULL,"=");
        char *ret;
        ret = strtok(resp, "\n");
        //Serial.println(ret);
        char *pt;   
        pt = strtok (ret,",");       
        flag.is_active=atoi(pt); 
        //Serial.println(flag.is_active);
        pt = strtok (NULL,",");  
        flag.is_sdcard=atoi(pt); 
        //Serial.println(flag.is_sdcard);
        pt = strtok (NULL,",");  
        flag.is_rms=atoi(pt); 
        //Serial.println(flag.is_rms);
        pt = strtok (NULL,",");  
        flag.is_dgsync=atoi(pt); 
        //Serial.println(flag.is_dgsync);
        pt = strtok (NULL,",");  
        flag.is_zeroexport=atoi(pt); 
        //Serial.println(flag.is_zeroexport);
        pt =strtok (NULL,",");  
        flag.is_wifi=atoi(pt); 
        //Serial.println(flag.is_wifi);
        pt =strtok (NULL,",");  
        flag.is_ethernet=atoi(pt); 
        //Serial.println(flag.is_ethernet);
        pt = strtok (NULL,",");  
        flag.is_gsm=atoi(pt); 
        //Serial.println(flag.is_gsm);
        pt = strtok (NULL,","); 
        if(pt==NULL)
        {
          Serial.println("down flag config successful");
          return true;
        }
     }
   } 
 } 
 return false;
}

bool saveFlagConfig()
{
  Serial.println("saveFlagConfig function");
  char arr[2];
  char flag_str[150]="[flagProfile]\n";
  strcat(flag_str,"isActive=");
  strcat(flag_str,itoa(flag.is_active,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isSdCard=");
  strcat(flag_str,itoa(flag.is_sdcard,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isRMS=");
  strcat(flag_str,itoa(flag.is_rms,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isDgSync=");
  strcat(flag_str,itoa(flag.is_dgsync,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isZeroExport=");
  strcat(flag_str,itoa(flag.is_zeroexport,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isWiFi=");
  strcat(flag_str,itoa(flag.is_wifi,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isEthernet=");
  strcat(flag_str,itoa(flag.is_ethernet,arr,10)); 
  strcat(flag_str,"\n");
  strcat(flag_str,"isGSM=");
  strcat(flag_str,itoa(flag.is_gsm,arr,10)); 
  strcat(flag_str,"\n");
  Serial.println(flag_str);
  if(writeFile(SPIFFS, "/flag.ini", flag_str))
    return true;
  else
    return false;
}

bool downNetworkConfig()
{
  Serial.println("\ndownNetworkConfig function");
  char response[250]="serialNo=HO-K301502&response=1,Skyworth_0CA9DC,passwOrd,xx:xx:xx:xx:xx,192.168.1.10,192.168.1.1,255.255.255.0,8.8.8.8,8.8.8.8,xx:xx:xx:xx,192.168.1.10,192.168.1.1,255.255.255.0,8.8.8.8,8.8.8.8\n";  
  //char response[250]="";
  char command[50]="serialNo=";  
  strcat(command,mfc.serial_no);
  strcat(command,"&command=NFC");
  for(int i=0;i<2;i++)
  {
    if (WiFi.status() == WL_CONNECTED)
    //sendGETDataWiFi(config_path , command, response);
    if(strstr(response,mfc.serial_no)!=NULL)
    {
      i=2;
      if(strstr(response,"&response=")!=NULL)
      {
        char *resp;
        resp = strtok(response,"=");
        resp = strtok(NULL,"=");
        resp = strtok(NULL,"=");
        char *ret;
        ret = strtok(resp, "\n");
        char *pt;   
        pt = strtok (ret,",");       
        network.dhcp_enable=atoi(pt); 
        //Serial.println(network.dhcp_enable);
        pt = strtok (NULL,",");  
        strcpy(network.ssid,pt); 
        //Serial.println(network.ssid);
        pt = strtok (NULL,",");  
        strcpy(network.password,pt);
        //Serial.println(network.password);
        
        pt = strtok (NULL,",");  
        strcpy(network.mac_wifi,pt);
        //Serial.println(network.mac_wifi);
        pt = strtok (NULL,",");  
        strcpy(network.ip_wifi,pt);
        //Serial.println(network.ip_wifi);
        pt =strtok (NULL,",");  
        strcpy(network.gateway_wifi,pt);
        //Serial.println(network.gateway_wifi);
        pt =strtok (NULL,",");  
        strcpy(network.subnet_wifi,pt);
        //Serial.println(network.subnet_wifi);
        pt = strtok (NULL,",");  
        strcpy(network.priDNS_wifi,pt); 
        //Serial.println(network.priDNS_wifi);
        pt = strtok (NULL,","); 
        strcpy(network.secDNS_wifi,pt);  
        //Serial.println(network.secDNS_wifi);
        pt = strtok (NULL,",");  
        strcpy(network.mac_ether,pt); 
        //Serial.println(network.mac_ether);
        pt = strtok (NULL,",");  
        strcpy(network.ip_ether,pt);
        //Serial.println(network.ip_ether);
        pt = strtok (NULL,",");  
        strcpy(network.gateway_ether,pt); 
        //Serial.println(network.gateway_ether);
        pt = strtok (NULL,",");  
        strcpy(network.subnet_ether,pt); 
        //Serial.println(network.subnet_ether);
        pt = strtok (NULL,",");  
        strcpy(network.priDNS_ether,pt); 
        //Serial.println(network.priDNS_ether);
        pt = strtok (NULL,",");  
        strcpy(network.secDNS_ether,pt);
        //Serial.println(network.secDNS_ether);
        pt = strtok (NULL,",");  
        if(pt==NULL)
        { 
          Serial.println("down network config success");
          return true;
        }
     }
   } 
 } 
 return false;
}

bool saveNetworkConfig()
{
  Serial.println("\nsaveNetworkConfig function");
  char arr[2];
  char network_str[500]="[NetworkProfile]\n";
  strcat(network_str,"dhcpEnable=");
  itoa(network.dhcp_enable,arr,10);
  strcat(network_str,arr); 
  strcat(network_str,"\n\n[wifi]\n");
  strcat(network_str,"ssid=");
  strcat(network_str,network.ssid); 
  strcat(network_str,"\n");
  strcat(network_str,"password=");
  strcat(network_str,network.password); 
  strcat(network_str,"\n");
  strcat(network_str,"macWiFi=");
  strcat(network_str,network.mac_wifi); 
  strcat(network_str,"\n");
  strcat(network_str,"ipAddress=");
  strcat(network_str,network.ip_wifi); 
  strcat(network_str,"\n");
  strcat(network_str,"gateway=");
  strcat(network_str,network.gateway_ether); 
  strcat(network_str,"\n");
  strcat(network_str,"subnet=");
  strcat(network_str,network.subnet_wifi); 
  strcat(network_str,"\n");
  strcat(network_str,"primaryDNS=");
  strcat(network_str,network.priDNS_wifi); 
  strcat(network_str,"\n");
  strcat(network_str,"secondaryDNS=");
  strcat(network_str,network.secDNS_wifi); 
  strcat(network_str,"\n\n[ethernet]\n");
  strcat(network_str,"macEther=");
  strcat(network_str,network.mac_ether); 
  strcat(network_str,"\n");
  strcat(network_str,"ipAddress=");
  strcat(network_str,network.ip_ether); 
  strcat(network_str,"\n");
  strcat(network_str,"gateway=");
  strcat(network_str,network.gateway_ether); 
  strcat(network_str,"\n");
  strcat(network_str,"subnet=");
  strcat(network_str,network.subnet_ether); 
  strcat(network_str,"\n");
  strcat(network_str,"primaryDNS=");
  strcat(network_str,network.priDNS_ether); 
  strcat(network_str,"\n");
  strcat(network_str,"secondaryDNS=");
  strcat(network_str,network.secDNS_ether); 
  strcat(network_str,"\n");
  Serial.println(network_str);
  if(writeFile(SPIFFS, "/network.ini", network_str))
  return true;
  else
  return false;
  
}

bool downModbusConfig()
{
  Serial.println("\ndownModbusConfig function");
  //char response[250]="serialNo=HO-K301503&response=9600,250,300,0";
  char response[250]="";
  char command[50]="serialNo=";
  strcat(command,mfc.serial_no);
  strcat(command,"&command=DOWNLOADMODBUS");
  for(int k=0;k<2;k++)
  {
     if (WiFi.status() == WL_CONNECTED)
     sendGETDataWiFi(config_path , command, response);
     if(strstr(response,mfc.serial_no)!=NULL)
     {
      if(strstr(response,"response=")!=NULL)
      {
        char *resp;
        resp = strtok(response,"=");
        resp = strtok(NULL,"=");
        resp = strtok(NULL,"=");
        char *ret;
        ret = strtok(resp,"\n");  
        Serial.println(ret);
        char *pt;
        pt = strtok(ret,",");
        modbus.baud_rate=atoi(pt);
        Serial.println(modbus.baud_rate);
        pt = strtok(NULL,",");
        modbus.parity=atoi(pt);
        Serial.println(modbus.parity);
        pt = strtok(NULL,",");
        
        modbus.poll_rate=atoi(pt);
        Serial.println(modbus.poll_rate);
        pt = strtok(NULL,",");
        modbus.timeout=atoi(pt);
        Serial.println(modbus.timeout);
        pt = strtok(NULL,",");
        if(pt==NULL)
        {
          Serial.println("download modbus config successful");
          return true;
        }
       } 
     }
  }
  return false;
}

bool saveModbusConfig()
{
   Serial.println("saveModbusConfig function");
   char arr[6];
   char modbus_str[70]="[modbusProfile]\n";
   strcat(modbus_str,"baudrate=");
   strcat(modbus_str,itoa(modbus.baud_rate,arr,10));
   strcat(modbus_str,"\n");
   strcat(modbus_str,"pollrate=");
   strcat(modbus_str,itoa(modbus.poll_rate,arr,10));
   strcat(modbus_str,"\n");
   strcat(modbus_str,"timeout=");
   strcat(modbus_str,itoa(modbus.timeout,arr,10));
   strcat(modbus_str,"\n");
   strcat(modbus_str,"parity=");
   strcat(modbus_str,itoa(modbus.parity,arr,10));
   strcat(modbus_str,"\n");
   Serial.println(modbus_str);
   if (writeFile(SPIFFS, "/modbus.ini", modbus_str))
   return true;
   else
   return false;
   
}


bool downDeviceConfig()
{  
  Serial.println("\ndownDeviceConfig function");
  bool isCorrect = false;
  char response[250]="serialNo=HO-K301502&response=0,50,3,1,1,2,0,25,25,25,192.168.10.1,usb2485,serial";
  //char response[250]="";
  //check for CNFCHK  
  //Serial.print("total devices to download=");
  //Serial.println(logger.device_count);
  for(int i=0;i<logger.device_count;i++)
  { 
    char command[50]="serialNo=";    
    strcat(command,mfc.serial_no);
    strcat(command,"&command=SDC&deviceIndex=");
    char c[3];
    itoa(i,c,10);
    strcat(command ,c);
    for(int k=0;k<2;k++)
    {
      if (WiFi.status() == WL_CONNECTED)
        //sendGETDataWiFi(config_path , command, response);
      
      if(strstr(response,mfc.serial_no)!=NULL)
      {
        if(strstr(response,"&response=")!=NULL)
        {
          char *resp;
          resp = strtok(response,"=");
          resp = strtok(NULL,"=");
          resp = strtok(NULL,"=");
          //Serial.println(resp);
          char *ret;
          ret = strtok(resp,"\n");
          //Serial.println(ret);
          char *pt;   
          pt = strtok (ret,",");
          device[i].device_index=atoi(pt); 
          //Serial.println(device[i].device_index);
          pt = strtok (NULL,",");  
          device[i].device_type=atoi(pt); 
          //Serial.println(device[i].device_type);
    
          pt = strtok (NULL,",");  
          device[i].fun_code=atoi(pt); 
          //Serial.println(device[i].fun_code);
          pt = strtok (NULL,",");  
          device[i].device_id=atoi(pt);
          //Serial.println(device[i].device_id);
          pt = strtok (NULL,",");  
          device[i].slave_id=atoi(pt);
          //Serial.println(device[i].slave_id);
          pt = strtok (NULL,",");  
          device[i].bin_count=atoi(pt);
          //Serial.println(device[i].bin_count);
          char arr_start[20]="";
          for(int j=0;j<device[i].bin_count;j++)
          {
            pt = strtok (NULL,",");  
            strcat(arr_start,pt);
            if (j!=((device[i].bin_count)-1))
            strcat(arr_start,",");
            
          }
          strcpy(device[i].bin_start,arr_start);
          //Serial.println(device[i].bin_start);
          char arr_length[20]="";
          for(int j=0;j<device[i].bin_count;j++)
          {
            pt = strtok (NULL,",");  
            strcat(arr_length,pt);
            if (j!=((device[i].bin_count)-1))
            strcat(arr_length,",");
          }
          strcpy(device[i].bin_length,arr_length);
          //Serial.println(device[i].bin_length);
          pt = strtok (NULL,",");  
          strcpy(device[i].ip_address,pt);
          //Serial.println(device[i].ip_address);
          pt = strtok (NULL,",");  
          strcpy(device[i].port,pt);
          //Serial.println(device[i].port);
          pt = strtok (NULL,",");  
          strcpy(device[i].protocol,pt);
          //Serial.println(device[i].protocol);
          pt = strtok (NULL,",");  
          if(pt!=NULL)
          return false; 
       }
      }
     }     
 }  
 return true;
}

bool saveDeviceConfig()
{
  Serial.println("\nsaveDeviceConfig function");
  char device_str[650]="[deviceProfile]\n";
  char arr[6];
  for (int i=0;i<logger.device_count;i++)
  {
    strcat(device_str,"[device");
    strcat(device_str,itoa((i+1),arr,10));
    strcat(device_str,"]\n");
    strcat(device_str,"deviceIndex=");
    itoa(device[i].device_index,arr,10);
    strcat(device_str,arr);
    strcat(device_str,"\n");
    strcat(device_str,"deviceType=");
    itoa(device[i].device_type,arr,10);
    strcat(device_str,arr);
    strcat(device_str,"\n");
    strcat(device_str,"funCode=");
    itoa(device[i].fun_code,arr,10);
    strcat(device_str,arr);
    strcat(device_str,"\n");
    strcat(device_str,"deviceId=");
    itoa(device[i].device_id,arr,10);
    strcat(device_str,arr);
    strcat(device_str,"\n");
    strcat(device_str,"slaveId=");
    itoa(device[i].slave_id,arr,10);
    strcat(device_str,arr);
    strcat(device_str,"\n");
    strcat(device_str,"binCount=");
    itoa(device[i].bin_count,arr,10);
    strcat(device_str,arr);
    strcat(device_str,"\n");
    strcat(device_str,"binStart=");
    strcat(device_str,device[i].bin_start);
    strcat(device_str,"\n");
    strcat(device_str,"binLength=");
    strcat(device_str,device[i].bin_length);
    strcat(device_str,"\n");
    strcat(device_str,"ipAddress=");
    strcat(device_str,device[i].ip_address);
    strcat(device_str,"\n");
    strcat(device_str,"port=");
    strcat(device_str,device[i].port);
    strcat(device_str,"\n");
    strcat(device_str,"protocol=");
    strcat(device_str,device[i].protocol);
    strcat(device_str,"\n");
    Serial.println(device_str); 
  } 
  if (writeFile(SPIFFS,"/device.ini", device_str))
    return true;  
  else
    return false;
}
int downloadConfiguration()
{
  //download configuration of datalogger here
  if(  downLoggerConfig()&& downFlagConfig()&& downDeviceConfig() && downNetworkConfig())
  {
    Serial.println(F("all Config files Download Success"));
    
    saveConfigProfile();
    displayConfigProfile();
    //reset config flag on server
    Serial.println("sending config done...");
    char command[50]="";
    char response[100]="";
    strcpy(command,"serialNo=");
    strcat(command,mfc.serial_no);
    strcat(command,"&command=CNFDONE"); 
    if (WiFi.status() == WL_CONNECTED)
        sendGETDataWiFi(config_path , command, response);
    if(strstr(response,"response=TRUE"))
    {
      Serial.println("restarting ESP");
      ESP.restart(); 
    }
      
    
  }
  return 0;
}
void saveConfigProfile()
{
  Serial.println("saveConfigProfile function");
  saveDeviceConfig();
  saveFlagConfig();
  saveNetworkConfig();
  saveLoggerConfig();
}

int checkForserverConfig()
{  
  Serial.println("checkForserverConfig function");
  char command[]=""; 
  char response[200]="";
  strcpy(command,"serialNo=");
  strcat(command,mfc.serial_no);
  strcat(command,"&command=CNFCHK");  
  Serial.print("command=");
  Serial.println(command);
  for(int i=0;i<2;i++)
  {
    if (WiFi.status() == WL_CONNECTED)
      sendGETDataWiFi(config_path , command, response);
    if(strstr(response,mfc.serial_no)!=NULL)
    {
      if(strstr(response,"TRUE")!=NULL)
      {
        Serial.println(F("Downloading configuration"));
        downloadConfiguration();
      }
      else if(strstr(response,"EXTENDED")!=NULL)
      {
        Serial.println("extended configuration");
         extendedServerCommunication();
      } 
      return 0;
    } 
  }
  return 0;
}
int extendedServerCommunication()
{
  //this function will execute the extended functions of the datalogger
  char data[100]=""; 
  char response[250]="";
  strcpy(data,"serialNo=");
  strcat(data,mfc.serial_no);
  strcat(data,"&command=ECNFCHK");
  if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  
  Serial.println(F("inside command check"));
  if(strstr(response,"response=RESETEEPROM")!=NULL)
  {
   Serial.println(F("Inside Clear EEPROM"));
   //clearConfig(4000);   
   strcpy(data,"serialNo=");
   strcat(data,mfc.serial_no);
   strcat(data, "&command=RESETEEPROM&configuration=OK");
   if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  }
  else if(strstr(response,"response=REBOOTDEVICE")!=NULL)
  {
   Serial.println(F("Inside Reboot Device"));
   strcpy(data,"serialNo=");
   strcat(data,mfc.serial_no);
   strcat(data, "&command=REBOOTDEVICE&configuration=OK");
   if (WiFi.status() == WL_CONNECTED)
    sendGETDataWiFi(config_path , data, response);
  }
  else if(strstr(response,"response=UPLOADTRACKER")!=NULL)
  {
    //uploadTrackerToServer();
  }
  else if(strstr(response,"response=DOWNLOADTRACKER")!=NULL)
  {
    //downloadTrackerFromServer();
  }
  else if(strstr(response,"response=UPLOADMODBUS")!=NULL)
  {
    uploadModbusConfig();
  }
  else if(strstr(response,"response=DOWNLOADMODBUS")!=NULL)
  {
    downModbusConfig();
  }
  else if(strstr(response,"response=CNFCHK")!=NULL)
  {
    checkForserverConfig();
  }
  
  return 0;
}

void readLoggerProfile()
{ 
    const size_t bufferLen = 20;
    char buffer[bufferLen];
    const char *filename = "/logger.ini";
    SPIFFSIniFile ini(filename);
    Serial.println("\t\t Logger Profile\n");
    if (!ini.open()) {
      Serial.print("Ini file ");
      Serial.print(filename);
      Serial.println(" does not exist");
    }
    // Check the file is valid. This can be used to warn if any lines
    // are longer than the buffer.
    if (!ini.validate(buffer, bufferLen)) {
      Serial.print("ini file ");
      Serial.print(ini.getFilename());
      Serial.print(" not valid: ");
      printErrorMessage(ini.getError());
    }
    
     // Fetch a value from a key which is present
     if (ini.getValue("loggerProfile", "isConfigured", buffer, bufferLen)) {
      Serial.print("isConfigured = ");
      Serial.println(buffer); 
      logger.is_configured = atoi(buffer);
    }
    else {
      Serial.print("isConfigured error : ");
      printErrorMessage(ini.getError());
    }
    if (ini.getValue("loggerProfile", "plantId", buffer, bufferLen)) {
      Serial.print("plantId = ");
      Serial.println(buffer); 
      logger.plant_id=atoi(buffer);
    }
    else {
      Serial.print("plantId error : ");
      printErrorMessage(ini.getError());
    }
     if (ini.getValue("loggerProfile", "loggerId", buffer, bufferLen)) {
      Serial.print("loggerId = ");
      Serial.println(buffer);
      logger.logger_id=atoi(buffer);
    }
    else {
      Serial.print("loggerId error : ");
      printErrorMessage(ini.getError());
    }
    if (ini.getValue("loggerProfile", "readInterval", buffer, bufferLen)) {
      Serial.print("readInterval = ");
      Serial.println(buffer);
      logger.read_interval=atoi(buffer);
    }
    else {
      Serial.print("readInterval error : ");
      printErrorMessage(ini.getError());
    }
    if (ini.getValue("loggerProfile", "deviceCount", buffer, bufferLen)) {
      Serial.print("deviceCount =");
      Serial.println(buffer);
      logger.device_count=atoi(buffer);
    }
    else {
      Serial.print("deviceCount error : ");
      printErrorMessage(ini.getError());
    }
 }

  void readManufacProfile()
{ 
      const size_t bufferLen = 60;
      char buffer[bufferLen];
      const char *filename = "/manufacture.ini";
      Serial.println("\t\t Manufacture Profile ");
      SPIFFSIniFile ini(filename);
      if (!ini.open()) {
        Serial.print("Ini file ");
        Serial.print(filename);
        Serial.println(" does not exist");
      }
    
      // Check the file is valid. This can be used to warn if any lines
      // are longer than the buffer.
      if (!ini.validate(buffer, bufferLen)) {
        Serial.print("ini file ");
        Serial.print(ini.getFilename());
        Serial.print(" not valid: ");
        printErrorMessage(ini.getError());
      }
      
       // Fetch a value from a key which is present
      if (ini.getValue("manufactureProfile", "serialNo", buffer, bufferLen)) {
        Serial.print("serial no. = ");
        Serial.println(buffer);
        strcpy(mfc.serial_no,buffer);
      }
      else {
        Serial.print("serial no. error : ");
        printErrorMessage(ini.getError());
      }
       if (ini.getValue("manufactureProfile", "firmwareVersion", buffer, bufferLen)) {
        Serial.print("firmware version = ");
        Serial.println(buffer);
        strcpy(mfc.firmware_version,buffer);
      }
      else {
        Serial.print("firmware version error : ");
        printErrorMessage(ini.getError());
      }
 
 }


 void readModbusProfile()
  { 
    const size_t bufferLen = 80;
    char buffer[bufferLen];
    const char *filename = "/modbus.ini";
    Serial.println("\t\t Modbus Profile ");
    SPIFFSIniFile ini(filename);
    if (!ini.open()) {
      Serial.print("Ini file ");
      Serial.print(filename);
      Serial.println(" does not exist");
    }
    // Check the file is valid. This can be used to warn if any lines
    // are longer than the buffer.
    if (!ini.validate(buffer, bufferLen)) {
      Serial.print("ini file ");
      Serial.print(ini.getFilename());
      Serial.print(" not valid: ");
      printErrorMessage(ini.getError());
    }
    
     // Fetch a value from a key which is present
    if (ini.getValue("modbusProfile", "baudrate", buffer, bufferLen)) {
      Serial.print("baudrate = ");
      Serial.println(buffer);
    }
    else {
      Serial.print("baudrate error : ");
      printErrorMessage(ini.getError());
    }
  
     if (ini.getValue("modbusProfile", "baudrate", buffer, bufferLen)) {
      Serial.print("baudrate = ");
      Serial.println(buffer);
    }
    else {
      Serial.print("baudrate error : ");
      printErrorMessage(ini.getError());
    }
     if (ini.getValue("modbusProfile", "pollrate(ms)", buffer, bufferLen)) {
      Serial.print("pollrate(ms) = ");
      Serial.println(buffer);
    }
    else {
      Serial.print("pollrate(ms) error : ");
      printErrorMessage(ini.getError());
    }
  
    if (ini.getValue("modbusProfile", "timeout(ms)", buffer, bufferLen)) {
      Serial.print("timeout(ms) = ");
      Serial.println(buffer);
    }
    else {
      Serial.print("timeout(ms) error : ");
      printErrorMessage(ini.getError());
    }
   
 }

 void readDeviceProfile(uint8_t count)
  { 
    const size_t bufferLen = 30;
    char buffer[bufferLen];
    const char *filename = "/device.ini";
    Serial.println("\t\t Device Profile ");
    SPIFFSIniFile ini(filename);
    if (!ini.open()) {
      Serial.print("Ini file ");
      Serial.print(filename);
      Serial.println(" does not exist");
    }
  
    // Check the file is valid. This can be used to warn if any lines
    // are longer than the buffer.
    if (!ini.validate(buffer, bufferLen)) {
      Serial.print("ini file ");
      Serial.print(ini.getFilename());
      Serial.print(" not valid: ");
      printErrorMessage(ini.getError());
    }
    for(int i=0;i<count;i++)
     {
        char device_no[2];
        char device_str[8]="";
        Serial.println("\n");
        itoa((i+1),device_no,10);
        strcat(device_str,"device");
        strcat(device_str,device_no);
        Serial.println(device_str);
        // Fetch a value from a key which is present
        if (ini.getValue(device_str, "deviceIndex", buffer, bufferLen)) {
          Serial.print("deviceIndex = ");
          Serial.println(buffer);
          device[i].device_index=atoi(buffer);
        }
        else {
          Serial.print("deviceIndex error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "deviceType", buffer, bufferLen)) {
          Serial.print("deviceType = ");
          Serial.println(buffer);
          device[i].device_type = atoi(buffer);
        }
        else {
          Serial.print("deviceType error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "funCode", buffer, bufferLen)) {
          Serial.print("funCode = ");
          Serial.println(buffer);
          device[i].fun_code=atoi(buffer);
        }
        else {
          Serial.print("funCode error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "deviceId", buffer,bufferLen)) {
          Serial.print("deviceId = ");
          Serial.println(buffer);
          device[i].device_id = atoi(buffer);
        }
        else {
          Serial.print("deviceId error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "slaveID", buffer, bufferLen)) {
          Serial.print("slaveId = ");
          Serial.println(buffer);
          device[i].slave_id = atoi(buffer);
        }
        else {
          Serial.print("slaveId error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "binCount", buffer, bufferLen)) {
          Serial.print("binCount = ");
          Serial.println(buffer);
          device[i].bin_count = atoi(buffer);
        }
        else {
          Serial.print("binCount error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "binstart", buffer, bufferLen)) {
          Serial.print("binStart = ");
          Serial.println(buffer);
          strcpy(device[i].bin_start,buffer);
        }
        else {
          Serial.print("binStart error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "binLength", buffer, bufferLen)) {
          Serial.print("binLength = ");
          Serial.println(buffer);
          strcpy(device[i].bin_length ,buffer);
        }
        else {
          Serial.print("binLength error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "ipAddress", buffer, bufferLen)) {
          Serial.print("ipAddress = ");
          Serial.println(buffer);
          strcpy(device[i].ip_address,buffer);
        }
        else {
          Serial.print("ipAddress error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "port", buffer, bufferLen)) {
          Serial.print("port = ");
          Serial.println(buffer);
          strcpy(device[i].port,buffer);
        }
        else {
          Serial.print("port error : ");
          printErrorMessage(ini.getError());
        }
        if (ini.getValue(device_str, "protocol", buffer, bufferLen)) {
          Serial.print("protocol = ");
          Serial.println(buffer);
          strcpy(device[i].protocol,buffer);
        }
        else {
          Serial.print("protocol error : ");
          printErrorMessage(ini.getError());
        }
    }
 }

 void readNetworkProfile()
    { 
      const size_t bufferLen = 30;
      char buffer[bufferLen];
      const char *filename = "/network.ini";
      Serial.println("\t\t Network Profile ");
      SPIFFSIniFile ini(filename);
      if (!ini.open()) {
        Serial.print("Ini file ");
        Serial.print(filename);
        Serial.println(" does not exist");
      }
      // Check the file is valid. This can be used to warn if any lines
      // are longer than the buffer.
      if (!ini.validate(buffer, bufferLen)) {
        Serial.print("ini file ");
        Serial.print(ini.getFilename());
        Serial.print(" not valid: ");
        printErrorMessage(ini.getError());
      }
       // Fetch a value from a key which is present
      if (ini.getValue("networkProfile", "dhcpEnable", buffer, bufferLen)) {
        Serial.print("dhcpEnable = ");
        Serial.println(buffer);
        int val=atoi(buffer);
        network.dhcp_enable=val;
      }
      else {
        Serial.print("dhcpEnable error : ");
        printErrorMessage(ini.getError());
      }
    
       if (ini.getValue("wifi", "ssid", buffer, bufferLen)) {
        Serial.print("ssid = ");
        Serial.println(buffer);
        strcpy(network.ssid,buffer);
        //Serial.println(network.ssid);
      }
      else {
        Serial.print("ssid error : ");
        printErrorMessage(ini.getError());
      }
       if (ini.getValue("wifi", "password", buffer, bufferLen)) {
        Serial.print("password = ");
        Serial.println(buffer);
        strcpy(network.password,buffer);
      }
      else {
        Serial.print("password error : ");
        printErrorMessage(ini.getError());
      }
    
      if (ini.getValue("wifi","macWiFi", buffer, bufferLen)) {
        Serial.print("macWiFi = ");
        Serial.println(buffer);
        strcpy(network.mac_wifi,buffer);
      }
      else {
        Serial.print("macWiFi error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("wifi", "ipAddress", buffer, bufferLen)) {
        Serial.print("ipAddress = ");
        Serial.println(buffer);
        strcpy(network.ip_wifi,buffer);
      }
      else {
        Serial.print("ipAddress error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("wifi", "gateway", buffer, bufferLen)) {
        Serial.print("gateway = ");
        Serial.println(buffer);
        strcpy(network.gateway_wifi,buffer);
      }
      else {
        Serial.print("gateway error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("wifi", "subnet", buffer, bufferLen)) {
        Serial.print("subnet = ");
        Serial.println(buffer);
        strcpy(network.subnet_wifi,buffer);
      }
      else {
        Serial.print("subnet error : ");
        printErrorMessage(ini.getError());
      }
       if (ini.getValue("wifi","primaryDNS", buffer, bufferLen)) {
        Serial.print("primaryDNS = ");
        Serial.println(buffer);
        strcpy(network.priDNS_wifi,buffer);
      }
      else {
        Serial.print("primaryDNS error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("wifi", "secondaryDNS", buffer, bufferLen)) {
        Serial.print("secondaryDNS = ");
        Serial.println(buffer);
        strcpy(network.secDNS_wifi,buffer);
      }
      else {
        Serial.print("secondaryDNS error : ");
        printErrorMessage(ini.getError());
      }
    
      if (ini.getValue("ethernet", "macEther", buffer, bufferLen)) {
        Serial.print("macEther = ");
        Serial.println(buffer);
        strcpy(network.mac_ether,buffer);
      }
      else {
        Serial.print("macEther error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("ethernet", "ipAddress", buffer, bufferLen)) {
        Serial.print("ipAddress =");
        Serial.println(buffer);
        strcpy(network.ip_ether,buffer);
      }
      else {
        Serial.print("ipAddress error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("ethernet", "gateway", buffer, bufferLen)) {
        Serial.print("gateway = ");
        Serial.println(buffer);
        strcpy(network.gateway_ether,buffer);
      }
      else {
        Serial.print("gateway error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("ethernet", "subnet", buffer, bufferLen)) {
        Serial.print("subnet = ");
        Serial.println(buffer);
        strcpy(network.subnet_ether,buffer);
      }
      else {
        Serial.print("subnet error : ");
        printErrorMessage(ini.getError());
      }
       if (ini.getValue("ethernet", "primaryDNS", buffer, bufferLen)) {
        Serial.print("primaryDNS = ");
        Serial.println(buffer);
        strcpy(network.priDNS_ether,buffer);
      }
      else {
        Serial.print("primaryDNS error : ");
        printErrorMessage(ini.getError());
      }
      if (ini.getValue("ethernet", "secondaryDNS", buffer, bufferLen)) {
        Serial.print("secondaryDNS = ");
        Serial.println(buffer);
        strcpy(network.secDNS_ether,buffer);
      }
      else {
        Serial.print("secondaryDNS error : ");
        printErrorMessage(ini.getError());
      }
 }
void readFlagProfile()
{ 
    const size_t bufferLen = 80;
    char buffer[bufferLen];
    const char *filename = "/flag.ini";
    Serial.println("\t\t Flag Profile ");
    SPIFFSIniFile ini(filename);
    if (!ini.open()) {
      Serial.print("Ini file ");
      Serial.print(filename);
      Serial.println(" does not exist");
    }
  
    // Check the file is valid. This can be used to warn if any lines
    // are longer than the buffer.
    if (!ini.validate(buffer, bufferLen)) {
      Serial.print("ini file ");
      Serial.print(ini.getFilename());
      Serial.print(" not valid: ");
      printErrorMessage(ini.getError());
    }
    
     // Fetch a value from a key which is present
    if (ini.getValue("flagProfile", "isActive", buffer, bufferLen)) {
      Serial.print("isActive = ");
      Serial.println(buffer);
      flag.is_active = atoi(buffer);
    }
    else {
      Serial.print("isActive error : ");
      printErrorMessage(ini.getError());
    }
    if (ini.getValue("flagProfile", "isSdCard", buffer, bufferLen)) {
      Serial.print("isSdCard =");
      Serial.println(buffer);
      flag.is_sdcard = atoi(buffer);
    }
    else {
      Serial.print("isSdCard error : ");
      printErrorMessage(ini.getError());
    }
    if (ini.getValue("flagProfile", "isRMS", buffer, bufferLen)) {
      Serial.print("isRMS = ");
      Serial.println(buffer);
      flag.is_rms = atoi(buffer);
    }
    else {
      Serial.print("isRMS error  ");
      printErrorMessage(ini.getError());
    }
     if (ini.getValue("flagProfile", "isDgSync", buffer, bufferLen)) {
      Serial.print("isDgSync = ");
      Serial.println(buffer);
      flag.is_dgsync = atoi(buffer);
    }
    else {
      Serial.print("isDgSync error : ");
      printErrorMessage(ini.getError());
    }
     if (ini.getValue("flagProfile", "isZeroExport", buffer, bufferLen)) {
      Serial.print("isZeroExport =");
      Serial.println(buffer);
      flag.is_zeroexport = atoi(buffer);
    }
    else {
      Serial.print("isZeroExport error : ");
      printErrorMessage(ini.getError());
    }
    if (ini.getValue("flagProfile", "isWiFi", buffer, bufferLen)) {
      Serial.print("isWiFi =");
      Serial.println(buffer);
      flag.is_wifi = atoi(buffer);
    }
    else {
      Serial.print("isWiFi error : ");
      printErrorMessage(ini.getError());
    }
     if (ini.getValue("flagProfile", "isEthernet", buffer, bufferLen)) {
      Serial.print("isEthernet =");
      Serial.println(buffer);
      flag.is_ethernet = atoi(buffer);
    }
    else {
      Serial.print("isEthernet error : ");
      printErrorMessage(ini.getError());
    }
     if (ini.getValue("flagProfile", "isGSM", buffer, bufferLen)) {
      Serial.print("isGSM =");
      Serial.println(buffer);
      flag.is_gsm = atoi(buffer);
    }
    else {
      Serial.print("isGSM error : ");
      printErrorMessage(ini.getError());
    }
 }
void displayConfigProfile()
{
  Serial.println("displayConfigProfile function");
  readLoggerProfile();
  readFlagProfile();
  readManufacProfile();
  readModbusProfile();
  readDeviceProfile(logger.device_count);
  readNetworkProfile();
}


bool syncTimeFromServer()
{
  Serial.println("syncTimeFromServer function");
  //write code to get time and set the time
  bool flag = false;
  uint8_t Hour=0;
  uint8_t Minute=0;
  uint8_t Seconds=0;
  uint8_t Day=9;
  uint8_t Month=1;
  uint16_t Year=2022;  

  char data[50]="";
  char response[250]="";  
  //strcat(command,serialNo);
  //strcat(command,"&command=DOWNLOADTRACKER");
  //String response="";
  //check for CNFCHK
  for(int i=0;i<2;i++)
  {
    if (WiFi.status() == WL_CONNECTED)
      sendGETDataWiFi(timestamp_path , data, response);
    //Serial.print("Response:");
    //Serial.println(response);
    if (strstr(response,"-"))
    {
      char *timeStr;
      timeStr = strtok(response,"\n");
      //Serial.println(timeStr);
  //    timeStr = strtok(NULL,"\n");
  //    Serial.println(timeStr);
  //    timeStr = strtok(NULL,"\n");
  //    Serial.print("Time String:");
  //    Serial.println(timeStr);
      char *resp;
      resp = strtok(timeStr,"-");
      //Serial.println(resp);
      Year = atoi(resp);
      
      resp = strtok(NULL,"-");
      //Serial.println(resp);
      Month = atoi(resp);
      
      resp = strtok(NULL," ");
      //Serial.println(resp);
      Day = atoi(resp);      
      
      resp = strtok(NULL,":");
      //Serial.println(resp);
      Hour = atoi(resp); 
      
      resp = strtok(NULL,":");
      //Serial.println(resp);
      Minute = atoi(resp);      
      
      resp = strtok(NULL,"\n");  
      //Serial.println(resp);             
      Seconds = atoi(resp);      
      
      resp = strtok (NULL,"\n"); 
      //Serial.print("Last Pointer");
      //Serial.println(resp); 
      if(resp==NULL)
      {
        flag = true;
        break;
      }
     }
    }  
    
    if(flag==true && Year>=2022)// If year is greater than or equal to 2021 then the time will be assumed corect
    {
      Serial.println("Server Time set success");
      setTime(Hour,Minute,Seconds,Day,Month,Year);
      return 1;
    }
    else
      Serial.println("time sync from server failed");  
    return 0;  
}
void loadRestartData(char *Data)
{
  Serial.println("Entered in loadRestartData function");
  Serial.println("preparing restart data format");
  //char Data[100]="";
  char response[250]="";
  strcpy(Data,"LD=");
  append(Data,logger.logger_id,",");
  append(Data,logger.plant_id,",");
  strcat(Data,"0,");
  strcat(Data,"0,");
  strcat(Data,"2,");
  long timestamp=now()-19800;
  append(Data,timestamp,"");
  strcat(Data,"&ID=");
  //append serial number
  strcat(Data,mfc.serial_no); 
  strcat(Data,",");
  if (flag.is_wifi==1)
  {
    append(Data,(int)WiFi.RSSI(),","); 
    //Serial.println(Data);
    if (network.dhcp_enable==1)
      append(Data,default_ssid,",");
    else 
      append(Data,network.ssid,",");
    char arr[20]="";
    ip2str(WiFi.localIP(),arr);
    append(Data,arr,",");
    append(Data,flag.is_wifi,"-");
    append(Data,flag_present_wifi,",");
    append(Data,flag.is_ethernet,"-");
    append(Data,flag_present_ethernet,",");
    append(Data,flag.is_gsm,"-");
    append(Data,flag_present_gsm,",");
    append(Data,flag.is_sdcard,"-");
    append(Data,flag_present_sd,"");
  }
  /*
  else if(flag.is_gsm==1)
  {
    char imeiNumber[30];
    getImeiNumber(imeiNumber);
    strcat(Data,imeiNumber);
    strcat(Data,",");
    //append operator name
    char operatorName[30];
    getOperator(operatorName);
    strcat(Data,operatorName);
    strcat(Data,",");
    //append sim number
    char simNumber[25];
    getSimNumber(simNumber);
    strcat(Data,simNumber);
    strcat(Data,",");
    append(Data,flag.is_wifi,"-");
    append(Data,flag_present_wifi,",");
    append(Data,flag.is_ethernet,"-");
    append(Data,flag_present_ethernet,",");
    append(Data,flag.is_gsm,"-");
    append(Data,flag_present_gsm,",");
    
  }
  */
}
bool uploadRestartData(char *Data)
{
  int fail_count =0;
  char response[250]="";
  for(int i=0;i<5;i++)
  {
    if (flag.is_wifi==1)
      sendGETDataWiFi(data_path, Data, response);
    if (strstr(response,"OK")!=NULL)
     {
      Serial.println("restart upload successful"); 
      return true;
     }
    else
    {
      Serial.println("restart upload failed");
      fail_count++;
      if (fail_count==4)
      {
        return false;
      }
    }
  }  
}

bool saveToFile(char* dataWrite,uint16_t len)
{
  Serial.print(F("Data to Save:"));
  Serial.println(dataWrite);
  Serial.print("Length:");
  Serial.println(len);
  String Year =String(year());
  String Month=String(month());
  String Day  =String(day());  
  String Hour =String(hour());
  String directoryName="/records/"+Year;
 
  Serial.println(directoryName);
  if(SD.open(directoryName)!=true)
  {
   if(SD.mkdir(directoryName.c_str())==true)
   Serial.println(F("Directory created successfuly"));
   else
   Serial.println(F("Directory creation failed"));
  }
  else
  {
    Serial.println(F("Directory exist"));
    }
   directoryName="/records/"+Year+"/"+Month;
   Serial.println(directoryName);
   if(SD.open(directoryName)!=true)
  {
   if(SD.mkdir(directoryName.c_str())==true)
   Serial.println(F("Directory created successfuly"));
   else
   Serial.println(F("Directory creation failed"));
  }
  else
  {
    Serial.println(F("Directory exist"));
    }
   directoryName="/records/"+Year+"/"+Month+"/"+Day;
   Serial.println(directoryName);
   if(SD.open(directoryName)!=true)
  {
   if(SD.mkdir(directoryName.c_str())==true)
   Serial.println(F("Directory created successfuly"));
   else
   Serial.println(F("Directory creation failed"));
  }
  else
  {
    Serial.println(F("Directory exist"));
    }
  String fileName=directoryName+"/"+Hour+".txt";
  Serial.println(fileName);
  myFile=SD.open(fileName,FILE_WRITE);
  //myFile.seek(EOF);  
  myFile.println(dataWrite);
  myFile.close();
  Serial.println(F("Saved to File"));
}
int readAndSave()
{
  Serial.println("\nreadAndSave function");
  //This function will read all the device connected to datalogger
  //and save it to SD card with timestamp  
  for (uint8_t i=0;i<logger.device_count;i++)
  {
    Serial.print("reading device index =");
    Serial.println(i);
    char deviceData[600]=""; 
    char response[250]="";
    int errorFlag=readDeviceData(device[i],deviceData);

    char Data[650]="LD=";
    long timestamp=now()-19800; 
      
    append(Data,logger.logger_id,",");
    append(Data,logger.plant_id,",");
    append(Data,device[i].device_type,",");
    append(Data,device[i].device_id,",") ;
    append(Data,errorFlag,",");
    append(Data,timestamp,"");
    strcat(Data,"&ID=");
    strcat(Data,deviceData);  
    Serial.print("Data=");
    Serial.println(Data);
    if(flag.is_sdcard==HIGH)
    {
      Serial.println("Saving into SD card");
      saveToFile(Data,sizeof(Data));
    }
    else
    {
      if (WiFi.status() == WL_CONNECTED)
        sendGETDataWiFi(data_path , Data, response); 
      if(strstr(response,"ConnectedOK")!=NULL)
          Serial.println("data upload successful");
      else
       Serial.println("data upload failed"); 
    }    
  }
  return 0;
}
int initRS485(uint16_t baudRate,uint8_t parity)
{    
  pinMode(modbusEn, OUTPUT);  
  switch(parity)
  {
    case 0:modbusPort.begin(baudRate,SERIAL_8N1,modbus_Rx,modbus_Tx);
    break;
    case 1:modbusPort.begin(baudRate, SERIAL_8O1,modbus_Rx,modbus_Tx);
    break;
    case 2:modbusPort.begin(baudRate, SERIAL_8E1,modbus_Rx,modbus_Tx);
    break; 
    default:modbusPort.begin(9600);
    break; 
  }
  return 0;  
} 
int initSlave(uint8_t sid)
{
  master.begin(sid, modbusPort, modbusEn);
  return 0;
}
bool readDeviceData(clientDeviceDetails obj,char* data)
{  
  bool isError = true;
  initSlave(obj.slave_id);
  
  uint16_t bin_start[obj.bin_count];
  uint8_t bin_length[obj.bin_count];
  char *pt1;
  pt1=strtok(obj.bin_start,",");
  int val_start=atoi(pt1);
  bin_start[0]=val_start;
  for (int k=1;k<obj.bin_count;k++)
  {
    pt1=strtok(NULL,",");
    int val_start=atoi(pt1);
    bin_start[k]=val_start;
    
  }
  char *pt2;
  pt2=strtok(obj.bin_length,",");
  int val_length=atoi(pt2);
  bin_length[0]=val_length;
  for (int k=1;k<obj.bin_count;k++)
  {
    pt2=strtok(NULL,",");
    int val_length=atoi(pt2);
    bin_length[k]=val_length;
  }
  for (int i=0; i<obj.bin_count; i++)
  {
    uint16_t response[bin_length[i]];
    int result=readModbusPacket(obj.slave_id, obj.fun_code, bin_start[i], bin_length[i],response);
    if(result==0)
    {
      isError=false;
      for (int j=0; j<bin_length[i] ;j++)
      {
        char number[6];
        ltoa(response[j],number,10);
        strcat(data,number);
        if((i==(obj.bin_count-1))&& (j==(bin_length[i]-1)))
        strcat(data,"");//append empty at last of the data not comma
        else
        strcat(data,",");
      }      
    }
    else
    {
      isError=true;
      uint8_t packet=i+1;
      memset(data,0,600);
      append(data,packet,"-");
      append(data,result,"");
      return isError;
    } 
    delay(modbus.poll_rate);     
 }
 return isError;
}

int readModbusPacket(uint8_t sid, uint8_t fc, uint16_t start_add, uint16_t len,uint16_t *response)
{
  //This function will read modbus packet specified by function code, slave id, start address and length
  //and update the response array, also returns response code
  //modbus.begin(sid, modbusSerial, enablePin);
  Serial.print("Function ");
  Serial.println(fc);
  Serial.print("Start Add:");
  Serial.println(start_add);
  Serial.print("Length");
  Serial.println(len);
  bool success = master.getRegisters(fc, start_add, len);
  Serial.print("Success:");
  Serial.println(success);
  Serial.println(F("Response Buffer"));
  for(int i=0;i<len;i++)
  Serial.println(master.responseBuffer[i]);
  if(success==1)
  {    
    for (uint8_t i=0 ;i<len;i++)
    {
      response[i]=master.uint16FromFrame(bigEndian,i*2+3);
      Serial.println(response[i]);
      }
      return 0;    
  }
  else if((master.responseBuffer[1] & 0b10000000) ==  0b10000000)
  {    
    return master.responseBuffer[2];
    }
    else
    {      
      return 11;
    }
}
void getTimeRTC()
{
  Serial.println("\ngetting RTC time");
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  
  DateTime now = rtc.now();
  //Serial.println(now.year());
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  setTime(now.hour(),now.minute(),now.second(),now.day(),now.month(),now.year());
}
void setTimeRTC()
{
  Serial.println("\nsetting RTC time");
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
    if (! rtc.isrunning()) {
      Serial.println("RTC is NOT running, let's set the time!");
      // When time needs to be set on a new device, or after a power loss, the
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(DateTime(year(),month(),day(),hour(),minute(),second()));
      Serial.println("RTC time update success");
    }
}

bool initSdCard()
{
  Serial.println("\nEntered in initSDCard function ");
  Serial.println("\ninitializing sd card");
  if(!SD.begin(sdCardSS))
  {
    delay(3000);
    Serial.println("initializing again");
    if(!SD.begin(sdCardSS))
      return false;
  }
  else
    Serial.println("sd initialization successful");
  //check if records file exists
  if(SD.exists("/Records")!=true)
  {
    if(SD.mkdir("/Records")==true)
    Serial.print(F("records directory create success"));
    else
    {
      Serial.print(F("records directory create failed"));
      return false;
    }
  }
  else
    Serial.println("Records directory exists");
  return true;
}

String readFromFile(uint16_t y,uint8_t m,uint8_t d,uint8_t h,uint16_t index)
{
  String filePath="records/"+String(y)+"/"+String(m)+"/"+String(d)+"/"+String(h)+".txt";
  Serial.print(F("filename:"));
  Serial.println(filePath);
  if(SD.exists(filePath))
  {
    return readLines(index,filePath);
  }
  else
  {
    return "NO_FILE";  
  }
}

String readLines(uint16_t count,String fileName)
{
  String data="";
  uint16_t line=0;
  //fileName="records/2022/4/26.txt";
  myFile=SD.open(fileName,FILE_READ);
    
  while(myFile.available())
  {
    char c=myFile.read();
    //Serial.println(c);
    if(c=='\n')
    {
      //Serial.println("New Line");
      line=line+1;
      if(line==count)
      {
        myFile.close();
        return data;
      }
      else
      {
        data="";
      }
    }
    else
    {
      data+=c;
    }
  }
  myFile.close();
  return "NO_RECORD";//There is no data at this line
}

int countLines(String fileName)
{
  uint16_t count=0;        
  myFile=SD.open(fileName,FILE_READ);
  while(myFile.available())
  {
    char c=myFile.read();
    if(c=='\n')
    {
      count=count+1;      
    }    
  }
  myFile.close();
  return count;
}
//function to save tracker data into SD card
bool saveUploadTracker()
{
//  Serial.print(F("Data to Save:"));
//  Serial.println(data);
//  Serial.print("Length:");
//  Serial.println(len);
  char trackerData[20]="";
  append(trackerData,lastUpload.y,",");
  append(trackerData,lastUpload.m,",");
  append(trackerData,lastUpload.d,",");
  append(trackerData,lastUpload.h,",");
  append(trackerData,lastUpload.h,"\n");  
  Serial.print("Tracker:");
  Serial.println(trackerData);
  String directoryName="tracker";
  String fileName=directoryName+"/"+"lastUpload.txt";
  Serial.println(directoryName);
  Serial.println(fileName);
  if(SD.exists(directoryName)!=true)
  {
   if(SD.mkdir(directoryName)==true)
   Serial.println(F("Directory created successfuly"));
   else
   Serial.println(F("Directory creation failed"));
  }
  else
  {
    Serial.println(F("Directory exist"));
    }
  myFile=SD.open(fileName,FILE_WRITE);
  myFile.seek(0);  
  myFile.println(trackerData);
  myFile.close();
  Serial.println(F("Saved to File"));
}
bool syncUploadTracker()
{
  char Data[50]="serialNo=";
  char response[250];  
  strcat(Data,mfc.serial_no);
  strcat(Data,"&command=DOWNLOADTRACKER");
  //String response="";
  //check for CNFCHK
  for(int i=0;i<2;i++)
  {
    if (WiFi.status() == WL_CONNECTED)
      sendGETDataWiFi(config_path , Data, response);
   
    if(strstr(response,mfc.serial_no)!=NULL)
    {
      if(strstr(response,"&response=")!=NULL)
      {
        char *resp;
        resp = strtok(response,"=");
        resp = strtok(NULL,"=");
        resp = strtok(NULL,"=");
  
        char *ret;
        ret = strtok(resp,"\n");
        
        char *pt;   
        pt = strtok (ret,",");      
        lastUploadYear=atoi(pt); 
  
        pt = strtok (NULL,",");  
        lastUploadMonth=atoi(pt); 
  
        pt = strtok (NULL,",");  
        lastUploadDay=atoi(pt); 
  
        pt = strtok (NULL,",");  
        lastUploadHour=atoi(pt);

        pt = strtok (NULL,",");  
        lastUploadCount=atoi(pt);
  
        pt = strtok (NULL,",");  
        if(pt==NULL)
          return true;
     }
    }  
  }
  return false;
}
bool setUploadTracker()
{
  bool flag=true;
  uint16_t currentYear=year();
  uint8_t currentMonth=month();
  uint8_t currentDay=day();
  uint8_t currentHour=hour();
  
  Serial.print("Year:");
  Serial.println(currentYear);
  Serial.print("Month:");
  Serial.println(currentMonth);
  Serial.print("Day:");
  Serial.println(currentDay);
  Serial.print("Hour:");
  Serial.println(currentHour);
  
  if(lastUploadYear>currentYear)
  {
    return false;
  }
  else if(lastUploadYear<currentYear)
  {
    if(currentYear-lastUploadYear>1)
    {
      return false;
      }
    YEAR_FLAG=1;
    MONTH_FLAG=1;
    DAY_FLAG=1;
    HOUR_FLAG=1;
  }
  else
  {
    YEAR_FLAG=0;
    if(lastUploadMonth>currentMonth)
    {
      return false;  
    }
    else if(lastUploadMonth<currentMonth)
    {
      MONTH_FLAG=1;
      DAY_FLAG=1; 
      HOUR_FLAG=1; 
    } 
    else
    {
      MONTH_FLAG=0;
      if(lastUploadDay>currentDay)
      {
        return false;  
      }
      else if(lastUploadDay<currentDay)
      {
        DAY_FLAG  =1;  
        HOUR_FLAG =1;
      }
      else
      {
        DAY_FLAG=0;
        if(lastUploadHour>currentHour)
        {
          return false;  
        }
        else if(lastUploadHour<currentHour)
        {
          HOUR_FLAG=1;  
        }
        else
        {
          HOUR_FLAG=0;  
        }
      }  
    } 
  }

  Serial.print("Year Flag:");
  Serial.println(YEAR_FLAG);
  Serial.print("Month Flag:");
  Serial.println(MONTH_FLAG);
  Serial.print("Day Flag:");
  Serial.println(DAY_FLAG);
  
  Serial.println(F("Upload Tracker:"));
  Serial.print(lastUploadYear);
  Serial.print("/");
  Serial.print(lastUploadMonth);
  Serial.print("/");
  Serial.print(lastUploadDay); 
  Serial.print("-");
  Serial.println(lastUploadCount); 
  return flag;
}
int updateUploadTracker()
{ 
    if(HOUR_FLAG==1)
    {
      lastUploadCount=1;
      lastUploadHour+=1;
      if(lastUploadHour>23 && DAY_FLAG==1)
      {
        lastUploadHour=0;
        lastUploadDay+=1;
        if(lastUploadDay>31 && MONTH_FLAG==1)
        {
          lastUploadDay=1;
          lastUploadMonth+=1;
          if(lastUploadMonth>12 && YEAR_FLAG==1)
          {          
            lastUploadMonth=1;
            lastUploadYear+=1;
          }
        }
      } 
    } 
 
  lastUpload.count=lastUploadCount;
  lastUpload.h=lastUploadHour;
  lastUpload.d=lastUploadDay;
  lastUpload.m=lastUploadMonth;
  lastUpload.y=lastUploadYear;  
  return 0; 
}
bool initializeTracker()
{
  //sync tracker from server
    
  if(syncUploadTracker()==true)
  {
    setUploadTracker();
  }
  return 0;
}
int readAndUpload()
{
  //read last upload count from eeprom
  Serial.println(F("Reading from SD Card"));
  bool uploadStatus=false;
  unsigned long startMillis=millis();
  setUploadTracker();
  String Data=readFromFile(lastUploadYear,lastUploadMonth,lastUploadDay,lastUploadHour,lastUploadCount);
  Serial.print("Time to Read:");
  Serial.println(millis()-startMillis);
  if((Data!="NO_FILE")&&(Data!="NO_RECORD"))
  {
    char buff[650];
    char response[250];
    Data.toCharArray(buff,650);
    if (WiFi.status() == WL_CONNECTED)
      sendGETDataWiFi(config_path , buff, response);
   
    if(strstr(response,"ConnectedOk")!=NULL)
    {
      Serial.print(F("Upload Success"));
      lastUploadCount+=1;      
      //updateUploadTracker();
    }
    else
    {
      //failedCount+=1;
      Serial.println(F("Upload Failed"));
    }
  } 
  else
  {
    Serial.println(F("No records/no file"));
    updateUploadTracker();  
  } 
  //last upload year,month,day and dayCount
  //read data from the daily file after the dayCount
  //upload data read from file to server
  return 0;
}
