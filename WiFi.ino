void WiFi_Connect () {
  WiFi.mode(WIFI_STA);
  WiFi.setMinSecurity(WIFI_AUTH_OPEN);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  uint32_t start_time = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - start_time < WIFI_RES_PERIOD) {        
    delay(1000);
    
    Serial.print("Connecting to \"");
    Serial.print(ssid);
    Serial.println("\"...");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Restarting (timer overflow)!");
    ESP.restart();
  }

  Serial.println("Connected!"); 
  randomSeed(micros());
}