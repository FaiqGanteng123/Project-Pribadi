#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SOIL_PIN    A0
#define LDR_PIN     A2
#define TEMP_PIN    A3
#define BUTTON_PIN  2

#define RELAY_PUMP  5
#define RELAY_LAMP  6

#define SOIL_DRY_ON   550
#define SOIL_WET_OFF  650
#define LDR_DARK_ON   350
#define LDR_BRIGHT_OFF 450
#define TEMP_HOT_THRESHOLD 32.0

#define RELAY_ON  HIGH
#define RELAY_OFF LOW

#define SENSOR_INTERVAL 500
#define DEBOUNCE_DELAY  50
#define HASH_SAVE_INTERVAL 5000
#define PAGE_AUTO_ROTATE_INTERVAL 1000 

#define HASH_TABLE_SIZE 10
#define MAX_NODES 30

struct SensorNode {
  int key;
  int soil;
  int ldr;
  float temp;
  unsigned long timestamp;
  int nextIndex;
  bool used;
};

SensorNode hashNodes[MAX_NODES];
int hashTable[HASH_TABLE_SIZE];
int nodeCount = 0;


unsigned long lastSensorRead = 0;
unsigned long lastHashSave = 0;
int currentPage = 0;

int soil = 0;
int ldr  = 0;
float tempC = 0;

bool pumpOn = false;
bool lampOn = false;
unsigned long lastPageRotate = 0;

int lastButtonState = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;

void initHashTable() {
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    hashTable[i] = -1;
  }
  
  for (int i = 0; i < MAX_NODES; i++) {
    hashNodes[i].used = false;
    hashNodes[i].nextIndex = -1;
  }
  
  nodeCount = 0;
}

int hashFunction(int key) {
  return key % HASH_TABLE_SIZE;
}

int allocateNode() {
  for (int i = 0; i < MAX_NODES; i++) {
    if (!hashNodes[i].used) {
      hashNodes[i].used = true;
      hashNodes[i].nextIndex = -1;
      return i;
    }
  }
  return -1;
}

void hashInsert(int key, int soilVal, int ldrVal, float tempVal) {
  int newIndex = allocateNode();
  if (newIndex == -1) {
    Serial.println(F("HASH TABLE FULL!"));
    return;
  }
  
  hashNodes[newIndex].key = key;
  hashNodes[newIndex].soil = soilVal;
  hashNodes[newIndex].ldr = ldrVal;
  hashNodes[newIndex].temp = tempVal;
  hashNodes[newIndex].timestamp = millis();
  
  int hashIndex = hashFunction(key);
  
  bool collision = false;
  if (hashTable[hashIndex] != -1) {
    collision = true;
    int current = hashTable[hashIndex];
    while (hashNodes[current].nextIndex != -1) {
      current = hashNodes[current].nextIndex;
    }
    hashNodes[current].nextIndex = newIndex;
  } else {
    hashTable[hashIndex] = newIndex;
  }
  
  nodeCount++;
  
  Serial.println(F("=================================="));
  Serial.println(F("= HASH TABLE: NEW NODE INSERTED  ="));
  Serial.println(F("=================================="));
  Serial.print(F("= Key:        ")); Serial.println(key);
  Serial.print(F("= Hash Index: ")); Serial.println(hashIndex);
  Serial.print(F("= Collision:  ")); Serial.println(collision ? F("YES") : F("NO"));
  Serial.print(F("= Soil:       ")); Serial.println(soilVal);
  Serial.print(F("= LDR:        ")); Serial.println(ldrVal);
  Serial.print(F("= Temp:       ")); Serial.print(tempVal, 1); Serial.println(F(" C"));
  Serial.print(F("= Time:       ")); Serial.print(millis()/1000); Serial.println(F("s"));
  Serial.print(F("= Total Nodes: ")); Serial.println(nodeCount);
  Serial.println(F("=================================="));
}

int hashSearch(int key) {
  int hashIndex = hashFunction(key);
  int current = hashTable[hashIndex];
  
  while (current != -1) {
    if (hashNodes[current].key == key) {
      return current; 
    }
    current = hashNodes[current].nextIndex;
  }
  
  return -1; 
}

void printHashTable() {
  Serial.println(F("\n=================================="));
  Serial.println(F("=  HASH TABLE STRUCTURE          ="));
  Serial.println(F("=================================="));
  Serial.print(F("= Total Nodes: ")); Serial.println(nodeCount);
  Serial.print(F("= Table Size:  ")); Serial.println(HASH_TABLE_SIZE);
  Serial.println(F("=================================="));
  
  int totalCollisions = 0;
  
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    Serial.print(F("Bucket [")); 
    Serial.print(i); 
    Serial.print(F("]: "));
    
    if (hashTable[i] == -1) {
      Serial.println(F("(empty)"));
    } else {
      int current = hashTable[i];
      int chainLength = 0;
      
      while (current != -1) {
        if (chainLength > 0) {
          Serial.print(F(" -> "));
          totalCollisions++;
        }
        
        Serial.print(F("[Key: "));
        Serial.print(hashNodes[current].key);
        Serial.print(F(", Soil: "));
        Serial.print(hashNodes[current].soil);
        Serial.print(F(", LDR: "));
        Serial.print(hashNodes[current].ldr);
        Serial.print(F(", Temp: "));
        Serial.print(hashNodes[current].temp, 1);
        Serial.print(F("C]"));
        
        current = hashNodes[current].nextIndex;
        chainLength++;
      }
      Serial.println();
    }
  }
  
  Serial.print(F("\n= Total Collisions: "));
  Serial.println(totalCollisions);
  Serial.print(F("= Load Factor: "));
  Serial.print((float)nodeCount / HASH_TABLE_SIZE, 2);
  Serial.println(F("\n================\n"));
}

void setup() {
  Serial.begin(9600);

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_LAMP, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PUMP, RELAY_OFF);
  digitalWrite(RELAY_LAMP, RELAY_OFF);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Smart Farming");
  lcd.setCursor(0,1);
  lcd.print("with Hash Table");
  delay(1500);
  lcd.clear();

  initHashTable();

  Serial.println(F("\n=================================="));
  Serial.println(F("=  SMART FARMING SYSTEM v2.0     ="));
  Serial.println(F("=  with Hash Table (Chaining)    ="));
  Serial.println(F("=================================="));
  Serial.println(F("System started. Data saved every 5s."));
  Serial.println(F("\n[RELAY CONFIG]"));
  Serial.println(F("Relay Type: ACTIVE HIGH"));
  Serial.println(F("HIGH = ON, LOW = OFF"));
  
  Serial.println(F("\n[RELAY TEST]"));
  Serial.println(F("Testing Pump Relay..."));
  digitalWrite(RELAY_PUMP, HIGH);
  delay(500);
  digitalWrite(RELAY_PUMP, LOW);
  Serial.println(F("Testing Lamp Relay..."));
  digitalWrite(RELAY_LAMP, HIGH);
  delay(500);
  digitalWrite(RELAY_LAMP, LOW);
  Serial.println(F("Relay test complete!"));
  
  Serial.println(F("\n[THRESHOLDS]"));
  Serial.println(F("SOIL: <550 DRY (Pump ON)"));
  Serial.println(F("      >650 WET (Pump OFF)"));
  Serial.println(F("LDR:  <350 DARK (Lamp ON)"));
  Serial.println(F("      >450 BRIGHT (Lamp OFF)"));
  Serial.println(F("\n[HASH TABLE]"));
  Serial.print(F("Table Size: ")); Serial.println(HASH_TABLE_SIZE);
  Serial.print(F("Max Nodes:  ")); Serial.println(MAX_NODES);
  Serial.println(F("Collision Handling: Chaining"));
  Serial.println(F("\n[TEST MODE]"));
  Serial.println(F("Reading initial sensor values..."));
  Serial.println();
}

void loop() {

  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        currentPage = (currentPage + 1) % 3;
        lcd.clear();
        Serial.print(F(">>> Page: ")); Serial.println(currentPage);
      }
    }
  }
  
  if (millis() - lastPageRotate >= PAGE_AUTO_ROTATE_INTERVAL) {
    lastPageRotate = millis();
    currentPage = (currentPage + 1) % 3;
    lcd.clear();
    Serial.print(F(">>> Auto Page: ")); Serial.println(currentPage);
  }
  
  lastButtonState = reading;

  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();

    soil = analogRead(SOIL_PIN);
    ldr  = analogRead(LDR_PIN);

    int tempRaw = analogRead(TEMP_PIN);
    float voltage = tempRaw * (5.0 / 1023.0);
    tempC = (voltage - 0.5) * 100.0;

 
    bool sensorError =
      soil < 0 || soil > 1023 ||
      ldr  < 0 || ldr  > 1023 ||
      tempC < -20 || tempC > 80;

    if (sensorError) {
      pumpOn = false;
      lampOn = false;
      Serial.println(F("!!! SENSOR ERROR !!!"));
    } else {

      bool prevPump = pumpOn;
      if (!pumpOn && soil < SOIL_DRY_ON) {
        pumpOn = true;
      }
      if (pumpOn && soil > SOIL_WET_OFF) {
        pumpOn = false;
      }
      
      if (pumpOn != prevPump) {
        Serial.print(F(">>> PUMP: "));
        Serial.print(pumpOn ? F("ON") : F("OFF"));
        Serial.print(F(" (Soil: "));
        Serial.print(soil);
        Serial.print(pumpOn ? F(" - DRY)") : F(" - WET)"));
        Serial.println();
      }

   
      bool prevLamp = lampOn;
      if (!lampOn && ldr < LDR_DARK_ON) {
        lampOn = true;
      }
      if (lampOn && ldr > LDR_BRIGHT_OFF) {
        lampOn = false;
      }
      
      if (lampOn != prevLamp) {
        Serial.print(F(">>> LAMP: "));
        Serial.print(lampOn ? F("ON") : F("OFF"));
        Serial.print(F(" (LDR: "));
        Serial.print(ldr);
        Serial.print(lampOn ? F(" - DARK)") : F(" - BRIGHT)"));
        Serial.println();
      }
    }

    digitalWrite(RELAY_PUMP, pumpOn ? RELAY_ON : RELAY_OFF);
    digitalWrite(RELAY_LAMP, lampOn ? RELAY_ON : RELAY_OFF);


    Serial.print(F("Sensor -> Soil: "));
    Serial.print(soil);
    if (soil < SOIL_DRY_ON) {
      Serial.print(F(" [DRY]   -> Pump: ON  (Pin5: "));
    } else if (soil > SOIL_WET_OFF) {
      Serial.print(F(" [WET]   -> Pump: OFF (Pin5: "));
    } else {
      Serial.print(F(" [MID]   -> Pump: "));
      Serial.print(pumpOn ? F("ON ") : F("OFF"));
      Serial.print(F(" (Pin5: "));
    }
    Serial.print(digitalRead(RELAY_PUMP) == HIGH ? F("HIGH") : F("LOW"));
    Serial.print(F(")"));
    
    Serial.print(F(" | LDR: "));
    Serial.print(ldr);
    if (ldr < LDR_DARK_ON) {
      Serial.print(F(" [DARK]  -> Lamp: ON  (Pin6: "));
    } else if (ldr > LDR_BRIGHT_OFF) {
      Serial.print(F(" [BRIGHT]-> Lamp: OFF (Pin6: "));
    } else {
      Serial.print(F(" [MID]   -> Lamp: "));
      Serial.print(lampOn ? F("ON ") : F("OFF"));
      Serial.print(F(" (Pin6: "));
    }
    Serial.print(digitalRead(RELAY_LAMP) == HIGH ? F("HIGH") : F("LOW"));
    Serial.print(F(")"));
    
    Serial.print(F(" | Temp: "));
    Serial.print(tempC, 1);
    Serial.println(F("C"));
  }

  if (millis() - lastHashSave >= HASH_SAVE_INTERVAL) {
    lastHashSave = millis();
    
    int key = soil / 100;
    
    hashInsert(key, soil, ldr, tempC);
    printHashTable();
  }


  if (currentPage == 0) {
    lcd.setCursor(0,0);
    lcd.print("Soil: ");
    lcd.print(soil);
    lcd.print("    ");

    lcd.setCursor(0,1);
    if (soil < SOIL_DRY_ON) {
      lcd.print("DRY |Pump: ON  ");
    } else if (soil > SOIL_WET_OFF) {
      lcd.print("WET |Pump: OFF ");
    } else {
      lcd.print("MID |Pump: ");
      lcd.print(pumpOn ? "ON  " : "OFF ");
    }
  }
  else if (currentPage == 1) {
    lcd.setCursor(0,0);
    lcd.print("Light: ");
    lcd.print(ldr);
    lcd.print("   ");

    lcd.setCursor(0,1);
    if (ldr < LDR_DARK_ON) {
      lcd.print("DARK|Lamp: ON  ");
    } else if (ldr > LDR_BRIGHT_OFF) {
      lcd.print("BRT |Lamp: OFF ");
    } else {
      lcd.print("MID |Lamp: ");
      lcd.print(lampOn ? "ON  " : "OFF ");
    }
  }
  else {
    lcd.setCursor(0,0);
    lcd.print("Temp: ");
    lcd.print(tempC,1);
    lcd.print((char)223);
    lcd.print("C   ");

    lcd.setCursor(0,1);
    if (tempC >= TEMP_HOT_THRESHOLD) {
      lcd.print("Status: HOT !  ");
    } else {
      lcd.print("Status: Normal ");
    }
  }

  delay(10);
}