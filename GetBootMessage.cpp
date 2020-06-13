#include "GetBootMessage.h"
#include "VW2002FISWriter.h"
#include "Wire.h"
#include "VWbitmaps.h"

void GetBootMessage::returnBootMessage()
{
  GreetingMessage3 = "WELCOME";
  GreetingMessage4 = "ONBOARD";
}

void GetBootMessage::displayBootMessage()
{
  //Init the display and clear the screen
  fisWriter.FIS_init();
  delay(200);
  fisWriter.init_graphic();

  //Display the greeting.  40/48 is the height.
  fisWriter.write_text_full(0, 24, GreetingMessage1);
  fisWriter.write_text_full(0, 32, GreetingMessage2); delay(5);
  fisWriter.write_text_full(0, 40, GreetingMessage3); delay(5);
  fisWriter.write_text_full(0, 48, GreetingMessage4); delay(5);
  fisWriter.write_text_full(0, 56, GreetingMessage5); delay(5);
  fisWriter.write_text_full(0, 64, GreetingMessage6); delay(5);
  fisWriter.write_text_full(0, 72, GreetingMessage7); delay(5);
  fisWriter.write_text_full(0, 80, GreetingMessage8); delay(5);
  delay(3500);
}

void GetBootMessage::displayBootImage()
{
  //Init the display and clear the screen
  fisWriter.FIS_init();
  delay(200);
  fisWriter.init_graphic();
  delay(5);

  //Display the greeting.  40/48 is the height.
  fisWriter.GraphicFromArray(0, 0, 64, 65, avant, 1);
  fisWriter.GraphicFromArray(0, 70, 64, 16, QBSW, 1);
  delay(3500); 
}
