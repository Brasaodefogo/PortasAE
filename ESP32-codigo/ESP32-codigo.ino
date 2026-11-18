#include <Arduino.h>
#include <WiFi.h>

// ============================================================================
// PIN DEFINITIONS (Fill in the physical GPIO numbers from your PCB layout)
// ============================================================================

// Keypad Row Pins (Outputs)
const int ROW_1_PIN = 6; // TBD - Replace with actual GPIO
const int ROW_2_PIN = 20; // TBD - Replace with actual GPIO
const int ROW_3_PIN = 10; // TBD - Replace with actual GPIO
const int ROW_4_PIN = 8; // TBD - Replace with actual GPIO

// Keypad Column Pins (Inputs with internal Pull-Up)
const int COL_1_PIN = 7; // TBD - Replace with actual GPIO
const int COL_2_PIN = 5; // TBD - Replace with actual GPIO
const int COL_3_PIN = 9; // TBD - Replace with actual GPIO

// Actuator Output Pins
const int BUZZER_PIN        = 4; // TBD - Drives Q2 (2N2222)
const int ELECTRIC_STRIKE_PIN = 0; // TBD - Drives Q1 (IRLZ24N MOSFET)

// ============================================================================
// WI-FI & SYSTEM CONFIGURATION
// ============================================================================

const char* WIFI_SSID     = "YOUR_WIFI_SSID";     // TBD - Replace with your network name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // TBD - Replace with your network password

// Hardcoded 4-digit access code (First digit pressed = MSB)
const char CORRECT_CODE[5] = "1234"; // 4 characters + null terminator '\0'

// Keypad Matrix Mapping (4 Rows x 3 Columns)
const byte NUM_ROWS = 4;
const byte NUM_COLS = 3;

const int rowPins[NUM_ROWS] = {ROW_1_PIN, ROW_2_PIN, ROW_3_PIN, ROW_4_PIN};
const int colPins[NUM_COLS] = {COL_1_PIN, COL_2_PIN, COL_3_PIN};

const char keyMap[NUM_ROWS][NUM_COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Buffer to store the last 4 pressed keys (Index 0 is MSB, Index 3 is LSB)
char enteredCode[5] = {'-', '-', '-', '-', '\0'};
int keyCount = 0; // Tracks how many digits have been entered in the current sequence

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void initWiFi();
char scanKeypad();
void processKey(char key);
void verifyCode();
void triggerDoorUnlock();
void triggerErrorFeedback();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  delay(1000); // Short delay to allow Serial Monitor to connect on ESP32-C3 USB
  Serial.println("\n--- Smart Door Controller Booting ---");

  // Configure Actuator Pins as Digital Outputs and ensure they start OFF (LOW)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(ELECTRIC_STRIKE_PIN, OUTPUT);
  digitalWrite(ELECTRIC_STRIKE_PIN, LOW);

  // Configure Keypad Rows as Outputs (Idle state = HIGH)
  for (byte r = 0; r < NUM_ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }

  // Configure Keypad Columns as Inputs with Internal Pull-Up Resistors
  // When a button is pressed, the column pin is pulled LOW by the active row
  for (byte c = 0; c < NUM_COLS; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  // Connect to Wi-Fi
  initWiFi();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Continuously scan the keypad for a button press
  char pressedKey = scanKeypad();

  if (pressedKey != '\0') {
    processKey(pressedKey);
  }
}

// ============================================================================
// WI-FI INITIALIZATION
// ============================================================================

void initWiFi() {
  Serial.print("Connecting to Wi-Fi network: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA); // Station mode (connects to an existing router)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int maxRetries = 20; // Timeout after ~10 seconds (20 * 500ms)
  int retryCount = 0;

  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi Connection Successful!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi Connection Failed! Running in offline mode.");
  }
}

// ============================================================================
// KEYPAD MATRIX SCANNER (Row-by-Row Active-LOW Scanning)
// ============================================================================

char scanKeypad() {
  for (byte r = 0; r < NUM_ROWS; r++) {
    // Drive the current row LOW to test its buttons
    digitalWrite(rowPins[r], LOW);

    // Check every column in this active row
    for (byte c = 0; c < NUM_COLS; c++) {
      if (digitalRead(colPins[c]) == LOW) {
        // Debounce delay to filter out mechanical switch noise
        delay(25);

        // Confirm the button is still pressed after debounce
        if (digitalRead(colPins[c]) == LOW) {
          char detectedKey = keyMap[r][c];

          // Wait until the user releases the button (prevents multiple reads)
          while (digitalRead(colPins[c]) == LOW) {
            delay(10);
          }

          // Restore row to HIGH before returning
          digitalWrite(rowPins[r], HIGH);
          return detectedKey;
        }
      }
    }

    // Restore current row to HIGH before moving to the next row
    digitalWrite(rowPins[r], HIGH);
  }

  return '\0'; // No key pressed
}

// ============================================================================
// KEY BUFFER & CODE EVALUATION LOGIC
// ============================================================================

void processKey(char key) {
  Serial.print("Key Pressed: ");
  Serial.println(key);

  // Short feedback beep on every keypress
  digitalWrite(BUZZER_PIN, HIGH);
  delay(40);
  digitalWrite(BUZZER_PIN, LOW);

  // Store the key directly at the current index (0 = 1st/MSB ... 3 = 4th/LSB)
  enteredCode[keyCount] = key;
  keyCount++;

  // Once 4 keys have been entered, validate the code
  if (keyCount >= 4) {
    enteredCode[4] = '\0'; // Ensure string null-terminator is intact
    verifyCode();
    
    // Reset counter and clear buffer for the next attempt
    keyCount = 0;
    for (int i = 0; i < 4; i++) {
      enteredCode[i] = '-';
    }
  }
}

void verifyCode() {
  Serial.print("Evaluating Code: ");
  Serial.println(enteredCode);

  if (strcmp(enteredCode, CORRECT_CODE) == 0) {
    Serial.println("Correct Code, Opening Port");
    triggerDoorUnlock();
  } else {
    Serial.println("Error: Invalid Code! Access Denied.");
    triggerErrorFeedback();
  }
}

// ============================================================================
// ACTUATOR CONTROL FUNCTIONS
// ============================================================================

void triggerDoorUnlock() {
  // Activate Electric Strike and Buzzer for 2 seconds
  digitalWrite(ELECTRIC_STRIKE_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200); // Short confirmation beep
  digitalWrite(BUZZER_PIN, LOW);

  delay(1800); // Keep strike energized for the remaining time (total 2.0s)
  digitalWrite(ELECTRIC_STRIKE_PIN, LOW);
}

void triggerErrorFeedback() {
  // Three short error beeps on the buzzer
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}