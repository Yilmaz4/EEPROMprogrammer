constexpr int sdata = 2;
constexpr int sclck = 3;
constexpr int sltch = 4;

constexpr int eeprom_d0 = 5;
constexpr int eeprom_d7 = 12;
constexpr int write_en = 13;

int size = 0;
int lengthOfData = 0;

int receivingData = 0;
unsigned int address = 0;
int receivingSize = 0;

void set_address(int addr, bool OE) { 
  shiftOut(sdata, sclck, MSBFIRST, (addr >> 8) | (OE ? 0x00 : 0x80));
  shiftOut(sdata, sclck, MSBFIRST, (addr));

  digitalWrite(sltch, LOW);
  digitalWrite(sltch, HIGH);
  digitalWrite(sltch, LOW);
}

byte read(unsigned int addr) {
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

void write(unsigned int addr, byte data) {
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
  delay(10);
}

void dump(unsigned int limit) {
  for (int i = 0; i < limit; i++) {
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
    if (receivingData) {
      while (Serial.available() < 3);
      address = Serial.read();
      address <<= 8;
      address += Serial.read();

      write(address, (unsigned char)Serial.read());
      if (receivingData >= lengthOfData) {
        receivingData = 0;
        break;
      }
      receivingData += 1;
      address = 0;
    }
    else {
      switch (Serial.read()) {
      case 0x00:
        while (Serial.available() < 2);
        lengthOfData = Serial.read();
        lengthOfData <<= 8;
        lengthOfData += Serial.read();
        receivingData = 1;
        break;
      case 0x01:
        dump(size);
        break;
      case 0x02:
        while (!Serial.available());
        size = (Serial.read() == 0x00 ? 8192 : 32768);
        break;
      case 0x03:
        for (int i = 0; i < size; i++) {
          write(i, 0x00);
          if (i % 64 == 0) {
            Serial.write((unsigned char)0x55);
          }
        }
        break;
      }
    }
  }
}