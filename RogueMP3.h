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

#ifndef _ROGUEMP3_H
#define _ROGUEMP3_H

// RogueMP3 version
// @Version 2.0.0
#define ROGUEMP3_VERSION                      10000

#include <avr/pgmspace.h>
#include <stdint.h>
#include <Stream.h>
#include <Print.h>

/*
|| Public Constants
*/

#ifndef _ROGUESD_H

#define DEFAULT_PROMPT                        0x3E

#define ERROR_BUFFER_OVERRUN                  0x02
#define ERROR_NO_FREE_FILES                   0x03
#define ERROR_UNRECOGNIZED_COMMAND            0x04
#define ERROR_CARD_INITIALIZATION_ERROR       0x05
#define ERROR_FORMATTING_ERROR                0x06
#define ERROR_EOF                             0x07
#define ERROR_CARD_NOT_INSERTED               0x08
#define ERROR_MMC_RESET_FAIL                  0x09
#define ERROR_CARD_WRITE_PROTECTED            0x0a
#define ERROR_INVALID_HANDLE                  0xf6
#define ERROR_OPEN_PATH_INVALID               0xf5
#define ERROR_FILE_ALREADY_EXISTS             0xf4
#define ERROR_DE_CREATION_FAILURE             0xf3
#define ERROR_FILE_DOES_NOT_EXIST             0xf2
#define ERROR_OPEN_HANDLE_IN_USE              0xf1
#define ERROR_OPEN_NO_FREE_HANDLES            0xf0
#define ERROR_FAT_FAILURE                     0xef
#define ERROR_SEEK_NOT_OPEN                   0xee
#define ERROR_OPEN_MODE_INVALID               0xed
#define ERROR_READ_IMPROPER_MODE              0xec
#define ERROR_FILE_NOT_OPEN                   0xeb
#define ERROR_NO_FREE_SPACE                   0xea
#define ERROR_WRITE_IMPROPER_MODE             0xe9
#define ERROR_WRITE_FAILURE                   0xe8
#define ERROR_NOT_A_FILE                      0xe7
#define ERROR_OPEN_READONLY_FILE              0xe6
#define ERROR_NOT_A_DIR                       0xe5

#define ERROR_NOT_SUPPORTED                   0xff

#endif

/*
|| Typedefs, structs, etc
*/

struct playbackInfo {
                      uint16_t position;
                      uint8_t samplerate;
                      uint16_t bitrate;
                      char channels;
                    };

#ifndef _ROGUESD_H
enum moduleType {
                  uMMC = 1,
                  uMP3,
                  rMP3
                };
#endif

/*
|| Class
*/

class RogueMP3 : public Print
{
  public:
    // properties
    uint8_t LastErrorCode;
    moduleType getModuleType(void) { return _moduleType; }
    inline int16_t version(void) { return _fwVersion; }

    // methods
    RogueMP3(Stream &comms);            // constructor

    int8_t begin(bool blocking = false) { return sync(blocking); }
    int8_t sync(bool blocking = false);

    // Play Command ("PC") methods
    int8_t playFile_P(const char *path);
    int8_t playFile(const char *path, const char *filename = NULL, uint8_t pgmspc = 0);
    void setLoop(uint8_t loopcount);
    void jump(uint16_t newtime);
    void setBoost(uint8_t bass_amp, uint8_t bass_freq, int8_t treble_amp, uint8_t treble_freq);
    void setBoost(uint16_t newboost);

    uint16_t getVolume(void);

    void setVolume(uint8_t newVolume);
    void setVolume(uint8_t new_vLeft, uint8_t new_vRight);

    void fade(uint8_t newVolume);
    void fade(uint8_t newVolume, uint16_t fadems);
    void fadeLeftRight(uint8_t new_vLeft, uint8_t new_vRight);
    void fadeLeftRight(uint8_t new_vLeft, uint8_t new_vRight, uint16_t fadems);

    void playPause(void);
    void stop(void);
    
    playbackInfo getPlaybackInfo(void);
    char getPlaybackStatus(void);
    uint8_t getSpectrumAnalyzerValues(uint8_t values[], uint8_t peaks = 0);
    void setSpectrumAnalyzerBands(uint16_t bands[], uint8_t count);

    // Information Commands ("IC" - MP3 information)
    int16_t getTrackLength(const char *path, const char *filename = NULL, uint8_t pgmspc = 0);

    // Settings ("ST") methods
    //int8_t changesetting(char setting, const char *value);
    int8_t changeSetting(char setting, uint8_t value);
    int16_t getSetting(char setting);
    // ***************************

#if ARDUINO >= 100
    size_t write(uint8_t);
#else
    void write(uint8_t);  // needed for Print
#endif
    void print_P(const char *str);

  private:

    // Polymorphism used to interact with serial class
    // SerialBase is an abstract base class which defines a base set
    // of functionality for serial classes.
    Stream *_comms;

    uint8_t _promptChar;
    int16_t _fwVersion;
    moduleType _moduleType;
    
    // methods
    int16_t _getVersion(void);
    int8_t _getResponse(void);

    int8_t _readBlocked(void);
    int16_t _readTimeout(uint16_t timeout);  // Time out = timeout * 0.01 seconds
    int32_t _getNumber(uint8_t base);

    uint8_t _commAvailable(void);
    int _commPeek(void);
    int _commRead(void);
};

#endif
