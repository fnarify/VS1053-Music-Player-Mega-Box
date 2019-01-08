/**
 * Example program to check the values provided from an infrared remote.
 * Uses the same input pin for the remote as the VS1053 project code.
 */
#include <IRremote.h>

IRrecv IR(3);
decode_results results;

void setup()
{
  Serial.begin(9600);
  Serial.println("Hello world");
  IR.enableIRIn();
}

void loop()
{                               
    if (IR.decode(&results))
    {
        Serial.print("Got IR code: 0x");
        Serial.println(results.value, HEX);
        delay(200); // a 200ms delay provides the best debounce time
        IR.resume();
    }
}
