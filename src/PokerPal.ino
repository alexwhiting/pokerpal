#include <AccelStepper.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>


// ============================================================
// Stepper Motor / A4988
// ============================================================

#define motorInterfaceType 1

const int dirPin = 2;
const int stepPin = 3;
const int sleepPin = 10;  // SLEEP on A4988
const int resetPin = 11;  // RST on A4988

AccelStepper stepper(motorInterfaceType, stepPin, dirPin);


// ============================================================
// LCD
// ============================================================

//20x4 I2C LCD used for user prompts and system feedback.

LiquidCrystal_I2C lcd(0x27, 20, 4);


// ============================================================
// TCS3200 Color Sensor
// ============================================================

#define S0 8
#define S1 7
#define S2 6
#define S3 5
#define sensorOut 4
#define LED 9


// ============================================================
// Keypad
// ============================================================

// 4x3 keypad used for entering chip values.

const byte ROWS = 4; // 4 rows
const byte COLS = 3; // 3 columns

// Keypad layout.
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// Keypad rows and columns connections. 
byte rowPins[ROWS] = {12, A0, A1, A2}; 
byte colPins[COLS] = {A3, A6, A7};  

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// ============================================================
// Chip Counters and Values
// ============================================================

int redCount = 0;
int blueCount = 0;
int greenCount = 0;
int whiteCount = 0;
int blackCount = 0;

int redValue = 0;
int blueValue = 0;
int greenValue = 0;
int whiteValue = 0;
int blackValue = 0;


// ============================================================
// Motion and Timing Settings
// ============================================================

const int SETTLE_TIME = 100;  // Settling time after reaching a bin (ms)
const int DETECTION_DELAY = 800; // Delay between color detection cycles (ms)


// ============================================================
// Sorting Positions
// ============================================================

// Motor positions for each color based on the mechanical
// layout of the sorting system.
// Values are converted to microsteps using 4x microstepping. 
const int BLACK_STEPS = 325 * 4;
const int GREEN_STEPS = -450 * 4;
const int WHITE_STEPS = 500 * 4;
const int BLUE_STEPS = 675 * 4;
const int RED_STEPS = 825 * 4;


// ============================================================
// Motor State Machine
// ============================================================

// Tracks the current stage of a sorting operation.
enum State { 
  P_IDLE,       // Waiting for a chip
  P_MOVING,     // Moving to the selected sorting position
  P_RETURNING   // Returning to the home position
};

State currentState = P_IDLE;

long startPosition = 0;
long targetPosition = 0;
unsigned long operationTimer = 0;
unsigned long lastDetectionTime = 0;

bool wasBackwardMove = false;
bool pausedForEmptying = false;
String colorToEmpty = "";


// ============================================================
// Motor Control
// ============================================================

// Enables or disables the A4988 stepper driver.
// The motor is disabled when idle to reduce unnecessary
// power consumption and motor heating.

void enableMotor(bool enable) {
  if (enable) {
    digitalWrite(sleepPin, HIGH);
    delay(5);  // Allow driver to wake up
    digitalWrite(resetPin, HIGH);
  } else {
    digitalWrite(resetPin, LOW);
    digitalWrite(sleepPin, LOW);
  }
}


// ============================================================
// Movement Control
// ============================================================

// Initiates a movement to the sorting position associated
// with the detected chip color.
void executeMovement(long steps) {
  enableMotor(true);

  currentState = P_MOVING;

  // Calculate the target position relative to the current
  // motor position.
  startPosition = stepper.currentPosition();
  targetPosition = startPosition + steps;
  wasBackwardMove = (steps < 0);
  
  Serial.print("Moving ");
  Serial.print(wasBackwardMove ? "BACKWARD " : "FORWARD ");
  Serial.println(abs(steps));
}


// ============================================================
// Color Sensor
// ============================================================

// Each function reads the TCS3200 output with its associated color
// filter selected and converts the measured pulse width to an
// intensity value.

int readRed() {
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  int pulse = pulseIn(sensorOut, LOW, 100000);
  return map(pulse, 25, 72, 255, 0);
}

int readGreen() {
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  int pulse = pulseIn(sensorOut, LOW, 100000);
  return map(pulse, 30, 90, 255, 0);
}

int readBlue() {
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  int pulse = pulseIn(sensorOut, LOW, 100000);
  return map(pulse, 25, 68, 255, 0);
}


// ============================================================
// LCD Display
// ============================================================

// Updates the LCD with the current chip counts, individual
// chip values, and total values of all sorted chips.
void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("R:");
  lcd.print(redCount);
  lcd.print("(");
  lcd.print(redValue);
  lcd.print(")");

  lcd.print(" G:"); 
  lcd.print(greenCount);
  lcd.print("("); 
  lcd.print(greenValue); 
  lcd.print(")");

  lcd.setCursor(0, 1);
  lcd.print("B:"); 
  lcd.print(blueCount);
  lcd.print("("); 
  lcd.print(blueValue);
  lcd.print(")");

  lcd.print(" W:"); 
  lcd.print(whiteCount);
  lcd.print("("); 
  lcd.print(whiteValue);
  lcd.print(")");

  lcd.setCursor(0, 2);
  lcd.print("Bl:"); 
  lcd.print(blackCount);
  lcd.print("("); 
  lcd.print(blackValue); 
  lcd.print(")");

  lcd.setCursor(0, 3);
  lcd.print("Total Value: "); 
  lcd.print(
    redCount * redValue + 
    greenCount * greenValue + 
    blueCount * blueValue + 
    whiteCount * whiteValue + 
    blackCount * blackValue
  );
}


// ============================================================
// User Interface
// ============================================================

// Displays the startup message when the system is powered on. 
void showWelcomeMessage() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Welcome to");

  lcd.setCursor(0, 1);
  lcd.print("PokerPal!");

  delay(2000);
}

// Displays a prompt for entering the monetary value of a
// particular chip color.
void promptForColorValue(const char* color) {
  lcd.clear();

  lcd.print("Enter ");
  lcd.print(color);
  lcd.print(" value:");

  lcd.setCursor(0, 1);
  lcd.print("> ");

  lcd.setCursor(0, 2);
  lcd.print("Clear : '*'");

  lcd.setCursor(0, 3);
  lcd.print("Continue: '#'");
}

// Collects a chip value from the keypad and returns the
// entered value when '#' is pressed.
// '*' clears the current input.
int getColorValue(const char* color) {
  String inputBuffer = "";

  promptForColorValue(color);
  
  while (true) {
    char key = keypad.getKey();
    
    if (key) {

      // Confirm the entered value.
      if (key == '#' && inputBuffer.length() > 0) {
        return inputBuffer.toInt();
      } 
      
      // Clear the current value.
      else if (key == '*' && inputBuffer.length() > 0) {

        inputBuffer = "";

        lcd.setCursor(2, 1);
        lcd.print("   ");

        lcd.setCursor(2, 1);
      } 
      
      // Add a numerical digit to the input (if less than 3 digits).
      else if (isDigit(key) && inputBuffer.length() < 3) {
        inputBuffer += key;

        lcd.setCursor(2, 1);
        lcd.print(inputBuffer);
      }
    }

    // Small delay to prevent unncessary busy-waiting.
    delay(10);
  }
}


// Prompts the user to enter the value of each chip color
// before the sorting process begins.
void setupColorValues() {
  redValue = getColorValue("RED");
  blueValue = getColorValue("BLUE");
  greenValue = getColorValue("GREEN");
  whiteValue = getColorValue("WHITE");
  blackValue = getColorValue("BLACK");
  
  lcd.clear();

  lcd.print("Values set!");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(1000);
}


// ============================================================
// Bin Management
// ============================================================

// Checks whether any bin has reached its 10-chip capacity.
// Pauses sorting and prompts the user to empty the full bin.
void checkForFullBins() {

  if (redCount > 0 && redCount % 10 == 0) {
    colorToEmpty = "RED";
    pausedForEmptying = true;
  } 
  
  else if (blueCount > 0 && blueCount % 10 == 0) {
    colorToEmpty = "BLUE";
    pausedForEmptying = true;
  } 
  
  else if (greenCount > 0 && greenCount % 10 == 0) {
    colorToEmpty = "GREEN";
    pausedForEmptying = true;
  } 
  
  else if (whiteCount > 0 && whiteCount % 10 == 0) {
    colorToEmpty = "WHITE";
    pausedForEmptying = true;
  } 
  
  else if (blackCount > 0 && blackCount % 10 == 0) {
    colorToEmpty = "BLACK";
    pausedForEmptying = true;
  }
  
  if (pausedForEmptying) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Please empty the");

    lcd.setCursor(0, 1);
    lcd.print(colorToEmpty + " bin");

    lcd.setCursor(0, 2);
    lcd.print("Press # when done");
    
    // Wait for the user to confirm that the bin has been emptied.
    while (true) {

      char key = keypad.getKey();

      if (key == '#') {

      // Reset the count for the bin that was emptied.
      if (colorToEmpty == "RED") {
        redCount = 0;
      }

      else if (colorToEmpty == "BLUE") {
        blueCount = 0;
      }

      else if (colorToEmpty == "GREEN") {
        greenCount = 0;
      }

      else if (colorToEmpty == "WHITE") {
        whiteCount = 0;
      }

      else if (colorToEmpty == "BLACK") {
        blackCount = 0;
      }

      pausedForEmptying = false;
      colorToEmpty = "";

      updateLCD();

      break;
    }

      delay(10);
    }
  }
}


// ============================================================
// Setup
// ============================================================

void setup() {
  
  // -------------------- Motor Setup --------------------

  pinMode(sleepPin, OUTPUT);
  pinMode(resetPin, OUTPUT);

  // Keep the motor disabled during startup.
  enableMotor(false);


  // -------------------- Color Sensor Setup --------------------
  
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  pinMode(sensorOut, INPUT);
  pinMode(LED, OUTPUT);
  
  // Configure the TCS3200 frequency scaling.
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  // Turn on the sensor illumination LED.
  digitalWrite(LED, HIGH);
  
  
  // -------------------- Serial Communication --------------------
  Serial.begin(9600);


  // -------------------- Stepper Configuration --------------------
  stepper.setMaxSpeed(7500);
  stepper.setAcceleration(4250);


  // -------------------- LCD Setup --------------------
  
  lcd.init();
  lcd.backlight();


  // -------------------- Startup --------------------

  showWelcomeMessage();
  
  // Wait for the user to press '#' before beginning setup.
  lcd.clear();
  lcd.print("Press # to start");

  while (keypad.getKey() != '#') {
    delay(10);
  }


  // -------------------- Chip Value Setup --------------------
  
  setupColorValues();
  

  // Initialize chip counters.
  redCount = 0;
  blueCount = 0;
  greenCount = 0;
  whiteCount = 0;
  blackCount = 0;
  
  updateLCD();
}


// ============================================================
// Main Program Loop
// ============================================================

void loop() {

  // Check for a full bin when the motor is idle.
  if (currentState == P_IDLE) {
    checkForFullBins();
  }

  // If the system is waiting for a bin to be emptied,
  // skip the rest of the loop.
  if (pausedForEmptying) {
    return;
  }

  
  // ==========================================================
  // Color Detection
  // ==========================================================

  // Only detect a new chip when the motor is idle and the
  // detection cooldown period has elapsed.
  if (
    currentState == P_IDLE && 
    millis() - lastDetectionTime > DETECTION_DELAY
  ) {

    int redFreq = readRed();
    int greenFreq = readGreen();
    int blueFreq = readBlue();
    
    Serial.print("RGB: ");
    Serial.print(redFreq);
    Serial.print(", ");
    Serial.print(greenFreq);
    Serial.print(", ");
    Serial.println(blueFreq);

    
    // Classify the detected chip using experimentally
    // determined RGB intensity thresholds.
    if (
      redFreq > 320 && 
      blueFreq > 200 && 
      blueFreq < 300 && 
      greenFreq > 200 && 
      greenFreq < 300
    ) {

      Serial.println("RED - Executing movement");

      executeMovement(RED_STEPS);

      redCount++;

      updateLCD();
    } 
    else if (
      blueFreq > 300 && 
      greenFreq < 300 &&
       redFreq > 200
    ) {

      Serial.println("BLUE - Executing movement");

      executeMovement(BLUE_STEPS);

      blueCount++;

      updateLCD();
    } 
    else if (
      redFreq > 290 && 
      redFreq < 320 && 
      greenFreq > 260 && 
      greenFreq < 330 && 
      blueFreq > 200 && 
      blueFreq < 280
    ) {

      Serial.println("GREEN - Executing movement");

      executeMovement(GREEN_STEPS);

      greenCount++;

      updateLCD();
    } 
    else if (
      redFreq > 300 && 
      blueFreq > 300 && 
      greenFreq > 300
    ) {

      Serial.println("WHITE - Executing movement");

      executeMovement(WHITE_STEPS);
      
      whiteCount++;

      updateLCD();
    } 
    else if (
      redFreq > 200 && 
      redFreq < 300 && 
      greenFreq > 100 && 
      greenFreq < 200 && 
      blueFreq > 100 && 
      blueFreq < 200
    ) {

      Serial.println("BLACK - Executing movement");

      executeMovement(BLACK_STEPS);

      blackCount++;

      updateLCD();
    }
    
    // Prevent another detection until the cooldown period
    // has elapsed.
    lastDetectionTime = millis();
  }


  // ==========================================================
  // Motor State Machine
  // ==========================================================

  // Handle the current movement state and transition between
  // moving to a bin, returning home, and waiting for the next chip.
  switch(currentState) {

    case P_MOVING:

      // Move towards the selected sorting position.
      stepper.moveTo(targetPosition);

      // Once target position is reached, begin the
      // return-to-home sequence.
      if (stepper.distanceToGo() == 0) {

        currentState = P_RETURNING;

        operationTimer = millis();

        // All sorting positions return to the home position.
        targetPosition = 0;
      }

      break;

    
    case P_RETURNING:

      // Allow the mechanism to settle before returning home.
      if (millis() - operationTimer > SETTLE_TIME) {

        stepper.moveTo(targetPosition);

        // Once the mechanism reaches home, return to idle.
        if (stepper.distanceToGo() == 0) {

          currentState = P_IDLE;

          // Disable the motor while waiting for the next chip.
          enableMotor(false);

          // Explicitly reset the software position to home.
          stepper.setCurrentPosition(0);

          Serial.println("Movement complete");
        }
      }

      break;

    
    case P_IDLE:

    default:

      // No motor movement is required while idle.
      break;
  }


  // Run the stepper motor continuously. 
  stepper.run();
}