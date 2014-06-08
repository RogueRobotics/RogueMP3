/*
||
|| @author         Brett Hagman <bhagman@wiring.org.co>
|| @url            http://wiring.org.co/
|| @url            http://roguerobotics.com/
||
|| @description
|| | Rogue Robotics MP3 Module Library
|| |
|| | This Wiring and Arduino Library works with the following
|| | Rogue Robotics modules:
|| |   - uMP3 (Industrial MP3 Playback Module)
|| |   - rMP3 (Commercial MP3 Playback Module)
|| |
|| | Requires:
|| | uMP3 firmware > 111.01
|| | rMP3 firmware > 100.00
|| |
|| | See http://www.roguerobotics.com/faq/update_firmware for updating firmware.
|| #
||
|| @license Please see LICENSE.txt for this project.
||
*/

#include <stdint.h>
#include <ctype.h>

#if WIRING
 #include <Wiring.h>
#elif ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif

#include "RogueMP3.h"

/*
|| Private Constants
*/

#define UMP3_MIN_FW_VERSION_FOR_NEW_COMMANDS 11101

#define FADE_STEPS                      40
#define FADE_AUDIBLE_DIFF               5
#define FADE_DEFAULT_TIME               1000

#define ASCII_ESC                       0x1b

// Default to 1 second.
#define ROGUEMP3_DEFAULT_READ_TIMEOUT   100

/*
|| Constructor
*/

RogueMP3::RogueMP3(Stream &comms)
: LastErrorCode(0),
  _promptChar(DEFAULT_PROMPT),
  _fwVersion(0),
  _moduleType(rMP3)
{
  _comms = &comms;
}


/*
|| Public Methods
*/

int8_t RogueMP3::sync(bool blocking)
{
  // procedure:
  // 1. sync (send ESC, clear prompt)
  // 2. get version ("v"), and module type
  // 3. get prompt ("st p"), if newer firmware
  // 4. check status

  // 0. empty any data in the serial buffer
  _commFlush();

  // 1. sync
  print((char)ASCII_ESC);               // send ESC to clear buffer on uMMC
  if (blocking)
  {
    _readBlocked();                     // consume prompt
  }
  else
  {
    if (_readTimeout(ROGUEMP3_DEFAULT_READ_TIMEOUT) < 0)
    {
      return -1;  // TODO: match return values (0 = bad, !0 = good?)
    }
  }

  // 2. get version (ignore prompt - just drop it)
  _getVersion();

  // 3. get prompt ("st p"), if newer firmware
  if (_moduleType == rMP3 || (_moduleType == uMP3 && _fwVersion >= UMP3_MIN_FW_VERSION_FOR_NEW_COMMANDS))
  {
    // get the prompt char
    print('S');
    if (_moduleType != uMMC) { print('T'); };
    print('P'); print('\r');  // get our prompt (if possible)
    _promptChar = _getNumber(10);
    _readBlocked();                    // consume prompt
  }

  // 4. check status

  print('F'); print('C'); print('Z'); print('\r'); // Get status

  if (_getResponse())
    return -1;
  else
  {
    // good
    _readBlocked();                    // consume prompt

    return 0;
  }
}


int8_t RogueMP3::changeSetting(char setting, uint8_t value)
{
  print('S'); print('T'); print(setting); print(value, DEC); print('\r');

  return _getResponse();
}


// int8_t RogueMP3::changeSetting(char setting, const char* value)
// {
  // print('S'); print('T'); print(setting); print(value); print('\r');

  // return _getResponse();
// }

int16_t RogueMP3::getSetting(char setting)
{
  uint8_t value;

  print('S'); print('T'); print(setting); print('\r');

  while (!_commAvailable());
  if (_commPeek() != 'E')
  {
    value = _getNumber(10);
    _readBlocked();                    // consume prompt
  }
  else
  {
    value = _getResponse();            // get the error
  }

  return value;
}

int8_t RogueMP3::playFile_P(const char *path)
{
  return playFile(path, NULL, 1);
}


int8_t RogueMP3::playFile(const char *path, const char *filename, uint8_t pgmspc)
{
  print("PCF");

  if (pgmspc == 1)
    print_P(path);
  else
    print(path);

  if (filename)
  {
    if (path && path[strlen(path) - 1] != '/')
      print('/');
    print(filename);
  }
  print('\r');

  return _getResponse();
}


uint16_t RogueMP3::getVolume(void)
{
  uint16_t l, r;

  print("PCV\r");

  l = _getNumber(10);
  _readBlocked();                      // consume separator
  r = _getNumber(10);

  _readBlocked();                      // consume prompt

  return l << 8 | r;
}


void RogueMP3::setVolume(uint8_t newvolume)
{
  print("PCV");
  print(newvolume, DEC);
  print('\r');

  _readBlocked();                      // consume prompt
}


void RogueMP3::setVolume(uint8_t new_vleft, uint8_t new_vright)
{
  print("PCV");
  print(new_vleft, DEC);
  print(' ');
  print(new_vright, DEC);
  print('\r');

  _readBlocked();                      // consume prompt
}



void RogueMP3::fade(uint8_t newVolume)
{
  fadeLeftRight(newVolume, newVolume, FADE_DEFAULT_TIME);
}



void RogueMP3::fade(uint8_t newVolume, uint16_t fadems)
{
  fadeLeftRight(newVolume, newVolume, fadems);
}



void RogueMP3::fadeLeftRight(uint8_t new_vLeft, uint8_t new_vRight)
{
  fadeLeftRight(new_vLeft, new_vRight, FADE_DEFAULT_TIME);
}



void RogueMP3::fadeLeftRight(uint8_t new_vLeft, uint8_t new_vRight, uint16_t fadems)
{
  // fades either/both channels to new volume in fadems milliseconds
  // always 20 steps
  uint16_t vleft, vright;
  uint16_t currentvolume;
  uint16_t fadetimestep = 0;
  int16_t il, ir;
  int8_t i;

  fadetimestep = fadems/FADE_STEPS;

  if (fadetimestep<FADE_AUDIBLE_DIFF)
  {
    // too fast to hear - just set the volume
    setVolume(new_vLeft, new_vRight);
  }
  else
  {
    currentvolume = getVolume();
    // for precision, we move the volume over by 4 bits
    vleft = ((currentvolume >> 8) & 0xff) * 16;
    vright = (currentvolume & 0xff) * 16;

    il = (((uint16_t)new_vLeft)*16 - vleft);
    ir = (((uint16_t)new_vRight)*16 - vright);

    il /= FADE_STEPS;
    ir /= FADE_STEPS;

    for (i = 0; i < FADE_STEPS; i++)
    {
      vleft += il;
      vright += ir;
      setVolume(vleft/16, vright/16);
      delay(fadetimestep);
    }
  }
}



void RogueMP3::playPause(void)
{
  print("PCP\r");

  _readBlocked();                      // consume prompt
}



void RogueMP3::stop(void)
{
  print("PCS\r");

  _readBlocked();                      // consume prompt
}



playbackInfo RogueMP3::getPlaybackInfo(void)
{
  playbackInfo pi;

  print("PCI\r");
  // now get the info we need

  // first, time
  pi.position = _getNumber(10);
  _readBlocked();                      // consume separator

  // second, samplerate
  pi.samplerate = _getNumber(10);
  _readBlocked();                      // consume separator

  // third, bitrate
  pi.bitrate = _getNumber(10);
  _readBlocked();                      // consume separator

  // fourth, channels
  pi.channels = _readBlocked();

  _readBlocked();                      // consume prompt

  return pi;
}


char RogueMP3::getPlaybackStatus(void)
{
  char value;

  print("PCZ\r");

  value = _readBlocked();

  while (_readBlocked() != _promptChar);

  return value;
}


void RogueMP3::jump(uint16_t newtime)
{
  print("PCJ");
  print(newtime, DEC);
  print('\r');

  _readBlocked();                      // consume prompt
}


void RogueMP3::setBoost(uint8_t bass_amp, uint8_t bass_freq, int8_t treble_amp, uint8_t treble_freq)
{
  uint16_t newBoostRegister = 0;

  if (treble_freq > 15) treble_freq = 15;
  if (treble_amp < -8) treble_amp = -8;
  else if (treble_amp > 7) treble_amp = 7;
  if (bass_freq == 1) bass_freq = 0;
  else if (bass_freq > 15) bass_freq = 15;
  if (bass_amp > 15) bass_amp = 15;

  newBoostRegister = (uint8_t)treble_amp << 12;
  newBoostRegister |= treble_freq << 8;
  newBoostRegister |= bass_amp << 4;
  newBoostRegister |= bass_freq;

  setBoost(newBoostRegister);
}

void RogueMP3::setBoost(uint16_t newboost)
{
  print("PCB");
  print(newboost, DEC);
  print('\r');

  _readBlocked();                      // consume prompt
}


void RogueMP3::setLoop(uint8_t loopcount)
{
  print("PCO");
  print(loopcount, DEC);
  print('\r');

  _readBlocked();                      // consume prompt
}


// Added for sending PROGMEM strings
void RogueMP3::print_P(const char *str)
{
  while (pgm_read_byte(str) != 0)
  {
    print((char)pgm_read_byte(str++));
  }
}



uint8_t RogueMP3::getSpectrumAnalyzerValues(uint8_t values[], uint8_t peaks)
{
  uint8_t count = 0;
  uint8_t value = 0;
  uint8_t ch;

  print("PCY");
  if (peaks)
    print('P');
  print('\r');

  // now get the info we need

  ch = _readBlocked();  // start it off (ch should be a space)

  while (ch == ' ')
  {
    value = _getNumber(10);
    values[count++] = value;
    ch = _readBlocked();
  }

  return count;
}


void RogueMP3::setSpectrumAnalyzerBands(uint16_t bands[], uint8_t count)
{
  uint8_t i;

  if (count == 0)
    return;

  if (count > 23)
    count = 23;

  print("PCYS");

  // now send the band frequencies

  for (i = 0; i < count; i++)
  {
    print(' ');
    print(bands[i], DEC);
  }

  print('\r');

  _readBlocked();                      // consume prompt
}


int16_t RogueMP3::getTrackLength(const char *path, const char *filename, uint8_t pgmspc)
{
  int16_t tracklength = 0;

  print("ICT");

  if (pgmspc == 1)
    print_P(path);
  else
    print(path);

  if (filename)
  {
    print('/');
    print(filename);
  }

  print('\r');

  if(_getResponse() == 0)
  {
    tracklength = _getNumber(10);

    _readBlocked();                 // consume prompt

    return tracklength;
  }
  else
  {
    // error occurred with "IC" command
    return -1;
  }
}



/*
|| Public (virtual)
*/

#if ARDUINO >= 100

size_t RogueMP3::write(uint8_t c)
{
  _comms->write(c);
  return 1;
}

#else

void RogueMP3::write(uint8_t c)
{
  _comms->write(c);
}

#endif


/*
|| Private Methods
*/

int8_t RogueMP3::_readBlocked(void)
{
  // int8_t r;

  while (!_commAvailable());
  // while((r = this->_readf()) < 0);   // this would be faster if we could guarantee that the _readf() function
                                        // would return -1 if there was no byte read
  return _commRead();
}


int16_t RogueMP3::_readTimeout(uint16_t timeout)
{
  while (timeout)
  {
    if (_commAvailable())
      return (uint8_t) _commRead();

    timeout--;
    delay(10);
  }

  return -1;
}


int8_t RogueMP3::_getResponse(void)
{
  // looking for a response
  // If we get a space " ", we return as good and the remaining data can be retrieved
  // " ", ">", "Exx>" types only
  uint8_t r;
  uint8_t resp = 0;

  // we will return 0 if all is good, error code otherwise

  r = _readBlocked();

  if (r == ' ' || r == _promptChar)
    resp = 0;

  else if (r == 'E')
  {
    LastErrorCode = _getNumber(16);     // get our error code
    _readBlocked();                    // consume prompt

    resp = -1;
  }

  else
  {
    LastErrorCode = 0xFF;               // something got messed up, a resync would be nice
    resp = -1;
  }

  return resp;
}


int16_t RogueMP3::_getVersion(void)
{
  // get the version, and module type
  print('V'); print('\r');

  // Version format: mmm.nn[-bxxx] SN:TTTT-ssss...

  // get first portion mmm.nn
  _fwVersion = _getNumber(10);
  _readBlocked();                      // consume '.'
  _fwVersion *= 100;
  _fwVersion += _getNumber(10);
  // ignore beta version (-bxxx), if it's there
  if (_readBlocked() == '-')
  {
    for (char i = 0; i < 4; i++)
      _readBlocked();
  }
  // otherwise, it was a space

  // now drop the SN:
  _readBlocked();
  _readBlocked();
  _readBlocked();

  if (_readBlocked() == 'R')
    _moduleType = rMP3;
  else
  {
    // either UMM1 or UMP1
    // so drop the M following the U
    _readBlocked();
    if (_readBlocked() == 'M')
      _moduleType = uMMC;
    else
      _moduleType = uMP3;
  }

  // ignore the rest
  while (_readBlocked() != '-');

  // consume up to and including prompt
  while (isalnum(_readBlocked()));

  return _fwVersion;
}


int32_t RogueMP3::_getNumber(uint8_t base)
{
  uint8_t c, neg = 0;
  uint32_t val;

  val = 0;
  while (!_commAvailable());
  c = _commPeek();

  if(c == '-')
  {
    neg = 1;
    _commRead();  // remove
    while (!_commAvailable());
    c = _commPeek();
  }

  while (((c >= 'A') && (c <= 'Z'))
      || ((c >= 'a') && (c <= 'z'))
      || ((c >= '0') && (c <= '9')))
  {
    if (c >= 'a') c -= 0x57;             // c = c - 'a' + 0x0a, c = c - ('a' - 0x0a)
    else if (c >= 'A') c -= 0x37;        // c = c - 'A' + 0x0A
    else c -= '0';
    if (c >= base) break;

    val *= base;
    val += c;
    _commRead();                     // take the byte from the queue
    while (!_commAvailable());        // wait for the next byte
    c = _commPeek();
  }
  return neg ? -val : val;
}


uint8_t RogueMP3::_commAvailable(void)
{
  return _comms->available();
}

int RogueMP3::_commPeek(void)
{
  return _comms->peek();
}

int RogueMP3::_commRead(void)
{
  return _comms->read();
}

void RogueMP3::_commWrite(uint8_t c)
{
  _comms->write(c);
}

void RogueMP3::_commFlush(void)
{
  _comms->flush();
}
