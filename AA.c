/*

CAB202 Assignment 2

Christopher Ayling
n9713581

*/

///===============================================================
//				#INCLUDE STATEMENTS
///===============================================================

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "lcd.h"
#include "graphics.h"
#include "cpu_speed.h"
#include "sprite.h"
#include "usb_serial.h"


///===============================================================
//				CONSTANT DEFINITIONS
///===============================================================

//time
#define FREQUENCY 8000000.0
#define PRESCALER_0 1024.0
#define PRESCALER_1 1024.0

//button states
#define NUM_BUTTONS 6
#define BTN_DPAD_LEFT 0
#define BTN_DPAD_RIGHT 1
#define BTN_DPAD_UP 2
#define BTN_DPAD_DOWN 3
#define BTN_LEFT 4
#define BTN_RIGHT 5

#define BTN_STATE_UP 0
#define BTN_STATE_DOWN 1

//physics
#define SPACESHIPSPEED 0.2
#define MISSILESPEED 0.5
#define ALIENSPEED 0.3
#define MOTHERSHIPSPEED 0.2

//directions
#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

//objects
#define NUM_OF_MISSILES 5
#define NUM_OF_ALIENS 5
#define MOTHERSHIPFULLHEALTH 10

///===============================================================
//				GLOBAL VARIABLES
///===============================================================


//GAME MODE
bool inBossFight = false;

//gameplay variables
int lives = 10;
int score = 0;
double gametime = 0.0;

int ssdir = 0; //spaceShip direction

char buffer[80];//for sprintf

volatile unsigned char btn_hists[NUM_BUTTONS];
volatile unsigned char btn_states[NUM_BUTTONS];

volatile unsigned long ovf_count = 0; //overflow counter

double prev_system_time = 0.0; //for 0.5 second timings

double prev_attack_time[NUM_OF_ALIENS] = {0.0};
double attackCountdown[NUM_OF_ALIENS] = {0.0};
bool alienAttacking[NUM_OF_ALIENS] = {false};
int liveAliens = NUM_OF_ALIENS;

int atkTmin = 2;
int atkTmax = 10;

int MothershipHealth = MOTHERSHIPFULLHEALTH;
double MS_prev_attack_time = 0.0;
double MS_attackCountdown = 0.0;
bool MS_Attacking = 0.0;


///===============================================================
//				FUNCTION DECLARATIONS
///===============================================================

//OBJECT FUNCTIONS
void initSpaceShip(void);
void initMissiles(void);
void initAliens(void);
void initSprites(void);
void stepMissiles(void);
void stepAliens(void);
void stepSprites(void);
void respawnSpaceShip(bool safeSpawn);
void respawnSprite(Sprite* sprite);
void aim(void);
void aimAlien(Sprite* alien);
void launchMissile(void);
void checkCollisions(void);
void stepSprite(Sprite* sprite);
void checkCollisions(void);
bool collisionBetween(Sprite* spriteA, Sprite* spriteB);

//BOSS FIGHT
void stepMotherShip(void);
void stepMotherShipMissiles(void);
void checkMotherShipCollisions(void);
void beginMothershipFight(void);

//SETUP FUNCTIONS
void setupGame(bool reset);
void init_Hardware(void);

//GUI FUNCTIONS
void updateGraphics(void);
void drawSprites(void);
void drawBorder(void);
void drawStatus(void);
void menu(void);
void gameOverDialog(void);

//UTILITY FUNCTIONS
void waitForInput(void);
void draw_centred(unsigned char y, char* string);
unsigned int rand_interval(unsigned int min, unsigned int max);

//DEBUG FUNCTIONS
void send_debug_string(char* string);
void send_line(char* string);
void connect_to_serial(void);

//TIMER FUNCTIONS
void debugTimeCheck(void);
void alienAttackTimerCheck(void);
double get_system_time();
//ISRs go here too



///===============================================================
//				SPRITES AND SPRITE DECLARATIONS
///===============================================================

//Spaceship
unsigned char SpaceShip_img[7] = {
	0b01111100,
	0b10111010,
	0b11010110,
	0b11111110,
	0b11010110,
	0b10111010,
	0b01111100
};
Sprite spaceShip;

//Missile
unsigned char Missile_img[2] = {
	0b11000000,
	0b11000000
};
Sprite missiles[NUM_OF_MISSILES];

//Alien
unsigned char Alien_img[] = {
	0b01100110,
	0b00111100,
	0b11011100,
	0b10001000
};
Sprite aliens[NUM_OF_ALIENS];

unsigned char Mothership_img[] = {
	0b01111110,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b01111110
};
Sprite motherShip;


///===============================================================
//				MAIN
///===============================================================
///###############################################################

int main(){
	set_clock_speed(CPU_8MHz);

	//initiate the hardware
	init_Hardware();
	
	//Wait for debugger
	connect_to_serial();
	
	srand((unsigned) get_system_time());

	//show the menu
	menu();

	//set up game
	setupGame(false);

	//begin play
	while (1){ //loop infintely
		//process
		stepSprites();
		
		//CheckCollisions
		checkCollisions();

		//update graphics
		updateGraphics();		
		
		//if game over
		if (lives < 1){
			if (inBossFight){
				send_debug_string("Mothership Destroyed Player");
			}//end if
			
			//ask to play again
			gameOverDialog();
			//wait for input
			//return to menu screen and reset game
			setupGame(true);
		}//end game over check
		
		if (inBossFight){//check for death of motherShip
			if (MothershipHealth < 1){
				send_debug_string("Player Destroyed Mothership");//DEBUG
				inBossFight = false;
				setupGame(true);//check
			}//end if
			
		} else {//check for death of all aliens
			//check if should begin boss fight
			if (liveAliens < 1){
				inBossFight = true;
				beginMothershipFight();
			}//end if
		}//end if else
			
	}//end main loop

	return 0;
}//end main

///###############################################################
///===============================================================
//				OBJECT FUNCTIONS
///===============================================================

//missiles spawn in at -5x
//aliesn spawn in at -5y

/*
Initialise the spaceShip at a random location
*/
void initSpaceShip(void){
	init_sprite(&spaceShip, rand_interval(6,72), rand_interval(16,36), 8, 7, SpaceShip_img);
}//end initSpaceShip


/*
Spawn 5 missiles
*/
void initMissiles(void){
	for (int i = 0; i < NUM_OF_MISSILES; i++){
		init_sprite(&missiles[i], -5, 0, 3, 2, Missile_img);
	}//end for
}//end initMissiles


/*
Spawn 5 aliens
*/
void initAliens(void){
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		init_sprite(&aliens[i], 0, -5, 7, 4, Alien_img);
		aliens[i].is_visible = true;
	}//end for
}//end initAliens


/*
Initiate sprites
*/
void initSprites(void){
	initMissiles();
	initSpaceShip();
	initAliens();
	init_sprite(&motherShip, 30, 30, 8, 8, Mothership_img);
}//end initSprites


/*
Step each missile
*/
void stepMissiles(void){
	for (int i = 0; i < NUM_OF_MISSILES; i++){
		//step
		missiles[i].x = missiles[i].x + missiles[i].dx;
		missiles[i].y = missiles[i].y + missiles[i].dy;
		
		//check if out of bounds
		if (missiles[i].x > 81 || missiles[i].x < 0 || missiles[i].y > 46 || missiles[i].y < 16){
			missiles[i].x = -5;
			missiles[i].y = 0;
			missiles[i].dx = 0;
			missiles[i].dy = 0;
			missiles[i].is_visible = false;
		}//end if
	}//end
}//end stepMissiles


/*
step each alien
*/
void stepAliens(void){
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		if (alienAttacking[i]){
			if (aliens[i].is_visible == true){//check if in play
				stepSprite(&aliens[i]);
			}//end if
			//FIX WALL COLLISIONS
			if (aliens[i].x > 76 || aliens[i].x < 2 || aliens[i].y > 43 || aliens[i].y < 12){
				aliens[i].dx = 0;
				aliens[i].dy = 0;
				prev_attack_time[i] = get_system_time();
				alienAttacking[i] = false;
			}//end if
		}//end if
	}//end for
}//end stepAliens


/*
Step all sprites
*/
void stepSprites(void){
	//player missiles
	stepMissiles();	
	
	//aliens
	if (inBossFight){
		stepMotherShip();
		stepMotherShipMissiles();
	} else{
		stepAliens();
	}
}//end stepSprites


/*
Respawn the spaceship at a random location
*/
void respawnSpaceShip(bool safeSpawn){
	/*
	if (safeSpawn){ //ensure that the spaceShip doesn't spawn on an alien
		bool alienOverlap = false;
		do{
			spaceShip.x = rand_interval(6,72);
			spaceShip.y = rand_interval(16,36);
			for (int i = 0; i < NUM_OF_ALIENS; i++){
				if (collisionBetween(&spaceShip, &aliens[i])){
					alienOverlap = true;			
				}//end if
			}//end for			
		}while(alienOverlap);
	} else {
		spaceShip.x = rand_interval(6,72);
		spaceShip.y = rand_interval(16,36);
	}//end if else
	*/
	spaceShip.x = rand_interval(6,72);
	spaceShip.y = rand_interval(16,36);
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		if (collisionBetween(&spaceShip, &aliens[i])){
			respawnSpaceShip(false);			
		}//end if
	}//end for
	
}//end respawnSpaceShip


/*
Place all aliens on the screen and ensure they don't overlap the spaceShip
*/
void respawnAllAliens(void){
	for (int i = 0; i < NUM_OF_ALIENS; i ++){
		do{
			respawnSprite(&aliens[i]);
		}while (collisionBetween(&spaceShip, &aliens[i]));	
	}//end for
}//end respawnAllAliens


/*
Respawn sprite (change to exclusively aliens?) at a random location
*/
void respawnSprite(Sprite* sprite){
	sprite->x = rand_interval(6,72);
	sprite->y = rand_interval(16,36);
}//end respawnSprite


/*
aim the space craft's gun
*/
void aim(){
	if (ssdir == 0){//up
		draw_line(spaceShip.x + 3, spaceShip.y + 3, spaceShip.x + 3, spaceShip.y -2);
	}
	
	if (ssdir == 1){//right
		draw_line(spaceShip.x + 3, spaceShip.y + 3, spaceShip.x + 8, spaceShip.y +3);
	}
	
	if (ssdir == 2){//down
		draw_line(spaceShip.x + 3, spaceShip.y + 3, spaceShip.x + 3, spaceShip.y +8);
	}
	
	if (ssdir == 3){//left
		draw_line(spaceShip.x + 3, spaceShip.y + 3, spaceShip.x - 2, spaceShip.y +3);
	}
}//end aim


/*
Aim the specified alien towards the spaceship
*/
void aimAlien(Sprite* alien){
	//for finding unit vector
	double dist = sqrt((alien->x - spaceShip.x)*(alien->x - spaceShip.x)+(alien->y - spaceShip.y)*(alien->y - spaceShip.y));
	
	if (inBossFight){//aim the mothership
		alien->dx = MOTHERSHIPSPEED*(-(alien->x - spaceShip.x)/dist);
		alien->dy = MOTHERSHIPSPEED*(-(alien->y - spaceShip.y)/dist);		
	} else {
		alien->dx = ALIENSPEED*(-(alien->x - spaceShip.x)/dist);
		alien->dy = ALIENSPEED*(-(alien->y - spaceShip.y)/dist);
	}//end if else
}//end aimAlien


/*
launch a missile in aiming direction if not already 5 out
*/
void launchMissile(void){
	for (int i = 0; i < NUM_OF_MISSILES; i++){
		if (missiles[i].is_visible == false){
			
			//launch
			if (ssdir == UP){
				missiles[i].x = spaceShip.x + 3;
				missiles[i].y = spaceShip.y - 2;
				missiles[i].dx = 0;
				missiles[i].dy = -MISSILESPEED;
			} else if (ssdir == DOWN){
				missiles[i].x = spaceShip.x + 3;
				missiles[i].y = spaceShip.y + 8;
				missiles[i].dx = 0;
				missiles[i].dy = MISSILESPEED;
			} else if (ssdir == LEFT){
				missiles[i].x = spaceShip.x - 4;
				missiles[i].y = spaceShip.y + 3;
				missiles[i].dx = -MISSILESPEED;
				missiles[i].dy = 0;
			} else if (ssdir == RIGHT){
				missiles[i].x = spaceShip.x + 8;
				missiles[i].y = spaceShip.y + 3;
				missiles[i].dx = MISSILESPEED;
				missiles[i].dy = 0;
			}//end launch
			
			missiles[i].is_visible = true;
			break;
		}//end if
	}//end for	
}//end launchMissile


void stepSprite(Sprite* sprite){
	sprite->x = sprite->x + sprite->dx;
	sprite->y = sprite->y + sprite->dy;	
}//end stepSprite


/*
check for collisions between all objects
*/
void checkCollisions(void){
	if (inBossFight){//for boss fight mode
		checkMotherShipCollisions();
		
	} else {//for normal mode
	
		//between spaceship and aliens
		for (int i = 0; i < NUM_OF_ALIENS; i++){
			if (collisionBetween(&spaceShip, &aliens[i])){
				//negative life
				lives--;
				send_debug_string("Player lost a life");//DEBUG
				//respawn ship
				respawnSpaceShip(true);
				
			}//end if
		}//end for
		
		//between aliens and missiles
		for (int i = 0; i < NUM_OF_MISSILES; i++){
			for (int i = 0; i < NUM_OF_ALIENS; i++){
				if (collisionBetween(&missiles[i], &aliens[i])){
					//missile dissapears
					missiles[i].dx = 0;
					missiles[i].dy = 0;
					missiles[i].x = -5;
					missiles[i].y = 0;
					
					//alien dissapears
					aliens[i].dx = 0;
					aliens[i].dy = 0;
					aliens[i].x = 0;
					aliens[i].y = -5;
					aliens[i].is_visible = false;
					
					//increment score
					score++;
					
					//decrement live alien counter
					liveAliens--;
					send_debug_string("Alien Killed");//DEBUG
									
				}//end if
			}//end for
		}//end for
	}//end else
	
}//end checkCollisions


/*
return true if spriteA and spriteB overlap
*/
bool collisionBetween(Sprite* spriteA, Sprite* spriteB){
	bool collision = true;
	
	//use assignment 1 code
	int spriteA_top = round(spriteA->y);
	int spriteA_left = round(spriteA->x);
	int spriteA_bottom = round(spriteA->y) + spriteA->height - 1;
	int spriteA_right = round(spriteA->x) + spriteA->width - 1;
	
	int spriteB_top = round(spriteB->y);
	int spriteB_left = round(spriteB->x);
	int spriteB_bottom = round(spriteB->y) + spriteB->height - 1;
	int spriteB_right = round(spriteB->x) + spriteB->width - 1;
	
	
	if(spriteA_top > spriteB_bottom) collision = false;
	else if (spriteA_bottom < spriteB_top) collision = false;
	else if (spriteA_left > spriteB_right) collision = false;
	else if (spriteA_right < spriteB_left) collision = false;
	
	return collision;
}//end collisionBetween

///===============================================================
//				MOTHERSHIP FIGHT FUNCTIONS
///===============================================================


/*
Step the motherShip
*/
void stepMotherShip(void){	
	if (MS_Attacking){
		//step
		stepSprite(&motherShip);
		//check for collision with wall
		/////////////////////////////////////////CHANGE LIMITS
		if (motherShip.x > 76 || motherShip.x < 2 || motherShip.y > 43 || motherShip.y < 12){
			motherShip.dx = 0;
			motherShip.dy = 0;
			MS_prev_attack_time = get_system_time();
			MS_Attacking = false;
		}//end if
		
		//change health bar position
		if (motherShip.y < 44){
			//draw health at bottom
		} else {
			//draw at top
		}
		
	}//end if
	
}//end stepMotherShip


/*
Step the mothership's missiles
*/
void stepMotherShipMissiles(void){
	
}//end stepMotherShipMissiles


/*
Mothership collisions OBSELETE
*/
void checkMotherShipCollisions(void){
	//between spaceShip and motherShip
	if (collisionBetween(&spaceShip, &motherShip)){
		
	}//end if
	
	//between spaceShip missiles and motherShip
		//decrement health by 1
	
	//between mothership missiles and spaceShip
		//decrement lives by 1
	
}//end


/*
set up the mother ship fight
*/
void beginMothershipFight(void){
	//display fight message
	clear_screen();
	draw_centred(20, "BOSS FIGHT");
	show_screen();
	_delay_ms(2000);
	
	//spawn motherShip
	motherShip.x = 20;
	motherShip.y = 20;
	//set to full health
	MothershipHealth = MOTHERSHIPFULLHEALTH;
	//
	
}//end beginMothershipFight


///===============================================================
//				SETUP FUNCTIONS
///===============================================================


/*
Sets up the game
*/
void setupGame(bool reset){
	send_debug_string("Setting up the game...");
	//set score to 0
	score = 0;
	//set lives to 10
	lives = 10;
	//set time to 00:00 //reset timer???
	gametime = 0.0;
	ovf_count = 0;
	prev_system_time = 0.0;
	
	if (~reset){
		//initialise sprites
		initSprites();
	}//end if
	
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		prev_attack_time[i] = 0.0;
		attackCountdown[i] = 0.0;
		alienAttacking[i] = false;
	}//end for

	//move spaceship and aliens to starting locations
	respawnSpaceShip(false);
	respawnAllAliens();
	motherShip.x = -10;
	motherShip.y = -10;
	
	//set random countdown timers
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		attackCountdown[i] = (double) rand_interval(atkTmin,atkTmax);
	}//end for

}//end setupGame


/*
Initiates hardware
*/
void init_Hardware(void){
	//initialise the LCD screen
    lcd_init(LCD_DEFAULT_CONTRAST);

	//clear any characters that were written on the screen
	clear_screen();

	//set inputs for buttons
	DDRF &= ~((1 << PF5) | (1 << PF6));

	//set inputs for joystick
	DDRB &= ~(1<<1);
	DDRB &= ~(1<<7);
	DDRD &= ~(11<<0);

	//set inputs for potentiometer
	//ADMUX blah blah
	
	//initialise the USB serial
	usb_init();	
	
	//initialise timers with overflow interrupts:
	// 0 - button press checking @ ~30Hz
	// 1 - system time overflowing every 8.3s
	TCCR0B |= ((1 << CS02) | (1 << CS00));
	TCCR1B |= ((1 << CS12) | (1 << CS10));
	TIMSK0 |= (1 << TOIE0);
	TIMSK1 |= (1 << TOIE1);

	// Configure all necessary timers in "normal mode", enable all necessary
    // interupts, and configure prescalers as desired
	TCCR1B &= ~((1<<WGM02));
    // Set the prescaler for TIMER1 so that the clock overflows every ~8.3 seconds
	TCCR1B |= (1<<CS02)|(1<<CS00);
    TCCR1B &= ~((1<<CS01));
	TCCR0B &= ~((1<<WGM02));
    // Set the prescaler for TIMER0 so that the clock overflows every ~2.1 seconds
	TCCR0B |= (1<<CS02);
	TCCR0B &= ~((1<<CS01));
	TCCR0B &= ~((1<<CS00));
	TIMSK0 |= (0b1 << TOIE0);
    // Globally enable interrupts
    sei();
}//end init_Hardware


///===============================================================
//				GUI FUNCTIONS
///===============================================================

/*
updates the graphics during gameplay
*/
void updateGraphics(void){
	clear_screen();

	//draw border
	drawBorder();

	//draw status
	drawStatus();	

	//sprites etc ect
	drawSprites();
	draw_sprite(&motherShip);

	show_screen();
	_delay_ms(5);
}//end updateGraphics


/*
draw all the sprites
*/
void drawSprites(void){
	draw_sprite(&spaceShip);
	aim();
	
	//draw missiles
	for (int i = 0; i < NUM_OF_MISSILES; i++){
		draw_sprite(&missiles[i]);
	}//end for
	
	if (inBossFight){
		draw_sprite(&motherShip);
	} else {
		//draw aliens
		for (int i = 0; i < NUM_OF_ALIENS; i++){
			draw_sprite(&aliens[i]);
		}//end for
	}//end if else
	
}//end drawSprites


/*
Draws the playing field border
*/
void drawBorder(void){
	draw_line(0, 10, 83, 10);//top
	draw_line(0, 47, 83, 47);//bottom
	draw_line(0, 10, 0, 47);//left
	draw_line(83, 10, 83, 47);//right
}//end drawBorder


/*
Draws and updates the status display
*/
void drawStatus(void){
	sprintf(buffer, "L:%d S:%d T:%3.2f", lives, score, get_system_time());
	draw_string(0,0, buffer);
}//end drawStatus


/*
Draws the menu to the screen
*/
void menu(void){
	send_debug_string("Displaying Menu Screen");//DEBUG
	// Display Menu
	clear_screen();
	//game title
	draw_string(0,0, "Alien Advance");
	//name
	draw_string(0, 10, " Chris Ayling");
	//student number
	draw_string(0, 20, " n9713581");
	//press a button to continue
	draw_string(0, 30, "Press any button");
	draw_string(0, 40, " to continue...");
	show_screen();
	
	waitForInput();

	// Countdown from 3 at intervals of 300ms
	//3
	clear_screen();
	draw_string(0,0, "3");
	show_screen();
	_delay_ms(300);
	//2
	clear_screen();
	draw_string(0,0, "2");
	show_screen();
	_delay_ms(300);
	//1
	clear_screen();
	draw_string(0,0, "1");
	show_screen();
	_delay_ms(300);
	clear_screen();
	
	send_debug_string("Beginning game");//DEBUG
}//end menu


/*
Display game over dialog and wait for input
*/
void gameOverDialog(void){
	send_debug_string("Game Over.");
	//Disply game over dialog
	clear_screen();
	draw_string(0,0, "Game Over");
	draw_string(0,10, "Play again?");
	show_screen();

	//wait for input
	waitForInput();

	//reset game
	clear_screen();
	draw_string(0,0, "Reset tbd");
	show_screen();

	//REMOVE AFTER IMPLEMETING//
	waitForInput();
}//end gameOverDialog


///===============================================================
//				UTILITY FUNCTIONS
///===============================================================


/*
Waits for input from the left or right buttons
*/
void waitForInput(void){
	while (1){//infinite loop
		//if button pressed
		if (((PINF>>5) & 0b1)||(((PINF>>6) & 0b1))){
			//break out of loop
			break;
		}//end if
	}//end while
}//end waitForInput


/*
Draw a string centred in the LCD when you don't know the string length
*/
void draw_centred(unsigned char y, char* string) {    
   unsigned char l = 0, i = 0;
   while (string[i] != '\0') {
		l++;
		i++;
	}//end while
    char x = 42-(l*5/2);
    draw_string((x > 0) ? x : 0, y, string);
}//end draw_centred


/*
Generate a random number between min and max
*/
unsigned int rand_interval(unsigned int min, unsigned int max){
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do{
        r = rand();
    }while (r >= limit);

    return min + (r / buckets);
}//end rand_interval


///===============================================================
//				DEBUG FUNCTIONS
///===============================================================


/*
Send debug string format: [DEBUG @ ###.###]
*/
void send_debug_string(char* string) {
     // Send the debug preamble...
	 sprintf(buffer,"[DBG @ %03.3f] ", get_system_time());
     usb_serial_write(buffer, 16);

     // Send all of the characters in the string
     unsigned char char_count = 0;
     while (*string != '\0') {
         usb_serial_putchar(*string);
         string++;
         char_count++;
     }//end while

     // Go to a new line (force this to be the start of the line)
     usb_serial_putchar('\r');
     usb_serial_putchar('\n');
 }//end send_debug_string
 
 
 /*
Send a string through serial
*/
void send_line(char* string) {
    // Send all of the characters in the string
    unsigned char char_count = 0;
    while (*string != '\0') {
        usb_serial_putchar(*string);
        string++;
        char_count++;
    }

    // Go to a new line (force this to be the start of the line)
    usb_serial_putchar('\r');
    usb_serial_putchar('\n');
}//end send_line


/*
Connect teensy to serial debugger and display messages
*/
void connect_to_serial(void){
	draw_centred(17, "Waiting for");
    draw_centred(24, "debugger...");
    show_screen();
	//wait for connection
    while(!usb_configured() || !usb_serial_get_control());
	send_debug_string("Greetings, Human");
	send_line("CONTROLS");
	send_line("Movement: up, down, left and right");
	send_line("Missiles: M");
	clear_screen();
	draw_centred(17, "DGB Active");
    draw_centred(24, "starting game");
	show_screen();
	_delay_ms(2000);
}//end connect_to_serial
 

///===============================================================
//				TIMER FUNCTIONS
///===============================================================


//get system time
double get_system_time(void) {
    return (ovf_count * 65536 + TCNT1) * PRESCALER_1 / FREQUENCY;
}//end get_system_time


/*
debug timing
*/
void debugTimeCheck(void){
	if ((get_system_time() - prev_system_time) > 0.5){
		prev_system_time = get_system_time();
		send_debug_string("debugxy"); //FORMAT PROPERLY
	}//end if
}//end debugTimeCheck


/*
alien attack timing
*/
void alienAttackTimerCheck(void){
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		if (alienAttacking[i] == false){
			if (get_system_time() - prev_attack_time[i] > attackCountdown[i]){
				alienAttacking[i] = true;
				aimAlien(&aliens[i]);
				prev_attack_time[i] = get_system_time();
				attackCountdown[i] = (double)rand_interval(atkTmin, atkTmax);
			}//end if
		}//end if
	}//end for
}//end alienAttackTimerCheck


/*
check the state of all buttons
*/
ISR(TIMER0_OVF_vect){
	
	debugTimeCheck();
	
	alienAttackTimerCheck();
	
	for (int i = 0; i < NUM_BUTTONS; i++){
		btn_hists[i] = (btn_hists[i]<<0b1);
	}//end for
	btn_hists[BTN_DPAD_UP] |= ((PIND>>1) & 1)<<0b0;
	btn_hists[BTN_DPAD_LEFT] |= ((PINB>>1) & 1)<<0b0;
	btn_hists[BTN_DPAD_RIGHT] |= ((PIND) & 1)<<0b0;
	btn_hists[BTN_DPAD_DOWN] |= ((PINB>>7) & 1)<<0b0;
	btn_hists[BTN_LEFT] |= ((PINF>>6) & 1)<<0b0;
	btn_hists[BTN_RIGHT] |= ((PINF>>5) & 1)<<0b0;
	
	//stick up
	if (((PIND>>1) & 1) && (btn_hists[BTN_DPAD_UP] == 0b11111111)) {
		btn_states[BTN_DPAD_UP] = BTN_STATE_DOWN;
		//check collision with wall
		if (spaceShip.y > 12){
			spaceShip.y = spaceShip.y - SPACESHIPSPEED;//move spaceship up	
		}//end if	
		
		ssdir = UP;//aim up
		
	} else if ((btn_states[BTN_DPAD_UP] == BTN_STATE_DOWN)&&(btn_hists[BTN_DPAD_UP] == 0b00000000)){
		btn_states[BTN_DPAD_UP] = BTN_STATE_UP;		
	}
	
	//stick down
	if ((((PINB>>7) & 1) & 1) && (btn_hists[BTN_DPAD_DOWN] == 0b11111111)) {
		btn_states[BTN_DPAD_DOWN] = BTN_STATE_DOWN;
		//check collision with wall
		if (spaceShip.y < 40){
		spaceShip.y = spaceShip.y + SPACESHIPSPEED;
		}//end if
		ssdir = DOWN;//aim down		
	} else if ((btn_states[BTN_DPAD_DOWN] == BTN_STATE_DOWN)&&(btn_hists[BTN_DPAD_DOWN] == 0b00000000)){
		btn_states[BTN_DPAD_DOWN] = BTN_STATE_UP;
	}
	
	//stick left
	if (((PINB>>1) & 1) && (btn_hists[BTN_DPAD_LEFT] == 0b11111111)) {
		btn_states[BTN_DPAD_LEFT] = BTN_STATE_DOWN;
		//check collision with wall
		if (spaceShip.x > 2){
		spaceShip.x = spaceShip.x - SPACESHIPSPEED;
		}//end if
		ssdir = LEFT;//aim left
	} else if ((btn_states[BTN_DPAD_LEFT] == BTN_STATE_DOWN)&&(btn_hists[BTN_DPAD_LEFT] == 0b00000000)){
		btn_states[BTN_DPAD_LEFT] = BTN_STATE_UP;
	}
	
	//stick right
	if (((PIND) & 1) && (btn_hists[BTN_DPAD_RIGHT] == 0b11111111)) {
		btn_states[BTN_DPAD_RIGHT] = BTN_STATE_DOWN;
		//check collision with wall
		if (spaceShip.x < 75){
		spaceShip.x = spaceShip.x + SPACESHIPSPEED;
		}//end if
		ssdir = RIGHT;//aim right
	} else if ((btn_states[BTN_DPAD_RIGHT] == BTN_STATE_DOWN)&&(btn_hists[BTN_DPAD_RIGHT] == 0b00000000)){
		btn_states[BTN_DPAD_RIGHT] = BTN_STATE_UP;
	}
	
	//left button
	if ((((PINF>>6) & 1) & 1) && (btn_hists[BTN_LEFT] == 0b11111111)) {
		btn_states[BTN_LEFT] = BTN_STATE_DOWN;
	} else if ((btn_states[BTN_LEFT] == BTN_STATE_DOWN)&&(btn_hists[BTN_LEFT] == 0b00000000)){
		btn_states[BTN_LEFT] = BTN_STATE_UP;
		launchMissile();
	}
	
	//right button
	if ((((PINF>>5) & 1) & 1) && (btn_hists[BTN_RIGHT] == 0b11111111)) {
		btn_states[BTN_RIGHT] = BTN_STATE_DOWN;
	} else if ((btn_states[BTN_RIGHT] == BTN_STATE_DOWN)&&(btn_hists[BTN_RIGHT] == 0b00000000)){
		btn_states[BTN_RIGHT] = BTN_STATE_UP;
	}  
}//end ISR TIMER0_OVF_vect


/*
Increase the overflow counter for timer 1
*/
ISR(TIMER1_OVF_vect){
    ovf_count++;//increase overflow counter
	send_debug_string("TIMER1 Overflowed"); //DEBUG
}//end ISR TIMER1_OVF_vect


///===============================================================
//				CODE STORAGE AREA
///===============================================================

/*
void stepAliens(void){
	for (i = 0; i < NUM_OF_ALIENS; i++){
		if (alienAttacking[i]){
			stepSprite(aliens[i]);
			if (HIT WALL){
				alien.dx = 0;
				alien.dy = 0;
				prev_attack_time[i] = get_system_time();
				alienAttacking[i] = false;
			}//end if
		}//end if
	}//end for
}//end stepAliens


void alienAttackTimerCheck(void){
	for (i = 0; i < NUM_OF_ALIENS; i++){
		if (alienAttacking[i] == false){
			if (get_system_time() - prev_attack_time[i] > attackCountdown[i]){
				alienAttacking[i] = true;
				aimAlien(aliens[i]);
				prev_attack_time[i] = get_system_time();
				attackCountdown[i] = RAND BETWEEN 2 AND 4
			}//end if
		}//end if
	}//end for
}//end alienAttackTimerCheck

*/

/*
void stepAliens(void){
	for (int i = 0; i < NUM_OF_ALIENS; i++){
		//step
		aliens[i].x = aliens[i].x + aliens[i].dx;
		aliens[i].y = aliens[i].y + aliens[i].dy;
		
		//check if out of bounds
		if (aliens[i].x > 81 || aliens[i].x < 0 || aliens[i].y > 46 || aliens[i].y < 16){
			aliens[i].x = 0;
			aliens[i].y = -5;
			aliens[i].dx = 0;
			aliens[i].dy = 0;
			aliens[i].is_visible = false;
		}//end if
	}//end
}//end stepAlien
*/