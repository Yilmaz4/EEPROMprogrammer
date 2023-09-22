constexpr int sdata = 2;
constexpr int sclck = 3;
constexpr int sltch = 4;

constexpr int eeprom_d0 = 5;
constexpr int eeprom_d7 = 12;
constexpr int write_en = 13;

int addr = 0x0000;

int receiving = 0;

void set_address(int addr, bool OE) { 
  shiftOut(sdata, sclck, MSBFIRST, (addr >> 8) | (OE ? 0x00 : 0x80));
  shiftOut(sdata, sclck, MSBFIRST, (addr));

  digitalWrite(sltch, LOW);
  digitalWrite(sltch, HIGH);
  digitalWrite(sltch, LOW);
}

byte read(int addr) {
  for (int pin = eeprom_d7; pin >= eeprom_d0; pin--) {
    pinMode(pin, INPUT);
  }
  set_address(addr, true);
  byte data = 0;
  for (int pin = eeprom_d7; pin >= eeprom_d0; pin--) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

void write(int addr, byte data) {
  set_address(addr, false);
  for (int pin = eeprom_d0; pin <= eeprom_d7; pin++) {
    pinMode(pin, OUTPUT);
  }
  for (int pin = eeprom_d0; pin <= eeprom_d7; pin++) {
    digitalWrite(pin, data & 1);
    data >>= 1;
  }
  digitalWrite(write_en, LOW);
  delayMicroseconds(1);
  digitalWrite(write_en, HIGH);
  delay(1);
}

void dump(uint32_t limit = 8192) {
  for (uint32_t i = 0; i < limit; i++) {
    Serial.write((unsigned char)read(i));
  }
}

void setup() {
  pinMode(sdata, OUTPUT);
  pinMode(sclck, OUTPUT);
  pinMode(sltch, OUTPUT);

  digitalWrite(write_en, HIGH);
  pinMode(write_en, OUTPUT);

  Serial.begin(115200);
  Serial.setTimeout(100);
}

void loop() {
  readSerial();
}

void readSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (receiving) {
      write(addr, (byte)((unsigned char)ch));
      if (addr >= 8191) {
        receiving = 0;
        addr = 0;
      }
      addr += 1;
    }
    else {
      switch (int((unsigned char)ch)) {
      case 0xaa:
        receiving = 1;
        addr = 0;
        break;
      case 0xab:
        dump(8192);
        break;
      }
    }
  }
}
