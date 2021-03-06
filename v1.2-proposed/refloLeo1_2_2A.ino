#include <SdFat.h>
#include "Adafruit_MAX31855.h"
#include <LiquidCrystal.h>
#include <CapacitiveSensor.h>

int thermoCLK = A4; //F1
int thermoCS = A3;  //F4
int thermoDO = A5;  //F0

#define relayPin 11
#define relayAux 3
#define thresh 2200 //capacitive touch detect threshold

CapacitiveSensor cs_up = CapacitiveSensor(13,A9);  // C7,B5 (32,29)
CapacitiveSensor cs_dn = CapacitiveSensor(A2,7);   // F5,E6 (38,1)
CapacitiveSensor cs_sel = CapacitiveSensor(A0,A1); // F7,F6 (36,37)
byte capButtonStat[3]={0,0,0};

// Initialize the Thermocouple
Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO);

// initialize the LCD library with the numbers of the interface pins
/*
RS=A7    // (D7 27)
E=A8    //  (B4 28)
DB4=5   //  (C6 31)
DB5=A10 //  (B6 30)
DB6=1   //  (D3 21)
DB7=A6  //  (D4 25)
*/
LiquidCrystal lcd(A7,A8,5,A10,1,A6);

SdFat sdcard;
SdFile SDfile;  
SdFile SDfileLog;

char currInstruction = 'O';
unsigned long currTime;
unsigned long timers[4] = {0,0,0};
byte instructionStatus = 0;
unsigned long instructionExpire;
double currTemp = 0;
char parameter = ' ';
char v1_str[4];
char v2_str[4];
long v1_int;
long v2_int;
int selectedprofile = -1;
byte tempindex=0; //stores incremental time iterations for recording temperatures in output file
byte logFileStat=0; //

void setup() {
  pinMode(relayPin, OUTPUT);
  pinMode(relayAux, OUTPUT);
  
  Serial.begin(9600);  

  lcd.begin(16, 2);

  if(!sdcard.begin(SS, SPI_HALF_SPEED)){
    Serial.println("SD initialization failed!");
    updateLCD("No SD Memory","Card Detected ");
    sdcard.initErrorHalt();
    }
  
  if( sdcard.exists("selftest") )
    selfTest();

  updateLCD("Select", "Profile: 0");
 
  Serial.println("getting profile");
  
  while(selectedprofile < 0){
    selectedprofile = getProfile();
    if(selectedprofile >= 0){
      Serial.println("checking profile");
      if(checkProfile() == 0)
        selectedprofile = -1;
        }
      }

  Serial.println("opening file");

  sdRWfile('o',selectedprofile,'i');  // Open profile input file
  sdRWfile('o',selectedprofile,'o');  // Open profile output file
  logFileStat=1;

  while(checkcapbutton(2));  //wait until select button is released

  Serial.println("going to main loop");

} 

////////////////////////////////////////////
void selfTest(void){
  if(SDfile.open("selftest", O_WRITE)){
    updateLCD("Open selftest","SUCCESS");
    }
  else{
    updateLCD("Open selftest","FAILED");
    }

  delay(3000);

  byte fets[2]={0,128};
  
  while(checkcapbutton(2) == 0){
    double c = thermocouple.readCelsius();
    char c_str[7];
    dtostrf(c,5,1,&c_str[0]);

    char disp[17] = "Temp:";
    strcat(disp,c_str);

    if(checkcapbutton(0))
      strcat(disp," UP");

    if(checkcapbutton(1))
      strcat(disp," DOWN");

    updateLCD("Running-SelfTest", disp);

    analogWrite(relayPin,fets[0]+=6);
    analogWrite(relayAux,fets[1]+=6);    
    }

  if(SDfile.remove())
    updateLCD("Remove selftest", "SUCCESS");
  else
    updateLCD("Remove selftest", "FAILED");

  while(1);
  }

////////////////////////////////////////////
byte checkProfile(void){
  char tempc[5]={'\0','\0','\0','\0','\0'};
  char fileName[10]={'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
  itoa(selectedprofile,tempc,10);
  strcat(fileName,tempc);
  strcat(fileName,".txt");
  if( sdcard.exists(fileName) )
    return(1);
  else{
    updateLCD("Profile Does Not","Exist");
    delay(3000);
    updateLCD("Select", "Profile: 0");
    return(0);
    }
  }

////////////////////////////////////////////
byte getProfile(void){
  int sp = -1;
  byte displayedProfile=0;

  while(sp < 0){
    byte bctr=0;
    char buff[4];

    for(bctr=0;bctr<3;bctr++){
      byte bstat=checkcapbutton(bctr);
      if(bstat != capButtonStat[bctr]){
        capButtonStat[bctr]=bstat;
        if(bstat==1){
          char disp[17] = "Profile: ";
          switch(bctr){
            case 0:
              displayedProfile++;
              itoa(displayedProfile, buff, 10);
              strcat(disp,buff);
              updateLCD("Select", disp);
              Serial.println("up");
              break;
            case 1:
              displayedProfile--;
              itoa(displayedProfile, buff, 10);
              strcat(disp,buff);
              updateLCD("Select", disp);
              Serial.println("down");
              break;
            case 2:
              sp = displayedProfile;
              Serial.println("select");
              break;
          }
        }
      }
    } 
  }
  return(sp);
  }

////////////////////////////////////////////
byte checkcapbutton(byte bnum){
  long tstat=0;
  switch(bnum){
    case 0:
      tstat=cs_up.capacitiveSensor(30);
      break;
    case 1:
      tstat=cs_dn.capacitiveSensor(30);
      break;
    case 2:
      tstat=cs_sel.capacitiveSensor(30);
      break;
    }
  if(tstat > thresh)
    return(1);
  else
    return(0);
  }

////////////////////////////////////////////
void updateLCD(char *s1, char *s2){
  char firstline[17] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','\0'};
  char secline[17] =   {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','\0'};

  byte ele=0;
  while(s1[ele] != '\0' && ele < 18){
    firstline[ele] = s1[ele];
    ele++;
    }

  ele=0;
  while(s2[ele] != '\0' && ele < 18){
    secline[ele] = s2[ele];
    ele++;
    }
    
  lcd.setCursor(0, 0);
  lcd.print(firstline);
  lcd.setCursor(0, 1);  
  lcd.print(secline);
  }

////////////////////////////////////////////
byte sdRWfile(char op, byte fileNm, char ftype){

  if(op=='o' && ftype=='i'){
    char fileName[10]={'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
    char tempc[4];
    itoa(fileNm,tempc,10);
    strcat(fileName,tempc);
    strcat(fileName,".txt");
    if(!SDfile.open(fileName, O_READ)){
      Serial.println("opening file for reading failed");
      return(0);
      }
    }  

  else if(op=='o' && ftype=='o'){
    char fileName[10]={'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
    char tempc[4];
    itoa(fileNm,tempc,10);
    strcat(fileName,tempc);
    strcat(fileName,"-out.txt");
    if(!SDfileLog.open(fileName, O_RDWR | O_CREAT | O_TRUNC)){
      Serial.println("opening file for writing failed");
      return(0);
      }
    }

  else if(op=='c' && ftype=='i'){
    SDfile.close();
    }

  else if(op=='c' && ftype=='o'){
    SDfileLog.close();
    }

  return(1);
  }

//////////////////////////////////////////////
byte sdRW(char *buff, char op, byte memlength){
  byte fctr=0;

  if(op=='i'){
    byte gotchar;

    for(fctr=0;fctr<25;fctr++){
      gotchar = SDfile.read();
      if(gotchar != 10 && gotchar != 13)
        buff[fctr] = char(gotchar);
      if(gotchar == 13){
        buff[fctr]='\0';
        break;   //stop reading at end of line <CR>
        }
      if(gotchar == 10)
        fctr--;
      }
    if(fctr>=25) return(0);
    else return(fctr);
    } 

  else if(op=='o'){
    for(fctr=0; fctr < memlength; fctr++){
      SDfileLog.print(char(buff[fctr]));
      }
    SDfileLog.print(char(10));
    SDfileLog.print(char(13));
    }
  }

//////////////////////////////////////////////
byte checkHold(void){
  if(currTime > instructionExpire){
    instructionStatus = 0;
    }
  else {
    int delta = v1_int - currTemp;
    if(delta > 0)
      digitalWrite(relayPin,HIGH);
    else
      digitalWrite(relayPin,LOW);
    }
  }

//////////////////////////////////////////////
byte checkTemp(void){
  int delta = currTemp - v1_int;

  if(delta >= 0){
    instructionStatus = 0;
    }
  else{
    digitalWrite(relayPin,HIGH);
    }
  }

//////////////////////////////////////////////
byte checkWait(void){
  digitalWrite(relayPin,LOW);
  if(currTime > instructionExpire)
    instructionStatus = 0;
  }

///////////////////////////////////////////////
void getInstruction(void){
  char instruction[9] = "";
  sdRW(instruction,'i',0);

  byte v1_offset=1;
  if(instruction[1] == '0')
    v1_offset=2;

  byte vctr=v1_offset - 1;
  for(vctr=0;vctr<3+1-v1_offset;vctr++){
    v1_str[vctr]=instruction[vctr+v1_offset];
    }
  v1_str[vctr]='\0';

  byte v2_offset=5;
  if(instruction[5] == '0')
    v2_offset=6;

  vctr=v2_offset - 5;
  for(vctr=0;vctr<3+5-v2_offset;vctr++){
    v2_str[vctr]=instruction[vctr+v2_offset];
    }
  v2_str[vctr]='\0';

  v1_int = atoi(v1_str);
  v2_int = atoi(v2_str);

  currInstruction = instruction[0];

  if(instruction[0]=='H' || instruction[0]=='W'){ 
    Serial.println(v2_int);
    instructionExpire = currTime + (v2_int * 1000);
    }

  instructionStatus=1;
  }

///////////////////////////////////////////////
void calcTimeLeft(char *timeLeft){
  byte strSize;
  double tdiff = (instructionExpire - currTime) / 1000;
  if(tdiff > 99)
    strSize=3;
  else if(tdiff > 9)
    strSize=2;
  else
    strSize=1;
    
  dtostrf(tdiff,strSize,0,&timeLeft[0]);
  }

///////////////////////////////////////////////
void updateDisplay(void){
    
  char dispInstruction[17];
  char timeleft[4]="";

  if(currInstruction == 'H'){
    strcpy(dispInstruction,"HOLD:");
    strcat(dispInstruction,v1_str);
    strcat(dispInstruction," | ");
    calcTimeLeft(timeleft);
    strcat(dispInstruction,timeleft);
    }
  else if(currInstruction == 'T'){
    strcpy(dispInstruction,"GO TO: ");
    strcat(dispInstruction,v1_str);
    }
  else if(currInstruction == 'W'){
    strcpy(dispInstruction,"WAIT: ");
    calcTimeLeft(timeleft);
    strcat(dispInstruction,timeleft);
    //strcat(dispInstruction,v2_str);
    }
  else if(currInstruction == 'X')
    strcpy(dispInstruction,"EXIT");
    
  char c_str[7];
  dtostrf(currTemp,5,1,&c_str[0]);

  Serial.print("Temp=");
  Serial.print(currTemp);
  Serial.print(" | ");
  Serial.println(currInstruction);

  char disp[17] = "Temp:";
  strcat(disp,c_str);
  updateLCD(dispInstruction, disp);

  }


///////////////////////////////////////////////
void checkExit(void){
  instructionStatus=0;
  sdRWfile('c',selectedprofile,'i');    
  digitalWrite(relayPin,LOW);
  }

///////////////////////////////////////////////
void logTemp(void){

  if(currTemp < 125 && logFileStat == 2){
    sdRWfile('c',selectedprofile,'o');        
    logFileStat=0;
    updateLCD("   Log Closed   ","");  
    delay(2000);
    }

  if(logFileStat == 1 && currTemp > 125)
    logFileStat=2;

  if(logFileStat > 0){  
    char tbuff[10];
    char tempIndexStr[4];

    char c_str[7];
    if(currTemp >= 100) dtostrf(currTemp,5,1,&c_str[0]);
    else dtostrf(currTemp,4,1,&c_str[0]);

    itoa(tempindex,&tempIndexStr[0],10);

    strcpy(tbuff, tempIndexStr);
    strcat(tbuff, ",");
    strcat(tbuff, c_str);

    sdRW(tbuff,'o',strlen(tbuff));
    tempindex++;
    }
  }

void updateTemp(){
  double c = thermocouple.readCelsius();
  if(c > 10 && c < 285) //filters out erroneous thermocouple readings
    currTemp = c;
  }

///////////////// Main Instruction Set ////////////////////////////// 
/*
T = Heat to specified temperature
H = Hold above specified temperature
W = Wait specified amount of time (in seconds)
X = Exit program (close file, deactivate heat)

Time and temperature always given in 3 digit format

Example profile (don't include spaces or text at right below):
T130,000          Goto 130 degrees
W000,015          Wait 15 seconds (and turn off heat)
H150,120          Hold at 150 degrees for 120 seconds
T212,000          Goto 212 degrees
W000,020          Wait 20 seconds (and turn off heat)
H225,020          Hold at 225 degrees for 20 seconds
X000,000          Exit program and turn off relay

Timers:
 0 = update diaplay
 1 = when to visit current instruction again for update
*/

void loop() {

  currTime = millis();

  if(currTime > timers[3]){
    timers[3] = currTime + 1000;
    updateTemp();
    }
  
  if(instructionStatus == 0 && currInstruction != 'X')
    getInstruction();
  
  if(currTime > timers[1] && instructionStatus == 1){
    timers[1] = currTime + 1000;
    if(currInstruction=='T') {
      checkTemp();
      }
    else if(currInstruction=='H'){
      checkHold();
      }
    else if(currInstruction=='W'){
      checkWait();
      }
    else if(currInstruction=='X'){
      checkExit();
      }
    }

  if(currTime > timers[0]){
    timers[0] = currTime + 1000;
    updateDisplay();
    }

  if(currTime > timers[2]){
    timers[2] = currTime + 10000;
    logTemp();
    }

  if(checkcapbutton(2) && logFileStat > 0){
    sdRWfile('c',selectedprofile,'o');
    updateLCD("   Log Closed   ","");  
    delay(1000);
    }

  //Safety
  if( (int) currTemp > 275){
    updateLCD(" Error: Invalid ", "  Temperature   ");
    digitalWrite(relayPin,LOW);
    digitalWrite(relayAux,LOW);
    while(1);
    }


  }
  
