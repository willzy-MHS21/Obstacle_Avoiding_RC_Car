#include <AFMotor.h>
#include <NewPing.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin definitions
#define TRIG_PIN A1	// Ultrasonic sensor trigger pin
#define ECHO_PIN A0	// Ultrasonic sensor echo pin
#define BUZZER_PIN A2	// Buzzer 1
#define BUZZER2_PIN A3 // Buzzer 2
#define MAX_DISTANCE 200 // Maximum distance to ping (in cm)
#define MAX_SPEED 255	// Maximum motor speed (0-255)

// Initialize hardware components
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD
NewPing radar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE); // Ultrasonic sensor object
AF_DCMotor motor1(1); // DC motors
AF_DCMotor motor2(2);
AF_DCMotor motor3(3);
AF_DCMotor motor4(4);
Servo myservo; // Servo

// Global variables
int distance = 100;	// Current distance
int pos = 0;	// Current servo position
bool autoMode = false;	// Start in MANUAL mode for keyboard control
bool isMovingForward = false;	// Track if car is moving forward
bool obstacleDetected = false;	// Track obstacle state
unsigned long lastBeepTime = 0; // For beep timing
char command;	// Latest serial command

void setup()
{
	// Initialize two buzzers
	pinMode(BUZZER_PIN, OUTPUT);
	pinMode(BUZZER2_PIN, OUTPUT);
	digitalWrite(BUZZER_PIN, LOW);
	digitalWrite(BUZZER2_PIN, LOW);

	// Initialize LCD display
	lcd.init();
	lcd.backlight();
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Obstacle Avoid");
	lcd.setCursor(0, 1);
	lcd.print("Car Starting...");

	// Startup beep pattern with both buzzers
	digitalWrite(BUZZER_PIN, HIGH);
	digitalWrite(BUZZER2_PIN, HIGH);
	delay(200);
	digitalWrite(BUZZER_PIN, LOW);
	digitalWrite(BUZZER2_PIN, LOW);
	delay(100);
	digitalWrite(BUZZER_PIN, HIGH);
	digitalWrite(BUZZER2_PIN, HIGH);
	delay(200);
	digitalWrite(BUZZER_PIN, LOW);
	digitalWrite(BUZZER2_PIN, LOW);
	delay(1500);

	// Set all motors speed to 200
	motor1.setSpeed(200);
	motor2.setSpeed(200);
	motor3.setSpeed(200);
	motor4.setSpeed(200);

	// Attach servo to pin 9 and center it
	myservo.attach(9);
	myservo.write(100);
	delay(2000);

	// Initialize serial communication at 9600 baud for Python control
	Serial.begin(9600);

	// Display ready message
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Mode: MANUAL");
	lcd.setCursor(0, 1);
	lcd.print("Python Ready!");
}

void loop()
{
	// Check for serial commands from Python
	while (Serial.available() > 0)
	{
		command = Serial.read();
		handleCommand(command);
	}

	// Only run autonomous mode if still in auto mode
	if (autoMode)
	{
		autonomousDrive();
	}
	else
	{
		// In manual mode, continuously check distance for safety
		checkManualModeSafety();
	}
}

/*
 * Handle incoming serial commands from keyboard/Python
 * Commands: w=Forward, s=Backward, a=Left, d=Right,
 *           x=Stop, m=Auto Mode, h=Horn
 */
void handleCommand(char cmd)
{
	switch (cmd)
	{
	case 'W': // Forward
	case 'w':
		autoMode = false;
		isMovingForward = true;

		// Check for obstacle before moving
		distance = checkDistance();
		if (distance <= 15)
		{
			obstacleDetected = true;
			stopMovement();
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("BLOCKED!");
			lcd.setCursor(0, 1);
			lcd.print("Obstacle: ");
			lcd.print(distance);
			lcd.print("cm");
		}
		else
		{
			obstacleDetected = false;
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Mode: MANUAL");
			lcd.setCursor(0, 1);
			lcd.print("Forward");
			moveForward();
		}
		break;

	case 'S': // Backward
	case 's':
		autoMode = false;
		isMovingForward = false;
		obstacleDetected = false;
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Mode: MANUAL");
		lcd.setCursor(0, 1);
		lcd.print("Backward");
		moveBackward();
		break;

	case 'A': // Left
	case 'a':
		autoMode = false;
		isMovingForward = false;
		obstacleDetected = false;
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Mode: MANUAL");
		lcd.setCursor(0, 1);
		lcd.print("Left");
		manualTurnLeft();
		break;

	case 'D': // Right
	case 'd':
		autoMode = false;
		isMovingForward = false;
		obstacleDetected = false;
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Mode: MANUAL");
		lcd.setCursor(0, 1);
		lcd.print("Right");
		manualTurnRight();
		break;

	case 'X': // Stop
	case 'x':
		autoMode = false;
		isMovingForward = false;
		obstacleDetected = false;
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Mode: MANUAL");
		lcd.setCursor(0, 1);
		lcd.print("Stopped");
		stopMovement();
		break;

	case 'M': // Toggle to Auto Mode
	case 'm':
		autoMode = true;
		isMovingForward = false;
		obstacleDetected = false;
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Mode: AUTO");
		myservo.write(100); // Reset servo
		distance = checkDistance();
		break;

	case 'H': // Horn/Buzzer
	case 'h':
		digitalWrite(BUZZER_PIN, HIGH);
		digitalWrite(BUZZER2_PIN, HIGH);
		delay(300);
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);
		break;

	default:
		// Ignore other commands
		break;
	}
}

/*
 * Autonomous driving mode with obstacle avoidance
 * Car moves forward until obstacle detected, then scans and turns
 */
void autonomousDrive()
{
	int distanceRight = 0;
	int distanceLeft = 0;
	delay(10);

	// Check if obstacle is too close (15cm threshold)
	if (distance <= 15)
	{
		// Alert with both buzzers
		digitalWrite(BUZZER_PIN, HIGH);
		digitalWrite(BUZZER2_PIN, HIGH);

		// LCD display for obstacle detecting
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Obstacle!");
		lcd.setCursor(0, 1);
		lcd.print("Dist: ");
		lcd.print(distance);
		lcd.print(" cm");

		// Turn off buzzer
		delay(300);
		digitalWrite(BUZZER_PIN, LOW);
		digitalWrite(BUZZER2_PIN, LOW);

		// Stop
		stopMovement();
		delay(100);

		// LCD display for moving backward
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Moving");
		lcd.setCursor(0, 1);
		lcd.print("Backward");

		// Move backward for 1 sec then stop
		moveBackward();
		delay(1000);
		stopMovement();

		motor1.setSpeed(200);
		motor2.setSpeed(200);
		motor3.setSpeed(200);
		motor4.setSpeed(200);

		delay(200);

		// Scan right side with LCD display and calculate the right side distance
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Scanning");
		lcd.setCursor(0, 1);
		lcd.print("Right...");

		distanceRight = checkRightDistance();
		delay(200);

		// Scan left side with LCD display and calculate the left side distance
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Scanning");
		lcd.setCursor(0, 1);
		lcd.print("Left...");

		distanceLeft = checkLeftDistance();
		delay(200);

		// LCD display left side distance and right side distance
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("L:");
		lcd.print(distanceLeft);
		lcd.print("cm R:");
		lcd.print(distanceRight);
		lcd.print("cm");
		delay(500);

		// Turn toward the direction with more clearance
		if (distanceRight >= distanceLeft)
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Turning");
			lcd.setCursor(0, 1);
			lcd.print("Right");

			turnRight();
			stopMovement();
		}
		else
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Turning");
			lcd.setCursor(0, 1);
			lcd.print("Left");

			turnLeft();
			stopMovement();
		}
		delay(500);
	}
	else
	{
		// Path is clear - continue moving forward
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Moving Forward");
		lcd.setCursor(0, 1);
		lcd.print("Clear: ");
		lcd.print(distance);
		lcd.print(" cm");

		moveForward();
	}

	// Update distance reading for next iteration
	distance = checkDistance();
}

/*
 * Scan right side by rotating servo from center (100°) to right (30°)
 * Returns the distance measurement at the rightmost position
 */
int checkRightDistance()
{
	// Sweep servo to the right
	for (pos = 100; pos >= 30; pos -= 1)
	{
		myservo.write(pos);
		delay(5);
	}
	myservo.write(30);
	delay(500);
	int distance = checkDistance(); // Take measurement
	delay(100);

	// Return servo to center position
	for (pos = 30; pos <= 100; pos += 1)
	{
		myservo.write(pos);
		delay(5);
	}
	myservo.write(100);

	return distance;
}

/*
 * Scan left side by rotating servo from center (100°) to left (170°)
 * Returns the distance measurement at the leftmost position
 */
int checkLeftDistance()
{
	// Sweep servo to the left
	for (pos = 100; pos <= 170; pos += 1)
	{
		myservo.write(pos);
		delay(5);
	}
	myservo.write(170);
	delay(500);
	int distance = checkDistance(); // Take measurement
	delay(100);

	// Return servo to center position
	for (pos = 170; pos >= 100; pos -= 1)
	{
		myservo.write(pos);
		delay(5);
	}
	myservo.write(100);

	return distance;
}

/*
 * Helper function: Get distance reading from ultrasonic sensor
 * Returns distance in centimeters (or 250 if out of range)
 */
int checkDistance()
{
	delay(70);
	int cm = radar.ping_cm();
	if (cm == 0)
	{
		cm = 250;
	}
	return cm;
}

// Stop all four motors
void stopMovement()
{
	motor1.run(RELEASE);
	motor2.run(RELEASE);
	motor3.run(RELEASE);
	motor4.run(RELEASE);
}

// Move all motors forward (car moves forward)
void moveForward()
{
	motor1.run(FORWARD);
	motor2.run(FORWARD);
	motor3.run(FORWARD);
	motor4.run(FORWARD);
}

// Move all motors backward at maximum speed (car reverses)
void moveBackward()
{
	motor1.setSpeed(255);
	motor2.setSpeed(255);
	motor3.setSpeed(255);
	motor4.setSpeed(255);

	motor1.run(BACKWARD);
	motor2.run(BACKWARD);
	motor3.run(BACKWARD);
	motor4.run(BACKWARD);
}

// Manual turning - continuous movement until stopped
void manualTurnLeft()
{
	motor1.run(FORWARD);
	motor2.run(FORWARD);
	motor3.run(BACKWARD);
	motor4.run(BACKWARD);
}

void manualTurnRight()
{
	motor1.run(BACKWARD);
	motor2.run(BACKWARD);
	motor3.run(FORWARD);
	motor4.run(FORWARD);
}

// Auto mode turning - timed turn (1 sec)
void turnLeft()
{
	motor1.setSpeed(255);
	motor2.setSpeed(255);
	motor3.setSpeed(255);
	motor4.setSpeed(255);

	motor1.run(FORWARD);
	motor2.run(FORWARD);
	motor3.run(BACKWARD);
	motor4.run(BACKWARD);
	delay(1000);

	stopMovement();

	motor1.setSpeed(200);
	motor2.setSpeed(200);
	motor3.setSpeed(200);
	motor4.setSpeed(200);
}

void turnRight()
{
	motor1.setSpeed(255);
	motor2.setSpeed(255);
	motor3.setSpeed(255);
	motor4.setSpeed(255);

	motor1.run(BACKWARD);
	motor2.run(BACKWARD);
	motor3.run(FORWARD);
	motor4.run(FORWARD);
	delay(1000);

	stopMovement();

	motor1.setSpeed(200);
	motor2.setSpeed(200);
	motor3.setSpeed(200);
	motor4.setSpeed(200);
}

/*
 * Safety system for manual mode
 * Continuously monitors distance when moving forward
 * Stops car and sounds alarm if obstacle detected
 */
void checkManualModeSafety()
{
	// Check if user is trying to move forward
	if (isMovingForward)
	{
		distance = checkDistance();

		if (distance <= 15)
		{
			// Obstacle detected - stop car and beep
			if (!obstacleDetected)
			{
				obstacleDetected = true;
				stopMovement();
				lcd.clear();
				lcd.setCursor(0, 0);
				lcd.print("BLOCKED!");
				lcd.setCursor(0, 1);
				lcd.print("Obstacle: ");
				lcd.print(distance);
				lcd.print("cm");
			}

			// Continuous beeping pattern (beep every 500ms)
			unsigned long currentTime = millis();
			if (currentTime - lastBeepTime >= 500)
			{
				digitalWrite(BUZZER_PIN, HIGH);
				digitalWrite(BUZZER2_PIN, HIGH);
				delay(200);
				digitalWrite(BUZZER_PIN, LOW);
				digitalWrite(BUZZER2_PIN, LOW);
				lastBeepTime = currentTime;
			}

			// Update LCD with current distance
			lcd.setCursor(10, 1);
			lcd.print("   "); // Clear old number
			lcd.setCursor(10, 1);
			lcd.print(distance);
			lcd.print("cm");
		}
		else
		{
			// Path is clear - allow movement
			if (obstacleDetected)
			{
				// Obstacle cleared - stop beeping and allow forward
				obstacleDetected = false;
				digitalWrite(BUZZER_PIN, LOW);
				digitalWrite(BUZZER2_PIN, LOW);
				lcd.clear();
				lcd.setCursor(0, 0);
				lcd.print("Mode: MANUAL");
				lcd.setCursor(0, 1);
				lcd.print("Path Clear!");
				delay(500);

				// Resume forward movement
				lcd.clear();
				lcd.setCursor(0, 0);
				lcd.print("Mode: MANUAL");
				lcd.setCursor(0, 1);
				lcd.print("Forward");
				moveForward();
			}
		}
	}
}