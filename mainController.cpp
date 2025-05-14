/**********************************************************************
* Filename    : mainController.cpp
* Description : Main controller of the system---control music, display file, etc.
* Author      : Tony Zhang
* Date        : 2024/11/30
* Assignment  : Final Project
* Version     : 1.1

Credits to FreeNove, as this program modifies the MatrixKeyboard and I2CLCD1602 code they provide.
**********************************************************************/
#include "Keypad.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <pcf8574.h>
#include <lcd.h>
#include <vector>
#include <SFML/Audio.hpp>
#include <iostream>
#include <experimental/filesystem>
#include <string>
#include <iostream>

using namespace std;
namespace fs = std::experimental::filesystem;

// Matrix Keyboard Setup
const byte ROWS = 4;                // four rows
const byte COLS = 4;                // four columns
char keys[ROWS][COLS] = {           // key code
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {1, 4, 5, 6 }; // define the row pins for the keypad
byte colPins[COLS] = {12,3, 2, 0 }; // define the column pins for the keypad
//create Keypad object
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// I2CLCD1602 Setup
int pcf8574_address = 0x27;         // PCF8574T:0x27, PCF8574AT:0x3F
#define BASE 64                     // BASE any number above 64
//Define the output pins of the PCF8574, which are directly connected to the LCD1602 pin.
#define RS      BASE+0
#define RW      BASE+1
#define EN      BASE+2
#define LED     BASE+3
#define D4      BASE+4
#define D5      BASE+5
#define D6      BASE+6
#define D7      BASE+7

int lcdhd;                          // used to handle LCD

int detectI2C(int addr);
string truncateTextToLCD(string str);
void printSongLCD(string song);
void printDurationLCD(sf::Music&);
void changeSong(int& itr, vector<string> playlist, sf::Music&);

int main(){
    // initialize variables
    vector<string> playlist;                // playlist to hold paths to .wav files
    string path = "./playlist";             // path to the playlist directory
	char key = 0;                           // char holding the latest pressed key from MatrixKeypad
    int songIterator = 0;                   // iterator for current song
    sf::Music music;                        // music object
    bool isPaused = false;                  // bool used to pause/unpause audio

    // setup playlist vector
    for(const auto & entry : fs::directory_iterator(path)) {
        playlist.push_back(move(entry.path()));
    }

    // setup wiringPi
    wiringPiSetup();
    
    // time between key presses
	keypad.setDebounceTime(50);

    // detect I2C && setup I2CLCD1602
    if(detectI2C(0x27)) {
       pcf8574_address = 0x27;
    } else if(detectI2C(0x3F)) {
        pcf8574_address = 0x3F;
    } else {
        cout << "No correct I2C address found, \n"
        << "Please use command 'i2cdetect -y 1' to check the I2C address! \n"
        << "Program Exit." << endl;
        return -1;
    }

    pcf8574Setup(BASE,pcf8574_address);     // initialize PCF8574

    for(int i=0;i<8;i++){
        pinMode(BASE+i,OUTPUT);             // set PCF8574 port to output mode
    } 

    digitalWrite(LED,HIGH);                 // turn on LCD backlight
    digitalWrite(RW,LOW);                   // allow writing to LCD

    // initialize LCD and return “handle” used to handle LCD
	lcdhd = lcdInit(2,16,4,RS,EN,D4,D5,D6,D7,0,0,0,0);  
    if(lcdhd == -1){
        cout << "lcdInit failed !" << endl;
        return 1;
    }

    // startup     
    cout << "Program is starting ..." << endl;
    lcdPosition(lcdhd, 0, 0);
    lcdPrintf(lcdhd, "PROGRAM INIT", 1);

    // main loop
    while(1){
        key = keypad.getKey();              //get the state of keys

        /*
         * Key Code
         * 1 - Previous Song, 3 - Next Song
         * 4 - Rewind 10s, 5 - Pause, 6 - Forward 10s
        */
        switch(key) {
            case '1':
                changeSong(--songIterator, playlist, music);
                cout << "PREVIOUS SONG" << endl;
                break;
            case '3':
                changeSong(++songIterator, playlist, music);
                cout << "NEXT SONG" << endl;
                break;
            case '4':
                music.setPlayingOffset(music.getPlayingOffset() - sf::seconds(10));
                cout << "REWINDED" << music.getPlayingOffset().asSeconds() << endl;
                break;
            case '5':
                cout << "PAUSED: " << isPaused << endl;
                if(isPaused) {
                    music.play();
                } else {
                    music.pause();
                }
                isPaused = !isPaused;
                break;
            case '6':
                music.setPlayingOffset(music.getPlayingOffset() + sf::seconds(10));
                cout << "FORWARDED" << music.getPlayingOffset().asSeconds() << endl;
                break;
            case 'A':
                return 1;
                break;
            default:
                break;
        } 

        printDurationLCD(music);
    }
    return 1;
}

/**
 * Detects the I2CLCD1602, provided by FreeNove
 *
 * @param addr address of I2CLCD1602
 * @return status of I2C
*/
int detectI2C(int addr){
    int _fd = wiringPiI2CSetup (addr);   
    if (_fd < 0){		
        printf("Error address : 0x%x \n",addr);
        return 0 ;
    } 
    else{	
        if(wiringPiI2CWrite(_fd,0) < 0){
            printf("Not found device in address 0x%x \n",addr);
            return 0;
        }
        else{
            printf("Found device in address 0x%x \n",addr);
            return 1 ;
        }
    }
}

/**
 * Shortens a string so that it fits on the I2CLCD1602. 
 *
 * @param str the string to shorten
 * @return the shortened string
*/
string truncateTextToLCD(string str) {
    if(str.length() > 16) {
        // cout << str.substr(0, 16) << endl;
        return str.substr(0, 16) + "\0";
    }

    return str;
}

/**
 * Displays the current song on the I2CLCD1602.
 *
 * @param song the file path to the song
*/
void printSongLCD(string song) {
    // cout << "PRINTSONGLCD" << endl;
    string truncatedString = truncateTextToLCD(song.substr(11));

    lcdPosition(lcdhd,0,0);                             // set the LCD cursor position to (0,0) 
    lcdPrintf(lcdhd, "%s", truncatedString.c_str());    // display song name on the LCD
}

/**
 * Displays the status or duration of the song.
 * 
 * @param music the music object to access its functions
*/
void printDurationLCD(sf::Music& music) {
    // set the LCD cursor position to (0, 1)
    lcdPosition(lcdhd, 0, 1);

    // check if music is playing
    if(music.getStatus() == sf::SoundSource::Paused) {
        lcdPrintf(lcdhd, "PAUSED     ", 1);
        return;
    } else if(music.getStatus() == sf::SoundSource::Stopped) {
        lcdPrintf(lcdhd, "STOPPED    ", 1);
        return;
    }

    // get time and convert it to an int to display on LCD
    sf::Time currentTime = music.getPlayingOffset();
    sf::Time maxDuration = music.getDuration();

    float playingSeconds = currentTime.asSeconds();
    float durationSeconds = maxDuration.asSeconds();

    int minutes = static_cast<int>(playingSeconds) / 60;
    int seconds = static_cast<int>(playingSeconds) % 60;
    int totalMins = static_cast<int>(durationSeconds) / 60;
    int totalSecs = static_cast<int>(durationSeconds) % 60;

    // display duration on the LCD
    lcdPrintf(lcdhd, "%.1d:%.2d/%.1d:%.2d      ", minutes, seconds, totalMins, totalSecs);
}

/**
 * Changes the song to the next or previous song.
 * 
 * @param itr reference variable to the songIterator int
 * @param playlist vector of strings representing the file paths to the songs
 * @param music the music object to access its functions
*/
void changeSong(int& itr, vector<string> playlist, sf::Music& music) {
    cout << "CHANGED SONG" << endl;

    // Circular array to be able to loop through the directory
    int val = itr % playlist.size();

    if(music.openFromFile(playlist[val])) {
        cout << "Changed Song!" << playlist[val].c_str() << endl;
        printSongLCD(playlist[val]);
    } else {
        cout << "FAIL: Changed Song" << endl;
    }

    music.play();
}