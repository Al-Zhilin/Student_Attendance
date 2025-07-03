void EEPROM_INIT() {
  EEPROM.put(0, week_off);        //смещение в количестве недель
  EEPROM.commit();
}

void EEPROM_START() {
  EEPROM.get(0, week_off);
}

void EEPROM_PUT(byte addr, byte num) {
  EEPROM.put(addr, num);
  EEPROM.commit();
  
  EEPROM_START();
}