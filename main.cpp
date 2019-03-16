#include <Arduino.h>
#include <SoftwareSerial.h>
#include <JQ6500_Serial.h>

// Create the mp3 module object,
//   Arduino Pin 8 is connected to TX of the JQ6500
//   Arduino Pin 11 is connected to one end of a  1k resistor,
//     the other end of the 1k resistor is connected to RX of the JQ6500
//   If your Arduino is 3v3 powered, you can omit the 1k series resistor
#define ARDUINO_RX_JQ6500_TX 11
#define ARDUINO_TX_JQ6500_RX 8
JQ6500_Serial mp3(ARDUINO_RX_JQ6500_TX, ARDUINO_TX_JQ6500_RX);

#define CHOICE_OFF      0 //Used to control LEDs
#define CHOICE_NONE     0 //Used to check buttons
#define CHOICE_RED  (1 << 0)
#define CHOICE_GREEN    (1 << 1)
#define CHOICE_BLUE (1 << 2)
#define CHOICE_YELLOW   (1 << 3)

#define LED_RED     10
#define LED_GREEN   3
#define LED_BLUE    13
#define LED_YELLOW  5

// Button pin definitions
#define BUTTON_RED    9
#define BUTTON_GREEN  2
#define BUTTON_BLUE   12
#define BUTTON_YELLOW 6

// Buzzer pin definitions
#define BUZZER1  4
#define BUZZER2  7

// Define game parameters
#define ROUNDS_TO_WIN      4 //Number of rounds to succesfully remember before you win. 13 is do-able.
#define ENTRY_TIME_LIMIT   3000 //Amount of time to press a button before game times out. 3000ms = 3 sec

#define MODE_MEMORY  0

// Game state variables
byte gameMode = MODE_MEMORY; //By default, let's play the memory game
byte gameBoard[32]; //Contains the combination of buttons as we advance
byte gameRound = 0; //Counts the number of succesful rounds the player has made it through

void setLEDs(byte leds);
void buzz_sound(int buzz_length_ms, int buzz_delay_us);
void attractMode(void);
boolean play_memory(void);
void play_winner(void);
void play_loser(void);
void add_to_moves(void);
byte wait_for_button(void);
void playMoves(void);
void toner(byte which, int buzz_length_ms);
byte checkButton(void);
void winner_sound(void);

void setup() {
	//Setup hardware inputs/outputs. These pins are defined in the hardware_versions header file
	Serial.begin(115200);

	//Enable pull ups on inputs
	pinMode(BUTTON_RED, INPUT_PULLUP);
	pinMode(BUTTON_GREEN, INPUT_PULLUP);
	pinMode(BUTTON_BLUE, INPUT_PULLUP);
	pinMode(BUTTON_YELLOW, INPUT_PULLUP);

	pinMode(LED_RED, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	pinMode(LED_YELLOW, OUTPUT);

	pinMode(BUZZER1, OUTPUT);
	pinMode(BUZZER2, OUTPUT);

	mp3.begin(9600);
	mp3.reset();
	//volume 0 - 30
	mp3.setVolume(20);
	//don't loop by default
	mp3.setLoopMode(MP3_LOOP_NONE);
	Serial.println("Number of files in BUILTIN source: " + String(mp3.countFiles(MP3_SRC_BUILTIN)));
	Serial.println("Number of files in SD source: " + String(mp3.countFiles(MP3_SRC_SDCARD)));
	//select source: built-in memory
	//mp3.setSource(MP3_SRC_BUILTIN);
	//or from SD card
	//mp3.setSource(MP3_SRC_SDCARD);

	//Mode checking
	gameMode = MODE_MEMORY; // By default, we're going to play the memory game
}

void loop() {
	{
		attractMode(); // Blink lights while waiting for user to press a button

		// Indicate the start of game play
		setLEDs(CHOICE_RED | CHOICE_GREEN | CHOICE_BLUE | CHOICE_YELLOW); // Turn all LEDs on
		delay(1000);
		setLEDs(CHOICE_OFF); // Turn off LEDs
		delay(250);
	}
	if (gameMode == MODE_MEMORY) {
		// Play memory game and handle result
		if (play_memory() == true)
			play_winner(); // Player won, play winner tones
		else
			play_loser(); // Player lost, play loser tones
	}

}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//The following functions are related to game play only

// Play the regular memory game
// Returns 0 if player loses, or 1 if player wins
boolean play_memory(void) {
	randomSeed(millis()); // Seed the random generator with random amount of millis()

	gameRound = 0; // Reset the game to the beginning

	while (gameRound < ROUNDS_TO_WIN) {
		add_to_moves(); // Add a button to the current moves, then play them back

		playMoves(); // Play back the current game board

		// Then require the player to repeat the sequence.
		for (byte currentMove = 0; currentMove < gameRound; currentMove++) {
			byte choice = wait_for_button(); // See what button the user presses

			if (choice == 0)
				return false; // If wait timed out, player loses

			if (choice != gameBoard[currentMove])
				return false; // If the choice is incorect, player loses
		}

		delay(1000); // Player was correct, delay before playing moves
	}

	return true; // Player made it through all the rounds to win!
}

// Plays the current contents of the game moves
void playMoves(void) {
	for (byte currentMove = 0; currentMove < gameRound; currentMove++) {
		toner(gameBoard[currentMove], 150);

		// Wait some amount of time between button playback
		// Shorten this to make game harder
		delay(150); // 150 works well. 75 gets fast.
	}
}

// Adds a new random button to the game sequence, by sampling the timer
void add_to_moves(void) {
	byte newButton = random(0, 4); //min (included), max (exluded)

	// We have to convert this number, 0 to 3, to CHOICEs
	if (newButton == 0)
		newButton = CHOICE_RED;
	else if (newButton == 1)
		newButton = CHOICE_GREEN;
	else if (newButton == 2)
		newButton = CHOICE_BLUE;
	else if (newButton == 3)
		newButton = CHOICE_YELLOW;

	gameBoard[gameRound++] = newButton; // Add this new button to the game array
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//The following functions control the hardware

// Lights a given LEDs
// Pass in a byte that is made up from CHOICE_RED, CHOICE_YELLOW, etc
void setLEDs(byte leds) {
	if ((leds & CHOICE_RED) != 0)
		digitalWrite(LED_RED, HIGH);
	else
		digitalWrite(LED_RED, LOW);

	if ((leds & CHOICE_GREEN) != 0)
		digitalWrite(LED_GREEN, HIGH);
	else
		digitalWrite(LED_GREEN, LOW);

	if ((leds & CHOICE_BLUE) != 0)
		digitalWrite(LED_BLUE, HIGH);
	else
		digitalWrite(LED_BLUE, LOW);

	if ((leds & CHOICE_YELLOW) != 0)
		digitalWrite(LED_YELLOW, HIGH);
	else
		digitalWrite(LED_YELLOW, LOW);
}

// Wait for a button to be pressed.
// Returns one of LED colors (LED_RED, etc.) if successful, 0 if timed out
byte wait_for_button(void) {
	long startTime = millis(); // Remember the time we started the this loop

	while ((millis() - startTime) < ENTRY_TIME_LIMIT) // Loop until too much time has passed
	{
		byte button = checkButton();

		if (button != CHOICE_NONE) {
			toner(button, 150); // Play the button the user just pressed

			while (checkButton() != CHOICE_NONE)
				;  // Now let's wait for user to release button

			delay(10); // This helps with debouncing and accidental double taps

			return button;
		}

	}

	return CHOICE_NONE; // If we get here, we've timed out!
}

// Returns a '1' bit in the position corresponding to CHOICE_RED, CHOICE_GREEN, etc.
byte checkButton(void) {
	if (digitalRead(BUTTON_RED) == 0)
		return (CHOICE_RED);
	else if (digitalRead(BUTTON_GREEN) == 0)
		return (CHOICE_GREEN);
	else if (digitalRead(BUTTON_BLUE) == 0)
		return (CHOICE_BLUE);
	else if (digitalRead(BUTTON_YELLOW) == 0)
		return (CHOICE_YELLOW);

	return (CHOICE_NONE); // If no button is pressed, return none
}

// Light an LED and play tone
// Red, upper left:     440Hz - 2.272ms - 1.136ms pulse
// Green, upper right:  880Hz - 1.136ms - 0.568ms pulse
// Blue, lower left:    587.33Hz - 1.702ms - 0.851ms pulse
// Yellow, lower right: 784Hz - 1.276ms - 0.638ms pulse
void toner(byte which, int buzz_length_ms) {
	setLEDs(which); //Turn on a given LED

	//Play the sound associated with the given LED
	switch (which) {
	case CHOICE_RED:
		buzz_sound(buzz_length_ms, 1136);
		break;
	case CHOICE_GREEN:
		buzz_sound(buzz_length_ms, 568);
		break;
	case CHOICE_BLUE:
		buzz_sound(buzz_length_ms, 851);
		break;
	case CHOICE_YELLOW:
		buzz_sound(buzz_length_ms, 638);
		break;
	}

	setLEDs(CHOICE_OFF); // Turn off all LEDs
}

// Toggle buzzer every buzz_delay_us, for a duration of buzz_length_ms.
void buzz_sound(int buzz_length_ms, int buzz_delay_us) {
	// Convert total play time from milliseconds to microseconds
	long buzz_length_us = buzz_length_ms * (long) 1000;

	// Loop until the remaining play time is less than a single buzz_delay_us
	while (buzz_length_us > (buzz_delay_us * 2)) {
		buzz_length_us -= buzz_delay_us * 2; //Decrease the remaining play time

		// Toggle the buzzer at various speeds
		digitalWrite(BUZZER1, LOW);
		digitalWrite(BUZZER2, HIGH);
		delayMicroseconds(buzz_delay_us);

		digitalWrite(BUZZER1, HIGH);
		digitalWrite(BUZZER2, LOW);
		delayMicroseconds(buzz_delay_us);
	}

	digitalWrite(BUZZER1, LOW);
	digitalWrite(BUZZER2, LOW);

}

// Play the winner sound and lights
void play_winner(void) {
	setLEDs(CHOICE_GREEN | CHOICE_BLUE);
	winner_sound();
	setLEDs(CHOICE_RED | CHOICE_YELLOW);
	winner_sound();
	setLEDs(CHOICE_GREEN | CHOICE_BLUE);
	winner_sound();
	setLEDs(CHOICE_RED | CHOICE_YELLOW);
	winner_sound();
}

void wait_for_end_of_sound() {
	int status = MP3_STATUS_PLAYING;
	do {
		int status = (int) mp3.getStatus();
		String szStatus = "";
		switch(status){
			case MP3_STATUS_STOPPED: szStatus = "MP3_STATUS_STOPPED"; break;
			case MP3_STATUS_PLAYING: szStatus = "MP3_STATUS_PLAYING"; break;
			case MP3_STATUS_PAUSED: szStatus = "MP3_STATUS_PAUSED"; break;
			default: szStatus = "Unknown state " + String(status); break;
		}
		Serial.println("Status: " + szStatus);
	} while(status == MP3_STATUS_PLAYING);
}

// Play the winner sound
// This is just a unique (annoying) sound we came up with, there is no magic to it
void winner_sound(void) {
	//trigger playing sound 1
	Serial.println("Triggered winner sound");
	mp3.playFileByIndexNumber(2);
	delay(200); //wait a little bit so file starts playing
	//block execution until file is done playing
	wait_for_end_of_sound();
	//if we reach here, then the status is not "playing" anymore
	//i.e., STOPPED or PAUSED
	Serial.println("End of winner sound");
}

void looser_sound() {
	//trigger playing sound 2
	Serial.println("Triggered loser sound");
	mp3.playFileByIndexNumber(1);
	delay(200);
	wait_for_end_of_sound();
	Serial.println("End of loser sound");
}

// Play the loser sound/lights
void play_loser(void) {
	//"loop" 3 times
	//just 3 times in series with delays, actually ;)
	setLEDs(CHOICE_RED | CHOICE_GREEN);
	looser_sound();
	setLEDs(CHOICE_BLUE | CHOICE_YELLOW);
	looser_sound();
	setLEDs(CHOICE_RED | CHOICE_GREEN);
	looser_sound();
	setLEDs(CHOICE_BLUE | CHOICE_YELLOW);
}

// Show an "attract mode" display while waiting for user to press button.
void attractMode(void) {
	while (1) {
		setLEDs(CHOICE_RED);
		delay(100);
		if (checkButton() != CHOICE_NONE)
			return;

		setLEDs(CHOICE_BLUE);
		delay(100);
		if (checkButton() != CHOICE_NONE)
			return;

		setLEDs(CHOICE_GREEN);
		delay(100);
		if (checkButton() != CHOICE_NONE)
			return;

		setLEDs(CHOICE_YELLOW);
		delay(100);
		if (checkButton() != CHOICE_NONE)
			return;
	}
}
