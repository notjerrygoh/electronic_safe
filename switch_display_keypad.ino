// --- PIN DEFINITIONS ---
const int CLK_PIN = A4; // Connect CLK to A4
const int DIO_PIN = A5; // Connect DIO to A5
const int PIN_SPST_SWITCH = 8; // SPST Switch pin

// Keypad Row and Column Pins
const int PIN_ROW1 = 2;
const int PIN_ROW2 = 4;
const int PIN_ROW3 = 5;
const int PIN_ROW4 = 7;

const int PIN_COL1 = 10;
const int PIN_COL2 = 11;
const int PIN_COL3 = 12;
const int PIN_COL4 = 13;

const byte rowPins[4] = {PIN_ROW1, PIN_ROW2, PIN_ROW3, PIN_ROW4};
const byte colPins[4] = {PIN_COL1, PIN_COL2, PIN_COL3, PIN_COL4};

char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// --- PERSISTENT CORE STATES ---
bool hasPinSet = false;
bool safeLocked = true; 
char storedPin[5] = "";

// --- VOLATILE / WORKING STATES (Reset on power cycle) ---
char inputBuffer[5] = "    "; 
int bufferIndex = 0;          

// Segment byte mappings for characters
const uint8_t charTable[] = {
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F  // 9
};

uint8_t getSegment(char c) {
  if (c >= '0' && c <= '9') return charTable[c - '0'];
  switch (c) {
    case 'O': case 'o': return 0x3F;
    case 'N': case 'n': return 0x54;
    case 'E': case 'e': return 0x79;
    case 'F': case 'f': return 0x71;
    case 'T': case 't': return 0x78;
    case 'S': case 's': return 0x6D;
    case 'P': case 'p': return 0x73;
    case 'I': case 'i': return 0x06;
    case 'R': case 'r': return 0x50;
    case 'D': case 'd': return 0x5E;
    case 'L': case 'l': return 0x38;
    case 'C': case 'c': return 0x39;
    case 'K': case 'k': return 0x70;
    case 'U': case 'u': return 0x3E;
    case ' ':           return 0x00;
    default:            return 0x00;
  }
}

// Low-level TM1637 protocol functions
void tmStart() {
  pinMode(DIO_PIN, OUTPUT);
  delayMicroseconds(2);
}

void tmStop() {
  pinMode(DIO_PIN, OUTPUT);
  delayMicroseconds(2);
  pinMode(CLK_PIN, INPUT); 
  delayMicroseconds(2);
  pinMode(DIO_PIN, INPUT); 
  delayMicroseconds(2);
}

bool tmWriteByte(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(CLK_PIN, OUTPUT);
    delayMicroseconds(2);
    if (b & 1) pinMode(DIO_PIN, INPUT);
    else {
      pinMode(DIO_PIN, OUTPUT);
      digitalWrite(DIO_PIN, LOW);
    }
    delayMicroseconds(2);
    pinMode(CLK_PIN, INPUT);
    delayMicroseconds(2);
    b >>= 1;
  }
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DIO_PIN, INPUT);
  delayMicroseconds(2);
  pinMode(CLK_PIN, INPUT);
  delayMicroseconds(2);
  bool ack = (digitalRead(DIO_PIN) == LOW);
  pinMode(CLK_PIN, OUTPUT);
  delayMicroseconds(2);
  return ack;
}

void displayString(const char* str) {
  tmStart();
  tmWriteByte(0x40); // Auto increment mode
  tmStop();

  tmStart();
  tmWriteByte(0xC0); // Start address 00h
  for (int i = 0; i < 4; i++) {
    tmWriteByte(getSegment(str[i]));
  }
  tmStop();

  tmStart();
  tmWriteByte(0x88 | 0x07); // Turn display ON, max brightness
  tmStop();
}

void displayClear() {
  displayString("    ");
}

// Check switch status and handle power-off interruption immediately
bool checkPowerSwitch() {
  if (digitalRead(PIN_SPST_SWITCH) == LOW) {
    displayString("OFF ");
    delay(1000);
    displayClear();
    
    // Reset active input buffers/halfway operations on power down
    bufferIndex = 0;
    strcpy(inputBuffer, "    ");
    return false; 
  }
  return true; 
}

// Smooth single-pass scroller (~350ms per frame)
bool showMessageOnce(const char* msg) {
  String padded = "    " + String(msg) + "    ";
  int maxIdx = padded.length() - 4;
  
  for (int i = 0; i <= maxIdx; i++) {
    if (!checkPowerSwitch()) return false;
    
    char sub[5];
    padded.substring(i, i + 4).toCharArray(sub, 5);
    displayString(sub);
    
    unsigned long frameStart = millis();
    while (millis() - frameStart < 350) {
      if (!checkPowerSwitch()) return false;
      delay(10);
    }
  }
  return true;
}

// --- KEYPAD MATRIX SCANNER ---
char scanKeypad() {
  for (int r = 0; r < 4; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(10);

    for (int c = 0; c < 4; c++) {
      if (digitalRead(colPins[c]) == LOW) {
        char key = keys[r][c];
        while (digitalRead(colPins[c]) == LOW) {
          if (!checkPowerSwitch()) return '\0';
          delay(10);
        }
        pinMode(rowPins[r], INPUT);
        delay(20); // Debounce
        return key;
      }
    }
    pinMode(rowPins[r], INPUT);
  }
  return '\0';
}

// Helper function to collect 4 digits with Backspace ('B'), Clear ('C'), and Cancel ('A')
char collectPinDigits(bool allowCancel = false) {
  bufferIndex = 0;
  strcpy(inputBuffer, "    ");
  displayString(inputBuffer);

  while (true) {
    if (!checkPowerSwitch()) return '\0';

    char key = scanKeypad();
    if (key != '\0') {
      if (key >= '0' && key <= '9') {
        if (bufferIndex < 4) {
          inputBuffer[bufferIndex] = key;
          bufferIndex++;
          inputBuffer[4] = '\0';
          displayString(inputBuffer);
        }
      }
      else if (key == 'B') { // Backspace
        if (bufferIndex > 0) {
          bufferIndex--;
          inputBuffer[bufferIndex] = ' ';
          inputBuffer[4] = '\0';
          displayString(inputBuffer);
        }
      }
      else if (key == 'C') { // Clear
        bufferIndex = 0;
        strcpy(inputBuffer, "    ");
        displayString(inputBuffer);
      }
      else if (key == 'D' && bufferIndex == 4) {
        return 'D'; // Successfully entered 4 digits and pressed Enter
      }
      else if (allowCancel && key == 'A') {
        return 'A'; // Only 'A' cancels
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_SPST_SWITCH, INPUT_PULLUP);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DIO_PIN, OUTPUT);
  displayClear();

  for (int r = 0; r < 4; r++) {
    pinMode(rowPins[r], INPUT);
  }
  for (int c = 0; c < 4; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }
}

void loop() {
  bool switchState = digitalRead(PIN_SPST_SWITCH);

  // --- WAIT UNTIL SWITCHED ON (HIGH) ---
  if (switchState == HIGH) {
    displayString("ON  ");
    delay(1000);
    if (!checkPowerSwitch()) return;
    
    // Check if initial password has ever been set
    if (!hasPinSet) {
      if (!showMessageOnce("SET PIN")) return;
      displayClear();

      while (collectPinDigits(false) != 'D') {
        if (!checkPowerSwitch()) return;
      }

      for (int i = 0; i < 4; i++) {
        storedPin[i] = inputBuffer[i];
      }
      storedPin[4] = '\0';
      hasPinSet = true;
      safeLocked = true; // Default locked after initial setup

      if (!showMessageOnce("PIN STORED")) return;
      displayClear();
    }

    // --- MAIN OPERATION LOOP ---
    while (true) {
      if (!checkPowerSwitch()) return;

      if (safeLocked) {
        if (!showMessageOnce("ENTER PIN")) return;
        displayClear();

        char result = collectPinDigits(false);
        if (result == 'D') {
          inputBuffer[4] = '\0';
          if (strcmp(inputBuffer, storedPin) == 0) {
            safeLocked = false;
            showMessageOnce("UNLOCKED");
          } else {
            safeLocked = true;
            if (!showMessageOnce("INCORRECT PIN")) return;
            displayClear();
          }
        }
      } 
      else {
        // Safe is UNLOCKED. Display status and listen for '#' (Lock) or '*' (Reset Password)
        displayString("UNLK");
        
        char key = scanKeypad();
        if (key == '#') {
          safeLocked = true;
          showMessageOnce("LOCKED");
          displayClear();
        }
        else if (key == '*') {
          // Reset Password Flow (Triggered ONLY by *)
          bool resetSuccess = false;
          
          while (!resetSuccess) {
            if (!showMessageOnce("ENTER PIN")) return;
            displayClear();

            char res = collectPinDigits(true); // Allow 'A' to cancel here
            if (res == 'A') {
              break; // Cancel reset
            }
            
            if (res == 'D') {
              inputBuffer[4] = '\0';
              if (strcmp(inputBuffer, storedPin) == 0) {
                // Current password correct -> Prompt to set new pin
                if (!showMessageOnce("SET PIN")) return;
                displayClear();

                char newPinRes = collectPinDigits(true); // Allow 'A' to cancel new pin entry
                if (newPinRes == 'A') {
                  break; // Cancelled new pin setting
                }
                while (newPinRes != 'D') {
                  if (!checkPowerSwitch()) return;
                  if (newPinRes == 'A') break;
                  newPinRes = collectPinDigits(true);
                }
                if (newPinRes == 'A') break; // Cancelled

                for (int i = 0; i < 4; i++) {
                  storedPin[i] = inputBuffer[i];
                }
                storedPin[4] = '\0';

                if (!showMessageOnce("PIN STORED")) return;
                displayClear();
                resetSuccess = true;
              } else {
                // Incorrect current password -> loops back to ask again until 'A' is pressed
                if (!showMessageOnce("INCORRECT PIN")) return;
                displayClear();
              }
            }
          }
        }
      }
    }
  }
  
  // --- SWITCHED OFF (LOW) ---
  else {
    displayClear();
    // Wait here while switch is OFF
    while (digitalRead(PIN_SPST_SWITCH) == LOW) {
      delay(50);
    }
    // Small debounce delay before turning back on
    delay(100);
  }
}
