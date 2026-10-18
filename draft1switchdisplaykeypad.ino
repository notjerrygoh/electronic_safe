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

// --- STATE TRACKING ---
bool systemON = false;
bool hasPinSet = false;

char storedPin[5] = "";
char inputBuffer[5] = "    "; 
int bufferIndex = 0;          

// Segment byte mappings for characters (0-9, O, N, E, T, F, S, P, I, space, R, D)
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
    case ' ':           return 0x00;
    default:            return 0x00;
  }
}

// Low-level TM1637 protocol functions (No library needed!)
void tmStart() {
  pinMode(DIO_PIN, OUTPUT);
  delayMicroseconds(2);
}

void tmStop() {
  pinMode(DIO_PIN, OUTPUT);
  delayMicroseconds(2);
  pinMode(CLK_PIN, INPUT); // CLK High
  delayMicroseconds(2);
  pinMode(DIO_PIN, INPUT); // DIO High
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

// Scrolls "SET PIN" across the 4-digit display for 2 seconds
void showSetPin2Seconds() {
  const char* frames[] = {
    "   S",
    "  SE",
    " SET",
    "SET ",
    "ET P",
    "T PI",
    " PIN",
    "PIN ",
    "IN  ",
    "N   "
  };
  for (int i = 0; i < 10; i++) {
    displayString(frames[i]);
    delay(200); // 10 frames * 200ms = 2000ms (2 seconds)
  }
  displayClear();
}

// Scrolls "PIN STORED" across the screen for 2 seconds
void showPinStored2Seconds() {
  const char* frames[] = {
    "   P",
    "  PI",
    " PIN",
    "PIN ",
    "IN S",
    "N ST",
    " STO",
    "STOR",
    "TORE",
    "ORED"
  };
  for (int i = 0; i < 10; i++) {
    displayString(frames[i]);
    delay(200); // 10 frames * 200ms = 2000ms (2 seconds)
  }
  displayClear();
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
          delay(10); // Wait release
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

void setup() {
  Serial.begin(9600);
  pinMode(PIN_SPST_SWITCH, INPUT_PULLUP);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DIO_PIN, OUTPUT);
  displayClear();

  // Setup Keypad Pins
  for (int r = 0; r < 4; r++) {
    pinMode(rowPins[r], INPUT);
  }
  for (int c = 0; c < 4; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }
}

void loop() {
  bool switchState = digitalRead(PIN_SPST_SWITCH);

  // --- SWITCHED TO THE RIGHT (ON) ---
  if (switchState == HIGH && !systemON) {
    systemON = true;
    bufferIndex = 0;
    strcpy(inputBuffer, "    ");
    hasPinSet = false;
    
    displayString("ON  ");
    delay(2000);
    
    showSetPin2Seconds();
  }
  
  // --- SWITCHED TO THE LEFT (OFF) ---
  else if (switchState == LOW && systemON) {
    displayString("OFF ");
    delay(2000);
    
    displayClear();
    systemON = false;
    hasPinSet = false;
    bufferIndex = 0;
  }

  // If system is ON and PIN has already been set/stored, do nothing or remain idle
  if (!systemON || hasPinSet) {
    return;
  }

  // Handle Keypad Input
  char key = scanKeypad();
  if (key == '\0') return;

  // 1. Type numbers 0-9
  if (key >= '0' && key <= '9') {
    if (bufferIndex < 4) {
      inputBuffer[bufferIndex] = key;
      bufferIndex++;
      displayString(inputBuffer);
    }
  }
  // 2. 'B' Key = Backspace (remove previous digit)
  else if (key == 'B') {
    if (bufferIndex > 0) {
      bufferIndex--;
      inputBuffer[bufferIndex] = ' ';
      displayString(inputBuffer);
    }
  }
  // 3. 'C' Key = Clear input buffer completely
  else if (key == 'C') {
    bufferIndex = 0;
    strcpy(inputBuffer, "    ");
    displayString(inputBuffer);
  }
  // 4. 'D' Key = Enter (only works when all 4 digits are keyed)
  else if (key == 'D') {
    if (bufferIndex == 4) {
      for (int i = 0; i < 4; i++) {
        storedPin[i] = inputBuffer[i];
      }
      storedPin[4] = '\0';
      hasPinSet = true;

      // Show "PIN STORED" for 2 seconds, then clear (no permanent pin display)
      showPinStored2Seconds();
    }
  }
}
