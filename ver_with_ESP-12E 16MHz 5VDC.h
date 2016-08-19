//ver with ESP-12E and 5VDC 16MHz Pro Mini (wrong delivery)

/* Wiring:
 Arduino Pro Mini     BM019
 DIN: Pin 8         DIN: pin  2
 SS: pin  10         SS: pin  3
 MOSI: pin  11       MOSI: pin  5
 MISO: pin  12       MISO: pin  4
 SCK: pin 13        SCK: pin  6
*/

#include <SPI.h> // the NFC sensor communicates using SPI, so include the library:
#include <LowPower.h> // lib for ProMini to manage low power mode
#include <SoftwareSerial.h> // lib for creating addiotional serial ports

SoftwareSerial esp(3,2); //RX, TX. Crete serial port for ESP-12E

#define invON  0
#define invOFF  1
#define SSPin 10  // Slave Select pin
#define Din   8 // Sends wake-up pulse
#define BM019pin 7  // NFC reader power pin
#define buzzPin 9 // buzzer command pin
#define espEnPin 4  // pin commanding power thru darlington array to ESP-12E
#define motorPin 5  // motor command pin
#define wakeUpPin 3
#define CHARGE_BATTERY_1 1
#define NO_TAG_FOUND 2
#define LOW_BG 3

//const String apiKey = "XCBNVJEKYZXJP1E7"; //	Breadboard;	channel#: 130194
//const String apiKey = "5DSKXZTHBJZD3KZ0"; //	S/N: 001;	channel#: 143024
const String apiKey = "4FY0WOWMI8SXGCCY"; //	S/N:002;	channel#: 144286

String ssid[]={"TP-LINK_482E",	"Grozdan",	"G1"};
String pass[]={"86482042",		"a1b2c3d4",	"a1b2c3d4"};

byte RXBuffer[40];     // receive buffer
int BG[16], temperature[16]; //always divide by 180.00
int timestamp;
boolean processOK = false;
float trend=0;
int calDose=0; // calculated insulin dose
int batVDC=0;   // battery voltage; divide by 100.00
byte corrBG = 0, corrT = 0;
int sleepTime = 600; // in seconds



void setup() {
  pinMode(Din, OUTPUT);       pinMode(SSPin, OUTPUT);
  pinMode(BM019pin, OUTPUT);  pinMode(espEnPin,OUTPUT);
  pinMode(buzzPin,OUTPUT);    pinMode(motorPin,OUTPUT);
  
  Serial.begin(9600);
  
  digitalWrite(BM019pin,invOFF);
  digitalWrite(buzzPin,0);
  digitalWrite(espEnPin,invOFF);
  digitalWrite(motorPin,0);
}

void sound (byte cc){ 
byte  ss=0;  // times beep
int   dd=0;  // sound ON time
int   aa=0;  // sound OFF time
  
  // draws attention that msg will follow
  tone(buzzPin,3600);
  delay(600);
  noTone(buzzPin);
  delay(1500);
  
	switch (cc) {
		case 0 : digitalWrite(buzzPin,0); break;

		case CHARGE_BATTERY_1	: ss=1;	dd=200;	aa=0; break;

		case NO_TAG_FOUND		: ss=2;	dd=200;	aa=400; break;

		case LOW_BG				: ss=7;	dd=200;	aa=0; break;
	}
  
	// message itself
	for (byte ii=1;ii<=ss;ii++){
		tone(buzzPin,3500);
		delay(dd);
		noTone(buzzPin);

		// adding alarm tone instead of pause
		if (cc == LOW_BG) {
			tone(buzzPin,3400);
			delay(dd);
			noTone(buzzPin);
		}
		
		if (ii!=ss) delay(aa);
	}
  
	if (cc!=0) delay(2500); // delay to give time if other msg follows
}

void setNFC(boolean ss){ // switch ON/OFF NFC reader
  if (ss) { //swithc ON NFC
    digitalWrite(BM019pin,invON); // apply power to NFC
    delay(10);                    // power-up time
    for (byte i=0;i<40;i++)  RXBuffer[i]=33;
  
  //for (byte dd=10;dd<=13;dd++)  digitalWrite(dd,1); 
  digitalWrite(Din,0);
  
  SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV32);
    delay(10);
    digitalWrite(SSPin, HIGH); // SS high - MISO, MOSI are INOP
    digitalWrite(Din, HIGH); // set Din state
    delay(10);                      // start of wake up
    digitalWrite(Din, LOW);      // pulse
    delayMicroseconds(100);         //
    digitalWrite(Din, HIGH);     // end of wake-up pulse
  delay(1);
    Serial.println(F("Power sent to NFC reader."));
    }
  else { // switch OFF NFC
    SPI.end();
    digitalWrite(BM019pin,invOFF); // power OFF the board
    for (byte dd=10;dd<=13;dd++)  digitalWrite(dd,0);
  digitalWrite(Din,0);  
    Serial.println(F("NFC reader is OFF."));
  }
}

void IDN_Command() {  //identifies the chip on the PCB
	Serial.print(F("Checking NFC reader: "));  //
	sendCommand(0x00,0x01,0x00,0,0,0,0);
	poll(true);
	if (processOK) readData();
 
	if ((RXBuffer[0] == 0) & (RXBuffer[1] == 15)) {
		Serial.println(F("OK."));
		/*
		Serial.print("TX:\tDevice ID: ");
		for(byte i=0;i<(RXBuffer[1]-2);i++) {
			Serial.print(RXBuffer[i+2],HEX); Serial.print(" ");
		}
		Serial.println("\nTX:\tROM CRC: "+String(RXBuffer[RXBuffer[1]],HEX)+String(RXBuffer[RXBuffer[1]+1],HEX));
		*/
	}
	else {
		Serial.println(F("=== Cannot connect to NFC reader! ==="));
		processOK = false;
	}
}

void sendCommand(byte a,byte b,byte c,byte d,byte e,byte f, byte g) {
  digitalWrite(SSPin,LOW);
  SPI.transfer(a); SPI.transfer(b); SPI.transfer(c);
  SPI.transfer(d); SPI.transfer(e); SPI.transfer(f);
  SPI.transfer(g);
  digitalWrite(SSPin,HIGH);
  delay(1);
}

void poll(boolean ss) {
byte ss1=0;
byte limit=255; 
	
	digitalWrite(SSPin, LOW);
	while ((RXBuffer[0] != 8) & (ss1<=limit))  {
		RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
		RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
		ss1++;
	}
  
	digitalWrite(SSPin, HIGH);
	delay(1);
	if ((ss1>limit) & (ss)) {
		Serial.print(F("Poll failed: ")); Serial.println(String(ss1));
		processOK=false; // timeout. sensor is not ready;
	}
}

void readData() {
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  for (byte i=0;i<RXBuffer[1];i++)
    RXBuffer[i+2]=SPI.transfer(0);  // data
  digitalWrite(SSPin, HIGH);
  delay(1);
}

void setProtocolCommand() {// tells the chip to work with ISO 15693 protocol
  
  Serial.print(F("Setting the ISO-15963 protocol: "));
  sendCommand(0x00,0x02,0x02,0x01,0x0D,0,0);
  poll(true);
  if (processOK) readData();
  
  if ((RXBuffer[0] == 0) & (RXBuffer[1] == 0)) {
    Serial.println(F("OK"));
  }
  else {
    Serial.println(F("=== FAILED ==="));
    processOK = false; // NFC not ready
  }
  delay(1);
}

boolean TagPresent() { 
byte ii=1;
boolean tagFound = false,	exitSearch = false;

	Serial.print(F("Looking for BG sensor... "));
	
	while (!exitSearch) {
		sendCommand(0x00,0x04,0x03,0x26,0x01,0x00,0);
		poll(false);
		readData();
		
		if (RXBuffer[0] == 128)  {
			Serial.print(F("Detected. UID: "));
			for(byte i=11;i>=4;i--)    {
				Serial.print(RXBuffer[i],HEX);  Serial.print(F(" "));
			}
			Serial.println();
			exitSearch = true;
			tagFound = true;
		}
		else    {
			if (ii >= 3) 
				exitSearch = true;
		}
		
		ii++;
	}
	
	if (!tagFound) Serial.println(F("=== NO SENSOR DETECTED ==="));

	return tagFound;
}

void GetTagMemInfo() {
  Serial.println(F("--- TAG MEMORY INFO ---"));

  sendCommand(0x00,0x04,0x02,0x0a,0x2b,0,0);
  poll(true);
  readData();

  if (RXBuffer[0] == 0x80)  {
    Serial.print(F("\tMemory Blocks: ")); Serial.print(String(RXBuffer[12]+1));
  Serial.print(F(" with ")); Serial.print(String(RXBuffer[13]+1));
  Serial.println(F(" bytes per block"));
  }
  else    {
    Serial.print(F("\tERROR - Result Code, HEX: ")); Serial.println(RXBuffer[0],HEX);
    processOK=0;
    }
}

void get15minData () { 
byte c=0;
int s2[16], tTemp[16], bandBG = 2*180, bandT = 2*180;
int saBG = 0, saT = 0;
byte aa=0, bb=0, cc=0, oldest=0;
  
  Serial.println(F("Collecting sensor data: "));
  for (byte i=3;i<(3+13);i++){  // read blocks from 3 to 15
    sendCommand(0x00,0x04,0x03,0x02,0x20,i,0);
    poll(true);
    readData();
    for (int n=3;n<=10;n++) { // reads 8 bytes from i-th block
      
      if ((c+24) == 26) oldest=RXBuffer[n];

      // ------------ get BG
      if ( ((c+24) > 28) and ((c+24-29)%6 == 0) and ((c+24)<(28+16*6)) ) { 
        s2[bb]= ((RXBuffer[n]<<8) | aa) & 0x1FFF; // also must AND 1FFF (see the CTapp mails and your Excell calculations)
        bb++;
      }

      // ------------ get temperature
      if ( ((c+24)>28) and (c+24)<(28+16*6) and ((c+24-32)%6 == 0) ) { 
        tTemp[cc]=((RXBuffer[n]<<8) | aa) & 0x1FFF;
        //Serial.print(String("byte ")+String(c+24)+", value "+String(tTemp[cc],HEX));
        //Serial.println(", corrected "+String(tTemp[cc]&0x1FFF));
        cc++;
      }

      aa=RXBuffer[n];
      c++;
    }
  }
  
  // ------------ sort data hystorically
  aa=oldest;
  for (byte i=0;i<16;i++) {
    BG[i]=s2[aa]; // must divide by 180 every time to convert from mg/dL to mmol/l;
    temperature[i] = tTemp[aa]; // must divide 180.0 to obtain approximate deg C
    aa++;
    if (aa>15) aa=0;
  }
  
	// filtering using band
	// get saBG and saT (mean of BG and temperature)
	corrBG = 0;
	corrT = 0;
  
	for (byte i=0;i<16;i++) {
		saBG = saBG + BG[i]/16;
		saT  = saT  + temperature[i]/16;
	}
  
  //excluding the values outside the band
  for (byte i=0;i<16;i++) {
		if ( (BG[i] < (saBG-bandBG)) + (BG[i] > (saBG+bandBG)) ) {
			BG[i] = saBG;
			corrBG++;
		}
		
		if ( (temperature[i] < (saT-bandT)) + (temperature[i] > (saT+bandT)) ) {
			temperature[i] = saT;
			corrT++;
		}
	}
  
	trend=(BG[15]-BG[14])/180.0;
  
	// arrange next scan (min 2'; max 10'; trend <= 0.06 >> 10'; trend > 0.06 >> function)
	sleepTime = -8500*trend + 1110;
	//Serial.println("SleepTime = "+String(sleepTime));
	sleepTime = constrain (sleepTime,90,600); 
	//Serial.println("SleepTime = "+String(sleepTime));
	
	if ((BG[15]/180.0+trend*10) <= 4.0) {
		Serial.println(F("\n!!! === LOW BLOOD GLUCOSE === !!!\n"));
		sound(LOW_BG);
		//sleepTime = 90;
	}
	
  // printing hystorical BG data; last is in BG[15];
  Serial.print(F("BG data: "));
  for (byte i=0;i<16;i++) {
    Serial.print(BG[i]/180.0);
    Serial.print(F(" "));
  }
  Serial.println();
  
  // printing hystorical temperature data;
  Serial.print(F("Temperature: "));
  for (byte i=0;i<16;i++) {
    Serial.print(temperature[i]/180.0);
    Serial.print(F(" "));
  }
  Serial.println();
  
  Serial.print(F("\tTrend  : ")); Serial.print(trend);  //Serial.println(F(" (mmol/l)/min"));
  Serial.print(F("\tPredicted BG: "));  Serial.print(String(BG[15]/180.0+trend*10)); //Serial.println(F(" mmol/l")); // 10' delay from actual BG
  
  // reading timestamp bytes 317&316 (244 blocks 8 byte per block)
  sendCommand(0x00,0x04,0x03,0x02,0x20,39,0);
  poll(true);
  readData();
  for (int n=3;n<=10;n++) {
    if ((n-3+312)==317) timestamp = (RXBuffer[n]<<8) | aa;
    aa=RXBuffer[n];
    //Serial.print(312+n-3);Serial.print(" ");Serial.println(aa,HEX);
  }
  Serial.print(F("\tTimestamp: ")); Serial.println(String(timestamp));
    
}

void ScanAndSend(String ccc){
  
	Serial.print(F("Build: ")); Serial.println(ccc);
	
	processOK = true;
	setNFC(1);    // power the NFC
	IDN_Command();  // IDN_Command - check if NFC reader is there
	
	if (processOK) {
		setProtocolCommand(); // set the ISO-15963
		if (processOK){
			
			if (TagPresent()) {
				get15minData();
				setNFC(0);
				calculateInsulin();
			}
			else {
				setNFC(0);
				BG[15]			= 1.11  * 180.00;
				temperature[15] = 25.00	* 180.00;
				trend = 0;
				sleepTime = 300; // check again in 5 minutes
				//sound(NO_TAG_FOUND);
			}
			
			//checkVcc("NFC"); NO NEED ALREADY. THE VCC IS CHECKED IN sendDataWifi
			sendDataWifi();
		}
	}
}

void changeAP(byte iAP) {
  Serial.print(F("\nTesting AP: ")); Serial.println(ssid[iAP]);
  esp.println("AT+CWJAP_DEF=\""+ssid[iAP]+"\",\""+pass[iAP]+"\"");
}

/*void manualSSID(byte manNum) { // REAPIR THIS ONE WITH TIMEOUT
  String manStr;
  
  manNum--;
  Serial.print("Enter SSID name: ");
  while (Serial.available()==0);
  manStr = Serial.readString();
  manStr.remove(manStr.length()-2);
  ssid[manNum] = manStr;
  Serial.println(ssid[manNum]);

  Serial.print("Enter SSID Password: ");
  while (Serial.available()==0);
  manStr = Serial.readString();
  manStr.remove(manStr.length()-2);
  pass[manNum] = manStr;
  Serial.println(pass[manNum]);

  changeAP(manNum);
} */

boolean establishTCP(){
String estStr1;
boolean estOK = false, tcpResult = false;
int estCount=0, estTimeLimit=30000;
  
  estStr1 = "AT+CIPSTART=\"TCP\",\"";
        estStr1 += "184.106.153.149"; // api.thingspeak.com
        estStr1 += "\",80";
  
  
  Serial.print(F("TCP attempt: "));
  esp.println(estStr1);
  
  while (!estOK) {
    if (esp.available() > 0) estStr1 = esp.readString();

    if (estStr1.indexOf("OK") >= 0) {
      Serial.println(F("TCP OK"));
      estOK = true;
      tcpResult = true;
    }

    if (estStr1.indexOf("CLOSED") >= 0) {
      Serial.println(F("=== TCP Failed ==="));
      estOK = true;
    }

    if (estCount >= estTimeLimit) {
      estOK = true;
      Serial.println(F("=== TCP timeout ==="));
    }

    estCount++;
    estStr1 = "";
  }
  
  //if (!tcpResult) sound(failedTCP);
  return tcpResult;
}

boolean sendMessage() {
String	msgStr1, msgStr2;
boolean msgOK = false,  dataOK = false, msgResult = false;
int		sendTO1 =0,   sendTO2 = 0,  sendTOExit=10000;
  
	msgStr1 = "GET /update?api_key="+apiKey+
        "&field1="+String(BG[15]/180.0)+
        "&field2="+String(BG[15]/180.0+trend*10)+
        "&field3="+String(calDose)+
        "&field4="+String(batVDC/100.00)+
        "&field5="+String(temperature[15]/180.00)+
        "&field6="+String(sleepTime/60.0)+
        "&field7="+String(corrBG)+
        "&field8="+String(corrT)+
        "\r\n";
  
  msgStr2 = "AT+CIPSENDEX="+String(msgStr1.length());
  
  Serial.print(F("Sending data: "));
  
  esp.println(msgStr2);
  
	while (!msgOK) {
		if (esp.available() > 0) msgStr2 = esp.readString();

		if (msgStr2.indexOf(">") >= 0) {
		  
		  esp.println(msgStr1);
		  sendTO1 = 0;
		  
		  while (!dataOK) {
			if (esp.available() > 0) msgStr2 = esp.readString();

			if (msgStr2.indexOf("SEND OK") >= 0) {
			  Serial.println(F("OK"));
			  msgOK = true; dataOK = true;
			  msgResult = true;
			}

			if (msgStr2.indexOf("ERROR") >= 0) {
			  Serial.println(F("=== Failed ==="));
			  msgOK = true; dataOK = true;
			}
			
			if (sendTO2 == sendTOExit) {
			  Serial.println(F("=== Timeout message ==="));
			  msgOK = true; dataOK = true;
			}
			
			sendTO2++;
			msgStr2 = "";
		  }
		}

		if (sendTO1 == sendTOExit) {
			Serial.println(F("=== Timeout lenght ==="));
			Serial.println(sendTO1);
			msgOK = true;
		}

		sendTO1++;
		msgStr2 = "";
	}
  
	delay(10);
	return msgResult;
}

boolean establishAP() {
boolean apOK=false, apOK2=false, apResult=false;
String apStr="";
byte apNum=0, maxAP = (sizeof(ssid)/sizeof(String));
long	apCount = 0,	apExitTime = 70000;
int 	manTimeout=0,	manTimeoutExit = 36000;
  
	while (!apOK) {

		if (esp.available() > 0) apStr = esp.readString();
		//delay(10);

		// ------ No connection part --------
		if (apStr.indexOf("WIFI DISCONNECT") >= 0) {
			apCount=0;
			changeAP(apNum);
		}

		if (apStr.indexOf("FAIL") >= 0) {
			Serial.print(F("Failed! Count: ")); Serial.println(apCount);
			apNum++;
			if (apNum < maxAP) {
				apCount=0;
				changeAP(apNum);
			}
		}
      
		if (apNum == maxAP) {
			Serial.println(F("\n=== No valid AP found! ==="));
			
			apOK = true; // remove this if activate manualSSID
			
			/*
			Serial.println(F("\nWill you enter AP manually? (\"no\" will exit)"));
			apOK2 = false; apStr=""; manTimeout=0;
		  
			while (!apOK2) {
				if (Serial.available() != 0) apStr = Serial.readStringUntil('\r');

				if (apStr == "yes") {
					apCount = 0;
					manualSSID(maxAP);
					apNum=0;
					apOK2 = true;
				}

				if (apStr == "no") {
					Serial.print(F("=== AP connection canceled ===")); Serial.println(manTimeout);
					apOK2 = true;
					apOK = true;
					apNum=0;
				}
			
				if (manTimeout >= manTimeoutExit) {
					Serial.println(F("=== Manual mode timeout ==="));
					apOK2 = true;
					apOK = true;
					apNum = 0;
				}
			
				//Serial.println(manTimeout);
				manTimeout++;
				apStr = "";
			} 
			*/
		}
		// --------- end of "no connection" part -------------------

		// ---------- connection OK part --------------------------
		if (apStr.indexOf("ready") >= 0) {
			Serial.println(F("Boot OK"));
			apCount = 0;
		}
		
		if (apStr.indexOf("WIFI CONNECTED") >= 0) {
			apCount = 0;
		}
	
		if (apStr.indexOf("WIFI GOT IP") >= 0) {
			Serial.print(F("Connected!, count: ")); Serial.println(apCount);
			apCount = 0;
			apOK = true;
			apResult = true;
		}

		if (apCount >= apExitTime) {
			Serial.print(F("=== AP timeout ===, count: ")); Serial.println(apCount);
			apOK = true;
		}

		apCount ++;
		apStr="";
	} // while (apOK)
    
	return apResult;
}

void sendDataWifi() {
	esp.begin(9600);
    Serial.println(F("\nWiFi module ON"));
	Serial.print(F("Stored AP: ")); Serial.println(sizeof(ssid)/sizeof(String));
	digitalWrite(espEnPin,invON);
	checkVcc("WiFi");
	
	if (establishAP()) {
		if (establishTCP()) {
			sendMessage();
		}
	}
  
	Serial.println(F("WiFi module OFF"));
	esp.end();
	digitalWrite(espEnPin,invOFF);
}

void calculateInsulin() {
boolean injBan = true;
  
  calDose = int(((BG[15]/180.0+trend*10)-5.0)*0.5+trend*20);
  
  Serial.print(F("Calculated insulin dose: "));  Serial.print(String(calDose));
  Serial.print(F(" injBan: "));  Serial.println(String(injBan));
  
  if ((calDose<2) | injBan) calDose=0;
  Serial.print(F("Recommended insulin dose: ")); Serial.println(String(calDose));
  
}

/*void onWakeUp() {
  detachInterrupt(digitalPinToInterrupt(wakeUpPin));
  keyPressed = true;
}*/

void sleepFor (int ss) {
//boolean exitF = false;
//int manTimeout = 0, manTimeoutExit = 5000;
//String shutStr = "";
	
	// first print, then close the port... stupid!
	Serial.print(F("\nSystem sleeps for ")); Serial.print(ss/60.0); Serial.println(F(" minutes"));
	Serial.end();
	esp.end();
	SPI.end();
	ss = ss / 8;
	
  //attachInterrupt(digitalPinToInterrupt(wakeUpPin),onWakeUp,LOW);
  
  for (byte ii=0;ii<ss;ii++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    //if (keyPressed) ii = ss;
  }
  
  Serial.begin(9600);
  /*
  if (keyPressed) {
    keyPressed = false;
    Serial.println(F("\n--- Manual mode ---"));
    Serial.println(F(">"));
      
    while (!exitF) {
    if (Serial.available() > 0) shutStr = Serial.readString();
      
    if (shutStr.indexOf("cont") >= 0) {
    exitF = true;
    Serial.println(F("Continue operation..."));
    }
    
//    if (manTimeout = manTimeoutExit) {
//      exitF = true;
//      Serial.println(F("=== Timeout ==="));
//    }
      
      shutStr = "";
//    manTimeout++;
    } // while exitF;
  }
  else {
    detachInterrupt(digitalPinToInterrupt(wakeUpPin));
  }
  //SoftwareSerial esp(3,2); //RX, TX. Crete serial port for ESP-12E
  */
  Serial.println(F("\n\nSystem restore!"));
}

void checkVcc(String dest){
	Serial.print(F("Battery check with ")); Serial.print(dest); Serial.print(F(": "));
	analogRead(6);
	bitSet(ADMUX,3);
	delayMicroseconds(300);
	bitSet(ADCSRA,ADSC);
	
	while (bit_is_set(ADCSRA, ADSC));
	
	batVDC = int ((110L*1023)/ADC) ;
	
	if (batVDC < 330) { // less than 3.3 VDC
		Serial.print(F("=== CHARGE BATTERY === "));
		sound(CHARGE_BATTERY_1); // for the moment one battery only
	}
	else {
		Serial.print(F("OK! "));
	}
	//Serial.print(F("Voltage: "));
	Serial.print(String(batVDC/100.00)); Serial.println(F(" VDC"));
}

void loop() {
  ScanAndSend("103"); // build #
  sleepFor(sleepTime); // in seconds
}