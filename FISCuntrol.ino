//Include FIS Writer and KWP
#include "VW2002FISWriter.h"
#include "KWP.h"
#include "AnalogMultiButton.h" // https://github.com/dxinteractive/AnalogMultiButton
/* uncomment to enable boot message and boot image. Removed due excessive memory consumption */
#define Atmega32u4
#ifndef Atmega32u4 
  #include "GetBootMessage.h"
  #define bootmsg
  #define bootimg
#endif

// KWP 
#define pinKLineRX 5
#define pinKLineTX 4
KWP kwp(pinKLineRX, pinKLineTX);

// CDC
/*const byte dinCDC = 6;
const byte doutCDC = 7;
const byte clkCDC = 8; */

//Buttons
#define btn1PIN A3
#define btn2PIN A2
const uint8_t BUTTONS_TOTAL = 2; // 2 button on each button pin
const unsigned int BUTTONS_VALUES[BUTTONS_TOTAL] = {30, 123}; // btn value
AnalogMultiButton btn1(btn1PIN, BUTTONS_TOTAL, BUTTONS_VALUES); // make an AnalogMultiButton object, pass in the pin, total and values array
const uint8_t btn_NAV = 30;
const uint8_t btn_RETURN=123;
AnalogMultiButton btn2(btn2PIN, BUTTONS_TOTAL, BUTTONS_VALUES);
const uint8_t btn_INFO = 30;
const uint8_t btn_CARS = 123; 

// FIS
const uint8_t FIS_CLK = 15;  // 
const uint8_t FIS_DATA = 14; // 
const uint8_t FIS_ENA = 16;   // 

// KWP connection settings
const int NENGINEGROUPS = 20;
const int NDASHBOARDGROUPS = 1;
const int NABSGROUPS = 1;
const int NAIRBAGGROUPS = 1;
const int MAX_CONNECT_RETRIES = 5;
const int NMODULES = 3;

//define engine groups
//**********************************0, 1,2, 3, 4, 5, 6,   7,  8,  9,  10, 11, 12, 13, 14, 15, 16,  17,  18,  19)
//Block 3, 5 don't work?
int engineGroups[NENGINEGROUPS] = {2, 3, 4, 5, 6, 10, 11, 14, 15, 16, 20, 31, 32, 90, 91, 92, 113, 114, 115, 118};   //defined blocks to read
int dashboardGroups[NDASHBOARDGROUPS] = {2};                    //dashboard groups to read
int absGroups[NDASHBOARDGROUPS] = { 1 };                          //abs groups to read
//int airbagGroups[NDASHBOARDGROUPS] = { 1 };                       //airbag groups to read

//define engine as ECU, with ADR_Engine (address)
KWP_MODULE engine = {"ECU", ADR_Engine, engineGroups, NENGINEGROUPS};
KWP_MODULE dashboard = {"CLUSTER", ADR_Dashboard, dashboardGroups, NDASHBOARDGROUPS};
KWP_MODULE absunit = {"ABS", ADR_ABS_Brakes, absGroups, NABSGROUPS};
//KWP_MODULE airbag = {"AIRBAG", ADR_Airbag, airbagGroups, NAIRBAGGROUPS};

//define modules
KWP_MODULE *modules[NMODULES] = { &engine, &dashboard, &absunit };// &_abs, &airbag};
KWP_MODULE *currentModule = modules[0];

VW2002FISWriter fisWriter(FIS_CLK, FIS_DATA, FIS_ENA);

#ifdef bootmsg
  GetBootMessage getBootMessage;
 // #ifdef bootimg
 //   GetBootImage getBootImage;
 // #endif
#endif

bool ignitionState = false;         // variable for reading the ignition pin status
bool ignitionStateRunOnce = false;  // variable for reading the first run loop
bool removeOnce = true;             // variable for removing the screen only once
bool successfulConn = false;
bool successfulConnNoDrop = false;
bool fisDisable = false;            // is the FIS turned off?
bool fisBeenToggled = false;        // been toggled on/off?  Don't display welcome if!
bool isCustom = false;              // is the data block custom?
bool fetchedData = false;           // stop flickering when waiting on data

String GreetingMessage1 = ""; String GreetingMessage2 = ""; String GreetingMessage3 = ""; String GreetingMessage4 = "";
String GreetingMessage5 = ""; String GreetingMessage6 = "";  String GreetingMessage7 = ""; String GreetingMessage8 = "";
String definedUnitsPressure = "mbar";
String fisLine1; String fisLine2; String fisLine3; String fisLine4; String fisLine5; String fisLine6; String fisLine7; String fisLine8;

//currentSensor ALWAYS has to be 0 (if displaying all 4!)
//Change currentGroup for the group you'd like above!^
byte intCurrentModule = 0;
byte currentGroup = 0;
byte currentSensor = 0;
byte nSensors = 0;
byte nGroupsCustom = 1;
byte nGroupsCustomCount = 1;
byte maxSensors = 4;
byte maxAttemptsModule = 3;
byte maxAttemptsCountModule = 1;
byte lastGroup = 0;                  // remember the last group when toggling from custom
byte lastRefreshTime;               // capture the last time the data was refreshed
int refreshTime = 50;              // time for refresh in ms.  100 works.  250 works.  50 works.  1?
byte delayTime = 5;
int startTime = 10000;            // start up time = 1 minute

byte keyStatus;                   // current key status (see below)

//****************************************//

int getKeyStatus() {
  btn1.update();
  btn2.update();
  if ( btn1.isPressedAfter(btn_NAV,2000) ) return 4;  // disable screen after holding more than 2 sec
  if ( btn1.isPressed(btn_RETURN) ) return 1;         // return and cars switch between modules
  if ( btn1.isPressedAfter(btn_RETURN,2000) ) return 11;         // return and cars switch between modules
  if ( btn2.isPressed(btn_CARS) ) return 2;
  if ( btn2.isPressedAfter(btn_CARS,2000) ) return 21;
  if ( btn2.isPressed(btn_INFO) ) return 3;           // switch between groups
  else return 0;
}

void setup()
{
  //Initialise basic varaibles
 // Serial.begin(115200);
  //pinMode(stalkPushUpReturn, OUTPUT); pinMode(stalkPushDownReturn, OUTPUT); pinMode(stalkPushResetReturn, OUTPUT);

  //Cleanup, even remove last drawn object if not already/disconnect from module too.
  ignitionStateRunOnce = false;
  maxAttemptsCountModule = 1;
  intCurrentModule = 0;
  currentGroup = 0;
  lastGroup = 0;
  removeOnce = true;
  successfulConn = false;
  successfulConnNoDrop = false;
  fisBeenToggled = false;
  fetchedData = false;
  kwp.disconnect();
  fisWriter.remove_graphic();
  ignitionState = HIGH;

  long current_millis = (long)millis();
  long lastRefreshTime = current_millis;
}

void loop()
{
  //Check to see the current state of the digital pins (monitor voltage for ign, stalk press)
 // ignitionState = digitalRead(ignitionMonitorPin);

  //refresh the current "current_millis" (for refresh rate!)
  long current_millis = (long)millis();

  //update the Reset button (see if it's been clicked more than 2 times (there 3+)
  //if it has, toggle fisDisable to turn off/on the screen
  keyStatus = getKeyStatus(); // if NAV (disable) pressed for 2 sec - disable screen and disconnect KWP
  if (keyStatus == 4)
  {
    fisDisable = !fisDisable;   //flip-flop disDisable

    //set disBeenToggled to true to stop displaying the "good morning" on turn-on (only this session)
    fisBeenToggled = false;
    removeOnce = false;
    successfulConn = false;
    successfulConnNoDrop = false;
    fetchedData = false;
    maxAttemptsCountModule = 1;
    kwp.disconnect();
    fisWriter.remove_graphic();
    return;
  }

  //check the ign goes dead, prep the variables for the next run
  if (ignitionState == LOW)
  {
    ignitionStateRunOnce = false;
    removeOnce = false;
    successfulConn = false;
    successfulConnNoDrop = false;
    fisBeenToggled = false;
    fetchedData = false;
    maxAttemptsCountModule = 1;
    kwp.disconnect();
    fisWriter.remove_graphic();
  }

  //If the ignition is currently "on" then work out the message
  if (ignitionState == HIGH && !ignitionStateRunOnce) //&& !fisBeenToggled)
  {
    // Serial.println("Return & Display Boot Message");
    #ifdef bootmsg
      getBootMessage.returnBootMessage();         //find out the boot message
        #ifdef bootimg
          getBootMessage.displayBootImage();        //display the boot message
        #endif
      getBootMessage.displayBootMessage();    //TODO: build graph support!
    #endif
    ignitionStateRunOnce = true;            //set it's been ran, to stop redisplay of welcome message until ign. off.
  }

  //if the screen isn't disabled, carry on
  if (!fisDisable && ignitionState == HIGH)
  {
    if (ignitionState == 3)   //see if the down button has been pressed.  If it's been held, it will return -1
    {
      //if the down button has been held, prepare to swap groups!
      //use return; to get the loop to refresh (pointers need refreshing!)
      currentGroup = 0;
      successfulConn = false;
      successfulConnNoDrop = false;
      maxAttemptsCountModule = 1;
      fisLine1 = ""; fisLine2 = ""; fisLine3 = ""; fisLine4 = ""; fisLine5 = ""; fisLine6 = ""; fisLine7 = ""; fisLine8 = ""; //empty the lines from previous
      fetchedData = false;

      if ((intCurrentModule + 1) > (NMODULES - 1))        //if the next module causes a rollover, start again
      {
        currentModule = modules[0];                       //reset the currentModule to 0
        intCurrentModule = 0;                             //reset the intCurrentModule to 0 too
        kwp.disconnect();                                 //disconnect kwp
        fisWriter.init_graphic();                         //clear the screen
        return;                                           //return (to bounce back to the start.  Issues otherwise).
      }
      else
      {
        //rollover isn't a thing, so go to the next module.  Needs a "return" to allow reset of module
        intCurrentModule++;
        currentModule = modules[intCurrentModule];
        kwp.disconnect();
        fisWriter.init_graphic();
        return;
      }
    }

    //if the previous module causes a rollover, start again
    if (keyStatus == 3 ) // if info pressed -> switch module in the current group
    {
      if ((currentGroup + 1) > (NENGINEGROUPS - 1))
      {
        currentGroup = 0;
        fisLine1 = ""; fisLine2 = ""; fisLine3 = ""; fisLine4 = ""; fisLine5 = ""; fisLine6 = ""; fisLine7 = ""; fisLine8 = ""; //empty the lines from previous
        fetchedData = false;
        fisWriter.init_graphic();
        return;
      }
      else
      {
        //rollover isn't a thing, so go to the next module.  Needs a "return" to allow reset of module
        currentGroup++;
        fisLine1 = ""; fisLine2 = ""; fisLine3 = ""; fisLine4 = ""; fisLine5 = ""; fisLine6 = ""; fisLine7 = ""; fisLine8 = ""; //empty the lines from previous
        fetchedData = false;
        fisWriter.init_graphic();
        return;
      }
    }

    //check to see if "RETURN" is held.  If it is, toggle "isCustom"
    if ( keyStatus == 11 )
    {
      isCustom = !isCustom; //toggle isCustom
      if (isCustom == true)
      {
        lastGroup = currentGroup;
      }
      else
      {
        currentGroup = lastGroup;
      }

      fisLine1 = ""; fisLine2 = ""; fisLine3 = ""; fisLine4 = ""; fisLine5 = ""; fisLine6 = ""; fisLine7 = ""; fisLine8 = ""; //empty the lines from previous
      nGroupsCustom = 1;
      fetchedData = false;
      fisWriter.init_graphic();
      return;
    }

    // switch group if btn_CARS is pressed
    if (keyStatus == 2) 
    {
      if ( keyStatus == 2 && (currentGroup - 1) < 0)
      {
        currentGroup = NENGINEGROUPS - 1;
        fisLine1 = ""; fisLine2 = ""; fisLine3 = ""; fisLine4 = ""; fisLine5 = ""; fisLine6 = ""; fisLine7 = ""; fisLine8 = ""; //empty the lines from previous
        fetchedData = false;
        fisWriter.init_graphic();
        return;
      }
      else
      {
        currentGroup--;
        fisLine1 = ""; fisLine2 = ""; fisLine3 = ""; fisLine4 = ""; fisLine5 = ""; fisLine6 = ""; fisLine7 = ""; fisLine8 = ""; //empty the lines from previous
        fetchedData = false;
        fisWriter.init_graphic();
        return;
      }
    }

    //if KWP isn't connected, ign is live and the welcome message has just been displayed
    if (!kwp.isConnected())
    {
      if ((maxAttemptsCountModule > maxAttemptsModule) && ignitionStateRunOnce)
      {
        fisWriter.remove_graphic();
      }

      if ((maxAttemptsCountModule <= maxAttemptsModule) && ignitionStateRunOnce)
      {
        //Reconnect quietly if already connected (don't show that you're reconnecting!)
        if (!successfulConnNoDrop)
        {
          fisWriter.FIS_init(); delay(delayTime);
          fisWriter.init_graphic(); delay(delayTime);

          fisWriter.write_text_full(0, 24, "CONNECTING TO:"); delay(delayTime);
          fisWriter.write_text_full(0, 32, currentModule->name); delay(delayTime);
          fisWriter.write_text_full(0, 48, "ATTEMPT " + String(maxAttemptsCountModule) + "/" + String(maxAttemptsModule)); delay(delayTime);
        }

        if (kwp.connect(currentModule->addr, 10400))
        {
          maxAttemptsCountModule = 1;
          successfulConn = 1;
        }
        else
        {
          maxAttemptsCountModule++;
        }
      }
    }
    else
    {
      //If the first connection was successful, don't show reconnects!
      if (!successfulConnNoDrop)
      {
        fisWriter.init_graphic();
        fisWriter.write_text_full(0, 24, "CONNECTED TO:"); delay(5);
        fisWriter.write_text_full(0, 32, currentModule->name); delay(5);
        delay(500);
        successfulConnNoDrop = true;
      }

      //only refresh every "refreshTime" ms (stock is 500ms)
      if ((current_millis - lastRefreshTime) > refreshTime)
      {
        //grab data for the currentGroup.
        SENSOR resultBlock[maxSensors];
        nSensors = kwp.readBlock(currentModule->addr, currentModule->groups[currentGroup], maxSensors, resultBlock);

        //if the data is in a stock group (!isCustom), then fill out as normal
        if (!isCustom)
        {
          //stop "slow" fetching of data.  Remember that it refreshes variables every 500ms, so the first "grab" grabs each block every 500ms.  SLOW!
          //this makes sure that ALL the data is captured before displaying it.  Each "length" must have a value, so /8 > 1 will catch all
          if ((fisLine3.length() > 1) && (fisLine4.length() > 1) && (fisLine5.length() > 1) && (fisLine6.length() > 1) && fetchedData == false)
          {
            fetchedData = true;
            fisWriter.init_graphic();
          }

          if ((fisLine3.length() > 1) && (fisLine4.length() > 1) && (fisLine5.length() > 1) && (fisLine6.length() > 1) && fetchedData == true)
          {
            fisWriter.write_text_full(0, 8, "BLOCK " + String(engineGroups[currentGroup]));
            fisWriter.write_text_full(0, 16, resultBlock[currentSensor].desc);
            //only write lines 3, 4, 5, 6 (middle of screen) for neatness
          
            fisWriter.write_text_full(0, 40, fisLine3); delay(delayTime);
            fisWriter.write_text_full(0, 48, fisLine4); delay(delayTime);
            fisWriter.write_text_full(0, 56, fisLine5); delay(delayTime);
            fisWriter.write_text_full(0, 64, fisLine6); delay(delayTime);
          }
          else
          {
            fisWriter.write_text_full(0, 48, "FETCHING DATA"); delay(delayTime);
            fisWriter.write_text_full(0, 56, "PLEASE WAIT..."); delay(delayTime);
          }

          if (resultBlock[0].value != "") {
            fisLine3 = resultBlock[0].value + " " + resultBlock[0].units;
          } else {
            fisLine3 = "  " ;
          }

          if (resultBlock[1].value != "") {
            fisLine4 = resultBlock[1].value + " " + resultBlock[1].units;
          } else {
            fisLine4 = "  " ;
          }

          if (resultBlock[2].value != "") {
            fisLine5 = resultBlock[2].value + " " + resultBlock[2].units;
          } else {
            fisLine5 = "  " ;
          }

          if (resultBlock[3].value != "") {
            fisLine6 = resultBlock[3].value + " " + resultBlock[3].units;
          } else {
            fisLine6 = "  " ;
          }
          lastRefreshTime = current_millis;
          nGroupsCustom = 0;
        }
        else
        {
          //if isCustom, then same idea, only use ALL the lines
          //only populating Line 2, 3, 4, 7, 8, so wait until ALL lines are filled before displaying.  Looks cleaner
          if ((fisLine2.length() > 1) && (fisLine4.length() > 1) && (fisLine5.length() > 1) && (fisLine6.length() > 1) && (fisLine7.length() > 1) && (fisLine8.length() > 1) && fetchedData == false)
          {
            fetchedData = true;
            fisWriter.init_graphic();
          }

          if ((fisLine2.length() > 1) && (fisLine4.length() > 1) && (fisLine5.length() > 1) && (fisLine6.length() > 1) && (fisLine7.length() > 1) && (fisLine8.length() > 1) && fetchedData == true)
          {
            fisWriter.write_text_full(0, 8, "CUSTOM BLOCK"); delay(delayTime);
            fisWriter.write_text_full(0, 16, "AIT & KNOCK"); delay(delayTime);
            fisWriter.write_text_full(0, 32, fisLine2); delay(delayTime);
            fisWriter.write_text_full(0, 48, fisLine4); delay(delayTime);
            fisWriter.write_text_full(0, 56, fisLine5); delay(delayTime);
            fisWriter.write_text_full(0, 64, fisLine6); delay(delayTime);
            fisWriter.write_text_full(0, 72, fisLine7); delay(delayTime);
            fisWriter.write_text_full(0, 80, fisLine8); delay(delayTime);
          }
          else
          {
            fisWriter.write_text_full(0, 48, "FETCHING DATA"); delay(delayTime);
            fisWriter.write_text_full(0, 56, "PLEASE WAIT..."); delay(delayTime);
            //fisWriter.init_graphic();
          }

          //first run will be case 0 (since nGroupsCustom = 1).  Prepare for the NEXT run (which will be group 2 (AIT/timing)).
          switch (nGroupsCustom - 1)
          {
            //                                   0  1   2   3   4   5   6   7   8   9,  10, 11, 12, 13, 14, 15,  16,  17,  18
            //int engineGroups[NENGINEGROUPS] = {2, 3, 4, 5, 6, 10, 11, 14, 15, 16, 20, 31, 32, 90, 91, 92, 113, 114, 115, 118}; 
            case 0:
              nGroupsCustom++;
              currentGroup = 6;
              lastRefreshTime = current_millis;
              return;
            case 1:
              //this is the data from the ABOVE group (group 2).  Prepare for the NEXT group
              fisLine2 = "AIT: " + resultBlock[2].value + " " + resultBlock[2].units; //Group 11, Block 3, AIT
              fisLine4 = resultBlock[3].value + " " + resultBlock[3].units; //Group 11, Block 4, Timing Angle
              nGroupsCustom++;
              currentGroup = 10;
              lastRefreshTime = current_millis;
              return;
            case 2:
              //this is the data from the ABOVE group (group 4).  Prepare for the NEXT group
              fisLine5 = resultBlock[0].value + " " + resultBlock[0].units; //Group 20, Block 1, Timing Retard (Cyl 1)
              fisLine6 = resultBlock[1].value + " " + resultBlock[1].units; //Group 20, Block 1, Timing Retard (Cyl 2)

              fisLine7 = resultBlock[2].value + " " + resultBlock[2].units; //Group 20, Block 1, Timing Retard (Cyl 3)
              fisLine8 = resultBlock[3].value + " " + resultBlock[3].units; //Group 20, Block 1, Timing Retard (Cyl 4)

              nGroupsCustom = 2;
              currentGroup = 6;
              lastRefreshTime = current_millis;
              return;
          }
        }
      }
    }
  }
  else
  {
    //If FIS Disabled:  Minic the stalk buttons!
    removeOnce = false;
    successfulConn = false;
    successfulConnNoDrop = false;
    fisBeenToggled = false;
    fetchedData = false;
    maxAttemptsCountModule = 1;
    kwp.disconnect();
    fisWriter.remove_graphic();

    //Force HIGH on all the return pins
    // // Serial.println("Disabled.  Force pins HIGH");
    //digitalWrite(stalkPushUpReturn, HIGH);
    //digitalWrite(stalkPushDownReturn, HIGH);
    //digitalWrite(stalkPushResetReturn, HIGH);

    //if the Pins are held LOW (therefore, false), hold the corresponding pin LOW, too. - transfer stalk button to dashboard
    /*if (!digitalRead(stalkPushDownMonitor))
    {
      // // Serial.println("Disabled.  Force Down LOW");
      for (int i = 0; i <= 1500; i++) {
        if (digitalRead(stalkPushDownMonitor)) {
          i = 1700;
        }
        else {
          // // Serial.println("Down LOW");
          digitalWrite(stalkPushDownReturn, LOW);
        }
        delay(1);
      }
    }

   if (!digitalRead(stalkPushUpMonitor))
    {
      // // Serial.println("Disabled.  Force Up LOW");
      for (int i = 0; i <= 1500; i++) {
        if (digitalRead(stalkPushUpMonitor)) {
          i = 1700;
        }
        else {
          // // Serial.println("Up LOW");
          digitalWrite(stalkPushUpReturn, LOW);
        }
        delay(1);
      }
    }

   if (!digitalRead(stalkPushResetMonitor))
    {
      // // Serial.println("Disabled.  Force Reset LOW");
      //read the reset pin.  If it's held for more than 1700 ms, jump
      for (int i = 0; i <= 1500; i++) {
        if (digitalRead(stalkPushResetMonitor)) {
          i = 1700;
        }
        else {
          // // Serial.println("Reset LOW");
          digitalWrite(stalkPushResetReturn, LOW);
        }
        delay(1);
      }
    }*/
  }
}
