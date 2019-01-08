/**
 * V1.0
 * 
 * Arduino music player connected with the Altronics Mega Box K9670.
 * 
 * READ BELOW ***********************************************************************************************
 * 
 * VERY IMPORTANT: IF USING ARDUINO MEGA
 * The Arduino Mega has a different set of SPI pins than the Arudino Uno, which means they need to be connected
 * in parallel with jumper wires before the program will work. You'll be presented with this error message if not:
 * 
 * Can't access SD card. Do not reformat.
 *  No card, wrong chip select pin, or SPI problem?
 *  SD errorCode: 0X1,0XFF
 * 
 * The pin connections are as follows:
 * Pin    Uno   Mega
 * MOSI   11    51
 * MISO   12    50
 * SCK    13    52
 * 
 * So just connect the pins on the left to the ones on the right as stated above and everything should work OK.
 * 
 * READ ABOVE ***********************************************************************************************
 * 
 * This version of the project uses an IR remote and LED pushbuttons plus the rotary encoder for controls.
 * If using an Arduino Uno, only two of the pushbuttons are available due to a lack of pins. Use an Arduino
 * Mega if you want to use the two remaining pushbuttons and rotary encoder.
 * 
 * Make sure to change you build options if using an Arduino Mega or Uno in the Arduino IDE at Tools -> Board
 * 
 * Take note of the value of your own IR codes and make sure they match with what is written below, otherwise change one.
 * You can check the value of the IR codes for your remote by running the Altronics sample program, or write a small
 * one yourself.
 * 
 * NOTE: if the patch file "patches.053" is not loaded onto the chip during begin()
 * then the Arduino will not be able to play ogg and some other file types.
 * You also need the file "oggenc.053" created from "venc44k2q05.plg" if you want to
 * record to ogg.
 * 
 * As an aside the differential output option does the following:
 * stereo playback -> create a 'virtual' sound
 * mono playback -> create a differential left/right 3V maximum output
 */
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFEMP3Shield.h>
#include <LiquidCrystal.h>
#include <IRremote.h>

// below is not needed if interrupt driven. Safe to remove if not using
#if defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_Timer1
  #include <TimerOne.h>
#elif defined(USE_MP3_REFILL_MEANS) && USE_MP3_REFILL_MEANS == USE_MP3_SimpleTimer
  #include <SimpleTimer.h>
#endif

#define MENU_SIZE 9  // number of menu items to display
#define MAX_BUFF  13 // max buffer size for serial input (8.3 filename limit)

// LCD dimensions, set as needed
#define LCD_ROWS  2
#define LCD_COLS  16
#define LINE1     0
#define LINE2     1

// IR code information
#define ENTER     0x23
#define UP        0x12
#define DOWN      0x13
#define RIGHT     0x14
#define LEFT      0x15
#define BACK      0x0B
#define PLAY      0x32
#define RESTART   0x36
#define INC_SPD   0x34
#define DEC_SPD   0x37
#define MUTE      0x0D
#define CH_UP     0x21
#define CH_DOWN   0x20
#define INC_VOL   0x10
#define DEC_VOL   0x11

// special value, two's complement -1
#define NO_VAL    0xFF

// Arduino pin connections
#define IR_PIN    3
#define SW1_COM   4
#define SW2_COM   5
#ifdef ARDUINO_AVR_MEGA2560 // if using an Arduino Mega
  #include <Encoder.h>
  #define MAX_INDEX 1250 // max amount of file indices to store (16-bits ec), not needed if not traversing backwards
  #define NUM_SW    4    // number of switches connected
  #define SW3_COM   14
  #define SW4_COM   15
  #define ENC_A     16
  #define ENC_B     17
  Encoder Enc(ENC_A, ENC_B);
  const byte sw_loc[NUM_SW] = {SW1_COM, SW2_COM, SW3_COM, SW4_COM};
  const byte sw_val[NUM_SW] = {ENTER, DOWN, UP, BACK};
#else
  #define MAX_INDEX 75
  #define NUM_SW    2
  const byte sw_loc[NUM_SW] = {SW1_COM, SW2_COM};
  const byte sw_val[NUM_SW] = {ENTER, DOWN};
#endif
#define LCD_RS    A0
#define LCD_EN    A1
#define LCD_D4    A2
#define LCD_D5    A3
#define LCD_D6    A4
#define LCD_D7    A5

// interrupt routine used when recording
#define VS1053_INT_ENABLE 0xC01A
// use line-in for recording
#define USE_LINEIN 1

uint8_t  result;                    // globally check return values from various functions
uint8_t  rec_state;                 // state of recording function.
uint8_t  counter = 0;               // small counter used for menu movement
int      playnum = 0;               // current song being played in the playlist
int      playlen = 0;               // length of f_index, incase we find less than MAX_INDEX files
uint16_t playlist[MAX_INDEX] = {0}; // used to store mp3 file indexes, letting us go backwards and forwards

// menu strings (restricted to LCD screen width)
char menubuff[LCD_COLS + 1];
PROGMEM const char menu[MENU_SIZE][LCD_COLS + 1] = {
  "0.Playlist",
  "1.Play track num",
  "2.Record",
  "3.Mono/stereo",
  "4.Reset VS1053",
  "5.Sinewave test",
  "6.Differ. output",
  "7.Turn off chip",
  "8.Turn on chip",
};

SdFat          sd;
SdFile         file;
SFEMP3Shield   MP3player;
LiquidCrystal  lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
IRrecv         IR(IR_PIN);
decode_results IRcode;

// equalises the value of the IR code so that it is similar for either button press
#define getcode() ((IRcode.value ^ 0x800) & IRcode.value)
// macros for printing to an LCD panel.
#define printline(n, s) do {lcd.setCursor(0, (n)); lcd.print((s));} while(0)

/**
 * Setup the Arduino Chip's feature for our use.
 *
 * After Arduino's kernel has booted initialize basic features for this
 * application, such as Serial port and MP3player objects with .begin.
 * Along with displaying the Help Menu.
 *
 * \note returned Error codes are typically passed up from MP3player.
 * Whicn in turns creates and initializes the SdCard objects.
 */ 
void setup()
{
  Serial.begin(9600);
  lcd.begin(LCD_COLS, LCD_ROWS);
  IR.enableIRIn();
  // initialise all switches as pullups
  for (int i = 0; i < NUM_SW; i++) {pinMode(sw_loc[i], INPUT_PULLUP);}

  // initialise the SdCard.
  // SD_SEL == CS pin value.
  // SPI_HALF_SPEED == SPISettings(F_CPU/4, MSBFIRST, SPI_MODE0);
  if (!sd.begin(SD_SEL, SPI_HALF_SPEED)) {sd.initErrorHalt();}
  if (!sd.chdir("/"))                    {sd.errorHalt("sd.chdir");}

#ifdef ARDUINO_AVR_MEGA2560
  Enc.write(0); // reset encoder position
#endif

  createPlaylist();
  
  // initialise the MP3 Player Shield, begin() will attempt to load patches.053  
  result = MP3player.begin();
  if (result != 0)
  {
    Serial.print(F("Error code: "));
    Serial.print(result);
    Serial.println(F(" when trying to start MP3 player"));
    if (result == 6) {Serial.println(F("Warning: patch file not found, skipping."));}
  }
  
  // default volume is too loud (higher 8-bit values means lower volume).
  MP3player.setVolume(80, 80);

  lcd.cursor();
  displayMenu();
}

/**
 * Handles input from the IR remote and front-panel switches.
 * 
 * Given enough free pins (eg, an Arudino Mega) additional input handling can be included 
 * by using the front-panel rotary encoder and more switches.
 * 
 * Favour input from the IR remote over the front panel switches.
 */
void loop()
{
  byte cur;

  cur = getSwitches();
  getRemote(&cur);
  if (cur != NO_VAL) {navMenu(cur);}
  delay(200); // slow down pushbutton reads
}

/*** INPUT ***/

/**
 * Initial handling of input for navigating the context menu is done here.
 * 
 * The context menu is the String called menu, and two of the indices at a time
 * are displayed in displayMenu(). Movement is handled by the keys defined UP, DOWN, LEFT and RIGHT,
 * with ENTER acting as a selection key for the currently highlighted option.
 * 
 * When using an IR remote, the choice can be selected just using the numerical keys as well.
 */
void navMenu(byte val)
{ 
  switch (val)
  {
    case 0: case 1: case 2: case 3: case 4:
    case 5: case 6: case 7: case 8:
      counter = val % MENU_SIZE;
      // let case statement fall through once
    case ENTER: // enter
      parseMenu(counter);
      break;
      
    // movement functionality
    case UP: case CH_UP:
      counter = counter ? counter - 1 : MENU_SIZE - 1;
      break;
      
    case LEFT:
      if (counter >= LCD_ROWS) {counter -= LCD_ROWS;}
      else                     {counter = MENU_SIZE - (LCD_ROWS - counter);}
      break;
      
    case RIGHT:
      counter = (counter + LCD_ROWS) % MENU_SIZE;
      break;
      
    case DOWN: case CH_DOWN:
      if (++counter == MENU_SIZE) {counter = 0;}
      break;
      
    default:
      counter = 0;
      break;
  }
  displayMenu();
}

void interruptFunction()
{
}

/**
 * Handles input on the default context menu.
 * 
 * counter is directly related to the index of each menu item in the String menu.
 */
void parseMenu(int count)
{
  int cnt = 0;
  byte decoded_val;
  static uint8_t recfn = 0;
  char filename[MAX_BUFF]; // 8.3 filename (12 chars) + '\0'

  lcd.noCursor();
  lcd.clear();
  
  switch(count)
  {
    case 0: // playlist
      play();
      break;
      
    case 1: // play track num
      lcd.print(F("Track num 1-999"));
      printline(LINE2, F("Enter to confirm"));
      playTrack();
      break;
      
    case 2: // record
      lcd.print(F("Enter: 00-99"));
      printline(LINE2, F("or press enter"));
      strcpy(filename, "record00.ogg");
      while (cnt != 2) // only add up to 2 letters
      {
        if (IR.decode(&IRcode) || (decoded_val = getSwitches()) != NO_VAL)
        {
          decoded_val = getcode();
          if (decoded_val >= 0 && decoded_val <= 9)
          {
            filename[6 + cnt++] = decoded_val + '0';
            printline(LINE1, filename);
          }
          else if (decoded_val == ENTER) // record in order if ENTER is pressed
          {
            filename[6] = (recfn / 10) + '0';
            filename[7] = (recfn % 10) + '0';
            recfn++;
            if (recfn == 100) {recfn = 0;}
            break;
          }

          delay(500);
          IR.resume();
        }
      }

      lcd.clear();
      lcd.print(F("Recording..."));
      result = record(filename);
      lcd.clear();
      if (!result)
      {
        lcd.print(F("Finished"));
        printline(LINE2, filename);
      }
      else
      {
        lcd.print(F("Can't record:"));

        lcd.setCursor(0, 1);
        if      (result == 1) {lcd.print(F("no plugin"));}
        else if (result == 2) {lcd.print(F("cannot make file"));}
        else                  {lcd.print(F("wrong filename"));}
      }

      delay(1000);

      createPlaylist();            // recreate playlist incase data is written to the SD card
      MP3player.vs_init();         // restart MP3 player after recording to prevent lockups during playback
      MP3player.setVolume(80, 80); // volume does need to be reset.

      break;
      
    case 3: // change to mono/stereo
      lcd.print(F("Mono Mode:"));
      lcd.setCursor(0, 1);
      if (MP3player.getMonoMode())
      {
        MP3player.setMonoMode(0);
        lcd.print(F("Disabled"));
      }
      else
      {
        MP3player.setMonoMode(1);
        lcd.print(F("Enabled"));
      }
      delay(1000);
      break;
      
    case 4: // reset VS1053
      lcd.print(F("Resetting VS1503"));
      delay(1000);
      MP3player.stopTrack();
      MP3player.vs_init();
      break;
      
    case 5: // sinewave test
      lcd.print(F("Sinewave test:"));
      test('t'); // enable
      while (true)
      {
        if (IR.decode(&IRcode) || getSwitches() != NO_VAL)
        {
          delay(200);
          IR.resume();
          break;
        }
      }
      test('t'); // disable
      delay(1000);
      break;
  
    case 6: // differential output
      lcd.print(F("Differ. Output:"));
      lcd.setCursor(0, 1);
      if (MP3player.getDifferentialOutput())
      {
        MP3player.setDifferentialOutput(0);
        lcd.print(F("Disabled"));
      }
      else
      {
        MP3player.setDifferentialOutput(1);
        lcd.print(F("Enabled"));
      }
      delay(1000);
      break;
      
    case 7: // turn chip off
      lcd.print(F("VS1053b off"));
      delay(1000);
      MP3player.end();
      break;
      
    case 8: // turn chip on
      lcd.print(F("VS1053b on"));
      delay(1000);
      MP3player.begin();
      break;

    default: // don't do anything for the rest
      break;
  }

  lcd.cursor(); // cursor back on
}

/*** PLAY FUNCTIONS ***/

/**
 * Handles playing of file indices (16-bits) in the array playlist, and also
 * handles user input related to playlist functionality.
 * Including going forwards/backwards, volume, etc.
 * 
 * Generic function name may cause issues with other libraries.
 */
void play()
{
  char filename[MAX_BUFF];
  bool skipped = true;   // if a song has been skipped.
  
  // reset playlist index before starting
  playnum = 0;
  while (playnum < playlen)
  {
    lcd.clear();
    lcd.print(F("P:"));
    
    // increment to the next file index if and only if
    // the song finished without skipping (LEFT/RIGHT pressed)
    if (!skipped) {playnum++;}
    skipped = false;
    
    if (file.open(sd.vwd(), playlist[playnum], O_READ))
    {
      if (!file.getFilename(filename))
      {
        file.close();
      }
      else
      {
        file.close();
        result = MP3player.playMP3(filename, 0);
        if (result)
        {
          lcd.print(F("Cannot play"));
          printline(LINE2, F("trying next song"));
          delay(1000);
          Serial.print(result);
          playnum++;
        }
        else // display file data and handle input
        {
          lcd.print(filename);
          skipped = play_commands();
        }
      }
    }
  }
}


/** 
 *  Music is playing so take input.
 *  
 *  Commands:
 *  UP/CH_UP -> skip to previous song
 *  DOWN/CH_DOWN -> skip to next song
 *  ENTER/PLAY -> pause/unpause song
 *  RESTART -> restart song 1s from start
 *  MUTE -> mute/unmute to original volume
 *  DEC_SPD, INC_SPD -> decrease/increase playspeed
 *  DEC_VOL, INC_VOL -> decrease/increase volume
 *  BACK -> exit playback
 */
bool play_commands()
{
  uint16_t playspeed;
  union twobyte mp3_vol; // helps deal with endian issues and individual byte access  
  static uint8_t old_vol = 0;  // storing the temporary volume value when muting
  char linebuff[LCD_COLS + 1];
  byte command;
  bool retval = false;;

  while (MP3player.isPlaying())
  {
    // display time playing only when not paused
    if (MP3player.getState() == playback)
    {
      if (snprintf(linebuff, LCD_COLS + 1, "Time: %lus", MP3player.currentPosition() / 1000))
      {
        printline(LINE2, linebuff);
      }
    }

#ifdef ARDUINO_AVR_MEGA2560 // if using an Arduino Mega, allow the encoder to be used
    static int e_lastpos = -1;
    int e_newpos;
    e_newpos = Enc.read();
    if (e_newpos != e_lastpos)
    {
      adjustVolume(e_newpos > e_lastpos);
      e_lastpos = e_newpos;
    }
#endif

    command = getSwitches();
    getRemote(&command);

    switch (command)
    {
      case RESTART: // restart song
        lcd.clear();
        lcd.print(F("Restarting..."));
        MP3player.stopTrack();
        retval = true;
        break;
        
      case CH_UP: case UP: // skip to previous song
        playnum ? playnum-- : playnum = playlen - 1;
        retval = true;
        MP3player.stopTrack();
        break;
        
      case CH_DOWN: case DOWN:// skip to next song
        playnum++;
        retval = true;
        MP3player.stopTrack();
        break;
        
      case PLAY: case ENTER: // pause
        lcd.setCursor(0, 1);
        if (MP3player.getState() == playback)
        {
          lcd.print(F("Pause           "));
          MP3player.pauseMusic();
        }
        else if (MP3player.getState() == paused_playback)
        {
          lcd.print(F("Resume          "));
          delay(200);
          MP3player.resumeMusic();
        }
        break;
        
      case DEC_SPD: case INC_SPD: // decrease/increase playspeed
        playspeed = MP3player.getPlaySpeed();
        if (command == INC_SPD)
        {
          if (playspeed >= 254) {playspeed = 5;}
          else                  {playspeed++;}
        }
        else if (playspeed)
        {
          playspeed--;
        }
        MP3player.setPlaySpeed(playspeed);
        break;

      case DEC_VOL: case INC_VOL: // decrease/increase volume
        adjustVolume(command == INC_VOL);
        break;

      case MUTE: // mute/unmute playback volume
        mp3_vol.word = MP3player.getVolume();
        if (mp3_vol.byte[1] < 254)
        {
          old_vol = mp3_vol.byte[1];
          MP3player.setVolume(254, 254);
        }
        else if (old_vol >= 2) // restore original volume
        {
          MP3player.setVolume(old_vol, old_vol);
          old_vol = 0;
        }
        break;
        
      case BACK: // exit playback
        MP3player.stopTrack();
        playnum = playlen; // end outer loop
        counter = 0;
        break;

      default: // do nothing
        break;
    }
  }

  return retval;
}

/**
 * Adjust volume for the MP3 player.
 */
void adjustVolume(bool increase)
{
  // helps deal with endian issues and individual byte access, at least every variable isn't global by default
  union twobyte mp3_vol;
  static char soundlevel[LCD_COLS + 1];
  mp3_vol.word = MP3player.getVolume();

  // note dB is negative (higher byte values == lower volume)
  if (increase)
  {
    // assume equal balance and use byte[1] for math, plus keep it to whole dB's
    if (mp3_vol.byte[1] <= 2) {mp3_vol.byte[1] = 2;}
    else                      {mp3_vol.byte[1] -= 2;}
  }
  else
  {
    if (mp3_vol.byte[1] >= 254) {mp3_vol.byte[1] = 254;}
    else                        {mp3_vol.byte[1] += 2;}
  }
  
  // push byte[1] into both left and right assuming equal balance.
  MP3player.setVolume(mp3_vol.byte[1], mp3_vol.byte[1]);

  // print volume output, uncomment if you want
  /*
  int temp = map(mp3_vol.byte[1], 0, 255, 0, LCD_COLS);
  // print a volume indication.  
  for (int i = 0; i < temp; i++)        {soundlevel[LCD_COLS - i - 1] = ' ';}
  for (int j = temp; j < LCD_COLS; j++) {soundlevel[j - temp] = '|';}
  soundlevel[LCD_COLS] = '\0';
  printline(LINE2, soundlevel);
  delay(100);
  printline(LINE2, F("                "));
  */
}

/**
 * Handles collecting user input to form an integer between 1 - 999 which is then
 * used to create a formatted string ("trackXXX.ext", where ext is a given file extension.
 * 
 * A track will keep playing until eiter it stops itself or the user prompts it to stop
 * by pressing any key.
 * 
 * Note that it can take time for the buffer to be flushed once a file has stopped playing.
 * This means that opening the same file again may not work if you attempt to open it again straight away.
 * All you can do is wait a few seconds before opening it again.
 */
void playTrack()
{
  char filetype[4] = "mp3"; // default filetype is mp3
  char trackname[MAX_BUFF];
  byte decoded_val;
  int cnt = 0, tracknum = 0;

  // collect input (blocking).
  lcd.clear();
  lcd.print(F("track"));
  while (cnt < 3)
  {
    decoded_val = getSwitches();
    getRemote(&decoded_val);
    
    if (decoded_val >= 0 && decoded_val <= 9)
    {
      if (tracknum > 99) {cnt = 3;}
      else               {tracknum  = tracknum * 10 + decoded_val; cnt++;}
      lcd.setCursor(5, 0);
      lcd.print(tracknum);
    }
    else if (decoded_val == DOWN || decoded_val == RIGHT)
    {
      tracknum = tracknum > 999 ? 0 : tracknum + 1;
      lcd.setCursor(5, 0);
      lcd.print(tracknum);
    }
    else if (decoded_val == ENTER)
    {
      break;
    }
  }
  
  if (!tracknum) {tracknum = 1;}

  // play .ogg if RIGHT key pressed, otherwise default to play as .mp3
  printline(LINE2, F("MP3(L) or OGG(R)?"));
  // wait for input (blocking), doesn't like using IR.decode() in a loop
  while (true)
  {
    if (IR.decode(&IRcode) || (decoded_val = getSwitches()) != NO_VAL)
    {
      delay(200);
      IR.resume();
      break;
    }
  }
  decoded_val = getcode();
  if (decoded_val == RIGHT || decoded_val == DOWN) {strcpy(filetype, "ogg");}
  snprintf(trackname, MAX_BUFF, "track%03d.%s", tracknum, filetype);

  result = MP3player.playMP3(trackname, 0);
  if (result)
  {
    // debug info
    lcd.clear();
    if (result == 2)      {lcd.print(trackname); printline(LINE2, F("not found"));}
    else if (result == 3) {lcd.print(F("player in reset"));}
    Serial.print(F("Error code "));
    Serial.print(result);
    Serial.println();
  }
  else
  {
    lcd.clear();
    lcd.print(F("P: "));
    lcd.print(trackname);

    // play until any user input, or the song finishes
    while (MP3player.isPlaying())
    {
      if (IR.decode(&IRcode) || getSwitches() != NO_VAL)
      {
        MP3player.stopTrack();
        delay(200);
        IR.resume();
      }
    }
    
    printline(LINE2, F("Finished playing"));
  }
    
  delay(1000);
}

/**
 * Builds the playlist used in the play() function, and should be called whenever
 * the data on the SD card changes or during program start.
 */
void createPlaylist()
{
  // store the unique index for every mp3 file in the SD card.
  char filename[MAX_BUFF];
  
  // make sure length and array are zeroed before starting
  playlen = 0;
  memset(playlist, 0, sizeof(playlist));
  sd.vwd()->rewind();
  
  while (playlen < MAX_INDEX && file.openNext(sd.vwd(), O_READ))
  {
    if (file.getFilename(filename))
    {
      // check for mp3/aac/wma/wav/fla/mid/ogg substring.
      if (isFnMusic(filename))
      {
        // store 16-bit unsigned index.
        playlist[playlen++] = (sd.vwd()->curPosition() / 32) - 1;
      }
    }
    file.close();
  }
}

/*** RECORDING ***/

/**
 * Implements recording for the VS1053 according to the VorbisEncoder170c.pdf manual
 * The recorded files are saved in OGG format on an SD card.
 * Note that it primarily records using the on-board microphone, but line-input
 * can be enabled by writing SM_LINE1 to SCI_MODE as in the manual, or defining
 * a macro called USE_LINEIN
 */
uint8_t record(char *filename)
{
  uint8_t wordsToWrite;
  uint16_t data, wordsToRead, wordsWaiting;
  uint32_t timer;
  byte wbuff[256]; // we write at most 256 bytes at a time twice

  // make sure state begins at 0
  rec_state = 0;

  /* 1. set VS1053 clock to 4.5x = 55.3 MHz; our board uses a 12.288MHz crystal */
  MP3player.Mp3WriteRegister(SCI_CLOCKF, 0xC000);
  timer = millis();
  while (!digitalRead(MP3_DREQ) || millis() - timer > 100UL) {;}
  
  /* 2. clear SCI_BASS */
  MP3player.Mp3WriteRegister(SCI_BASS, 0);

  /* 3. reset VS1053 */
  MP3player.Mp3WriteRegister(SCI_MODE, (MP3player.Mp3ReadRegister(SCI_MODE) | SM_RESET));
  // Wait until DREQ is high or 100ms
  timer = millis();
  while (!digitalRead(MP3_DREQ) || millis() - timer > 100UL) {;}

  /* 4. disable all interrupts except SCI */
  MP3player.Mp3WriteRegister(SCI_WRAMADDR, VS1053_INT_ENABLE);
  MP3player.Mp3WriteRegister(SCI_WRAM, 0x2);

  /* 5. need to load plugin for recording (exported as oggenc.053, 
        original filename is venc44k2q05.plg but it's too large for an SD card) */
  if (MP3player.VSLoadUserCode("oggenc.053"))
  {
    Serial.println(F("Could not load plugin"));
    return 1;
  }

  // load file for recording
  if (filename)
  {
    if (!file.open(filename, O_RDWR | O_CREAT))
    {
      Serial.println(F("Could not open file"));
      return 2;
    }
  }
  else
  {
    Serial.println(F("No filename given"));
    return 3;
  }

  /* 6. Set VS1053 mode bits as needed. If USE_LINEIN is set
        then the line input will be used over the on-board microphone */
  #ifdef USE_LINEIN
    MP3player.Mp3WriteRegister(SCI_MODE, SM_LINE1 | SM_ADPCM | SM_SDINEW);
  #else
    MP3player.Mp3WriteRegister(SCI_MODE, SM_ADPCM | SM_SDINEW);
  #endif

  /* 7. Set recording levels on control registers SCI_AICTRL1/2 */
  // Rec level: 1024 = 1. If 0, use AGC.
  MP3player.Mp3WriteRegister(SCI_AICTRL1, 1024);
  // Maximum AGC level: 1024 = 1. Only used if SCI_AICTRL1 is set to 0.
  MP3player.Mp3WriteRegister(SCI_AICTRL2, 0);
  
  /* 8. no VU meter to set */
   
  /* 9. set a value to SCI_AICTRL3, in this case 0 */
  MP3player.Mp3WriteRegister(SCI_AICTRL3, 0);

  /* 10. no profile to set for VOX */

  /* 11. Active encoder by writing 0x34 to SCI_AIADDR for Ogg Vorbis */
  MP3player.Mp3WriteRegister(SCI_AIADDR, 0x34);

  /* 12. wait until DREQ pin is high before reading data */
  timer = millis();
  while (!digitalRead(MP3_DREQ) || millis() - timer > 100UL) {;}

  /**
   * Handles recording data:
   * state == 0 -> normal recording
   * state == 1 -> user and micro requested end of recording
   * state == 2 -> stopped recording, but data still being collected
   * state == 3 -> recoding finished
   */
  timer = millis(); // start timing.
  while (rec_state < 3)
  {
    printline(LINE2, (millis() - timer) / 1000);
    
    // detect if any signal is sent and stop recording
    if ((IR.decode(&IRcode) || getSwitches() != NO_VAL) && !rec_state)
    {
      rec_state = 1;
      MP3player.Mp3WriteRegister(SCI_AICTRL3, 1);
      delay(200);
      IR.resume();
    }

    // see how many 16-bit words there are waiting in the VS1053 buffer
    wordsWaiting = MP3player.Mp3ReadRegister(SCI_HDAT1);

    // if user has requested and VS1053 has stopped recording increment state to 2
    if (rec_state == 1 && MP3player.Mp3ReadRegister(SCI_AICTRL3) & (1 << 1))
    {
      rec_state = 2;
      // reread the HDAT1 register to make sure there are no extra words left.
      wordsWaiting = MP3player.Mp3ReadRegister(SCI_HDAT1);
    }

    // read and write 512-byte blocks. Except for when recording ends, then write a smaller block.
    while (wordsWaiting >= ((rec_state < 2) ? 256 : 1))
    {
      wordsToRead = min(wordsWaiting, 256);
      wordsWaiting -= wordsToRead;

      // if this is the last block, read one 16-bit value less as it is handled separately
      if (rec_state == 2 && !wordsWaiting) {wordsToRead--;}

      wordsToWrite = wordsToRead / 2;

      // transfer the 512-byte block in two groups of 256-bytes due to memory limitations,
      // except if it's the last block to transfer, then transfer all data except for the last 16-bits
      for (int i = 0; i < 2; i++)
      {
        for (uint8_t j = 0; j < wordsToWrite; j++)
        {
          data = MP3player.Mp3ReadRegister(SCI_HDAT0);
          wbuff[2 * j] = data >> 8;
          wbuff[2 * j + 1] = data & 0xFF;
        }
        file.write(wbuff, 2 * wordsToWrite);
      }
            
      // if last data block
      if (wordsToRead < 256)
      {
        rec_state = 3;

        // read the very last word of the file
        data = MP3player.Mp3ReadRegister(SCI_HDAT0);

        // always write first half of the last word
        file.write(data >> 8);

        // read SCI_AICTRL3 twice, then check bit 2 of the latter read
        MP3player.Mp3ReadRegister(SCI_AICTRL3);
        if (!(MP3player.Mp3ReadRegister(SCI_AICTRL3) & (1 << 2)))
        {
          // write last half of the last word only if bit 2 is clear
          file.write(data & 0xFF);
        }
      }
    }
  }

  // done, now close file
  file.close();

  MP3player.Mp3WriteRegister(SCI_MODE, (MP3player.Mp3ReadRegister(SCI_MODE) | SM_RESET));

  return 0;
}

/*** MISC ***/

/**
 * Tests both a sinewave and the chip's memory depending on the mode passed.
 * mode == 't' -> sinewave test
 * mode == 'm' -> memory test
 */
void test(byte mode) {
  int8_t state;
  state = mode == 't' ? MP3player.enableTestSineWave(126) :
          mode == 'm' ? MP3player.memoryTest()            :
          0                                               ;
          
  if (state == -1)
  {
    Serial.println(F("Unavailable while playing music or chip in reset."));
  }
  else if (state == 1 && mode == 't')
  {
    printline(LINE2, F("Enabled"));
  }
  else if (state == 2)
  {
    MP3player.disableTestSineWave();
    printline(LINE2, F("Disabled"));
  }
  else if (mode == 'm')
  {
    Serial.print(F("Memory test results = "));
    Serial.println(state, HEX);
    Serial.println(F("Result should be 0x83FF."));
    Serial.println(F("Reset is needed to recover to normal operation"));
  }
}

/**
 * Displays the char array menu on the LCD screen and handles scrolling.
 */
void displayMenu()
{
  lcd.clear();
  // strcyp_P so that menu items can be stored in flash
  strcpy_P(menubuff, menu[counter]);
  lcd.print(menubuff);
  strcpy_P(menubuff, menu[(counter + 1) % MENU_SIZE]);
  lcd.setCursor(0, 1);
  lcd.print(menubuff);

  lcd.setCursor(0, 0);
}

/**
 * Check whether one of the front panel switches was pressed.
 * Returns the value of the first switch that was pressed.
 */
byte getSwitches()
{
  for (int i = 0; i < NUM_SW; i++)
  {
    if (digitalRead(sw_loc[i]) == LOW)
    {
      return sw_val[i];
    }
  }

  return NO_VAL;
}

/**
 * Decodes a value from an IR remote.
 * Pass by reference.
 */
byte getRemote(byte *val)
{
  if (IR.decode(&IRcode))
  {
    *val = getcode();
    delay(200); // debounce time
    IR.resume();
  }
}

