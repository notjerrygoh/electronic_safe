// --- PIN DEFINITIONS ---
const int CLK_PIN = A4; // Connect CLK to A4
const int DIO_PIN = A5; // Connect DIO to A5
const int PIN_SPST_SWITCH = 8; // SPST Switch pin

// --- STATE TRACKING ---
bool systemON = false;

// Segment byte mappings for characters (0-9, O, N, E, T, F, S, P, I, space)
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

void setup() {
  Serial.begin(9600);
  pinMode(PIN_SPST_SWITCH, INPUT_PULLUP);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DIO_PIN, OUTPUT);
  displayClear();
}

void loop() {
  bool switchState = digitalRead(PIN_SPST_SWITCH);

  // --- SWITCHED TO THE RIGHT (ON) ---
  if (switchState == HIGH && !systemON) {
    systemON = true;
    
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
  }
}
