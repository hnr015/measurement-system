#include "Arduino.h"
#include <Wire.h>
#include <LCD-I2C.h>
#include <DHT.h>
#include "WiFiS3.h"
#include <ArduinoJson.h>
#include <math.h>
#include "index.h"

#define DHT_PIN 8 // definicja pinu czujnika DHT22 (cyfrowe)
#define DHT_TYPE DHT22 // definicja typu czujnika DHT22 

#define LED_PIN 13 // definicja pinu diody LED (cyfrowe)
#define BUZZER_PIN 4 // definicja pinu buzzera (cyfrowe)

#define MQ2_PIN A0 // definicja pinu czujnika MQ-2 (analogowe)

// Dane z noty katalogowej czujnika (wykres)
int RL_VALUE = 5;                           // rezystancja (w kOhm) obciążenia modułu MQ-2
int RO_CLEAN_AIR_FACTOR = 9.83;             // Rezystancja czujnika w czystym powietrzu
int CALIBARAION_SAMPLE_TIMES = 100;         // liczba próbek pobranych do kalibracji
int CALIBRATION_SAMPLE_INTERVAL = 200;      // przedział czasu (w ms) między każdą próbką w fazie kalibracji
int READ_SAMPLE_INTERVAL = 10;              // liczba próbek dla pomiaru w trybie normalnej pracy
int READ_SAMPLE_TIMES = 5;                  // przedział czasu pomiędzy próbkami
int GAS_LPG = 0;
int GAS_CO = 1;
int GAS_SMOKE = 2;
// Opis charakterystyki czujnika
float LPGCurve[3] = {2.3,0.21,-0.47};       // z nich powstaje linia która w przybliżeniu jest równoważna prawdziwej charakterystyce czujnika
float COCurve[3] = {2.3,0.72,-0.34};        // format danych:{ x, y, slope}; punkt1, punkt2, nachylenie. punkt 1 i 2 są pobrane z wykresu
float SmokeCurve[3] = {2.3,0.53,-0.44};
float Ro = 50;                              // inicjalizacja Ro początkową wartością 50 kOhm
uint16_t lpg = 0, co = 0, smoke = 0; 
uint8_t rawMQ2 = 0;

uint8_t LCD_ADDRESS = 0x27; 
uint8_t LCD_COLUMNS = 20; 
uint8_t LCD_ROWS = 4;
LCD_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS); // przekazanie wartości do funkcji LCD

float h = 0, t = 0;
DHT dht(DHT_PIN, DHT_TYPE); // przekazanie wartości do funkcji DHT

String jsonData;
char id[] = "ArduinoUNOR4WiFi";

bool warning = true;
const int warningFrequency = 1000;  // 1000Hz
const int warningDuration = 1000; // 1000ms=1s
unsigned long startTime = 0; 
 
unsigned long currentTime = 0;
unsigned long previousTime = 0; 
const long timeoutTime = 2000; // 2000ms=2s

char ssid[] = "FunBox2-35D6-2"; 
char pass[] = "Mariusz1966"; 
String SSID = String(ssid);
int status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiClient client;
unsigned long wifiStartTime = 0;            // Czas rozpoczęcia działania WiFi
const unsigned long wifiDuration = 20000;   // (20s) Maksymalny czas z jakim WiFi ma przestać się łaczyć z siecią WiFi
bool wifiConnected = true;                  // flaga, która pozwala zapobiec wielokrotnem połączeniom 

WiFiClient clientDB;
int HTTP_PORT = 80;
char HOST_NAME[] = "192.168.1.125"; 
String PATH_NAME = "/system_pomiarowydb/database.php";
unsigned long mil_time = 0;

void setup() {
  Serial.begin(115200); 
  setPin();                   // funkcja przypisania pinów
  LCDbegin();                 // funkcja uruchom i zainicjuj wyświetlacz LCD
  blinkLED();                 // funkcja mignięcie diodą LED
  playWarning();              // funkcja wydania dźwięku buzzera
  dht.begin();            // funkcja uruchom i zainicjuj obiekt dht
  DHTReadAllDataFromSensor();
  while (true) {      // Pętla, która będzie się powtarzać, dopóki czujniki nie zadziałają       
    if (checkSensors()) {     // funkcja sprawdzająca poprawność działania czujników
      break;  // Czujniki działają poprawnie - przerwij pętlę
    } else {
      blinkLED();      // Wystąpił błąd czujnika - sygnalizuj i spróbuj ponownie
      playWarning();
      delay(5000); // Odczekaj 5 sekund przed ponowną próbą
      // Możesz dodać tutaj kod resetujący czujniki, jeśli to konieczne
    }
  }
  WiFiConnect();          // funkcja, która pozwala przyłączyć się do sieci WiFi
  MQbegin();              // funkcja zaczynająca kalibracje czujnika MQ2
}

void loop() {
    while (true) {      // Pętla, która będzie się powtarzać, dopóki czujniki nie zadziałają       
    if (checkSensors()) {   // funkcja sprawdzająca, czy dana liczba (w tym przypadku wartośći z czujników) zmiennoprzecinkowa 
                            // jest wartością NaN (Not a Number) czyli w skrócie funkcja sprawdza poprawność działania czujników
      DHTReadAllDataFromSensor();   // funckja sczytywania danych pomiarowych z czujnika DHT22
      MQReadAllDataFromSensor();    // funckja sczytywania danych pomiarowych z czujnika MQ2
      MQOutOfScope();               // funkcja sprawdzającą czy wartości pomiarowe z czujnika MQ2 wyszły poza skalę
      printDHTSensorData();         // funckja wyświetlająca dane pomiare z czujnika DHT22 na wyświetlaczu
      printMQSensorData();          // funckja wyświetlająca dane pomiare z czujnika MQ2 na wyświetlaczu
      break;  // Czujniki działają poprawnie - przerwij pętlę
    } else {
      blinkLED();      // Wystąpił błąd czujnika - sygnalizuj i spróbuj ponownie
      playWarning();
      delay(5000); // Odczekaj 5 sekund przed ponowną próbą
      // Możesz dodać tutaj kod resetujący czujniki, jeśli to konieczne
    }
  }
  if(wifiConnected == true) {
    sendDataToJSON();           // funkcja, która wstawia zmienne do dokumentu json
    sendDataToDatabase();       // funkcja wysyłająca wszystkie dane pomiarowe do lokalnej bazy danych
    StartWebServer();           // funkcja, która uruchamia serwera web na mikrokontrolerze
  }
}

void setPin() { // funkcja przypisania pinów
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(A0, INPUT_PULLDOWN);
}

void blinkLED() { // funkcja mignięcie diodą LED
  unsigned long startTime = millis();  

  while (millis() - startTime < warningDuration) {
    digitalWrite(LED_PIN, HIGH);    
    delayMicroseconds(warningFrequency / 2);  
    digitalWrite(LED_PIN, LOW);     
    delayMicroseconds(warningFrequency / 2);  
  }
}

void playWarning() { // funkcja wydania dźwięku buzzera 
  unsigned long startTime = millis();  

  while (millis() - startTime < warningDuration) {
    digitalWrite(BUZZER_PIN, HIGH);    
    delayMicroseconds(warningFrequency / 2);  
    digitalWrite(BUZZER_PIN, LOW);     
    delayMicroseconds(warningFrequency / 2);  
  }
}

void LCDbegin() { // funkcja uruchom i zainicjuj wyświetlacz LCD
  Wire.begin();
  lcd.begin(&Wire);
  lcd.display();
  lcd.backlight();
}

void MQbegin() { // funkcja zaczynająca kalibracje czujnika MQ2
  Serial.println("Kalibracja czujnika MQ-2");
  lcd.setCursor(5, 0);
  lcd.print("Kalibracja");
  lcd.setCursor(6, 1);
  lcd.print("czujnika");
  lcd.setCursor(8, 2);
  lcd.print("MQ-2");
  Ro = MQCalibration(A0);
  lcd.clear();
  Serial.print("Ro: ");
  Serial.print(Ro);
  Serial.println(" kOhm"); 
  Serial.println("Kalibracja zakończona");
  lcd.setCursor(5, 0);
  lcd.print("Kalibracja");
  lcd.setCursor(5, 1);
  lcd.print("zakonczona");
  lcd.setCursor(0, 3);
  lcd.print("Ro: ");
  lcd.setCursor(4, 3);
  lcd.print(Ro);
  lcd.setCursor(9, 3);
  lcd.print("kOhm"); 
  delay(3000);
  lcd.clear();
}

float MQResistanceCalculation(int raw_adc) { // funkcja 
  return (((float)RL_VALUE*(1023-raw_adc)/raw_adc));
}

float MQCalibration(int mq2_pin) { // funkcja kalibracyjna, wykonuje sekwencję pomiarów, przyjmowane jest że czujnik w czasie kalibracji musi znajdować sie w czystym powietrzu 
  float val=0;
 
  for (int i=0; i<CALIBARAION_SAMPLE_TIMES; i++) {// pomiar ustalonej wartości próbek           
    val += MQResistanceCalculation(analogRead(mq2_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val/CALIBARAION_SAMPLE_TIMES;                   // obliczenie wartości średniej 
  val = val/RO_CLEAN_AIR_FACTOR;                        // podzielenie wyniku pomiaru przez RO_CLEAN_AIR_FACTOR daje wartość Ro zgodnie z charakterystyką czujnika 
  return val; 
}

float MQRead(int mq2_pin) { // funkcja pomiaru wartości rezystancji czujnika, wykorzystuje funkcję kalibracyjną
  int i;
  float rs=0;

  for (i=0; i<READ_SAMPLE_TIMES; i++) {
    rs += MQResistanceCalculation(analogRead(mq2_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
  rs = rs/READ_SAMPLE_TIMES;
  return rs;  
}

float MQGetGasPercentage(float rs_ro_ratio, int gas_id) { // funkcja przekazuje nachylenie oraz punkty charakterystyki czujnika
  if ( gas_id == GAS_LPG ) {
    return MQGetPercentage(rs_ro_ratio,LPGCurve);
  } else if ( gas_id == GAS_CO ) {
    return MQGetPercentage(rs_ro_ratio,COCurve);
  } else if ( gas_id == GAS_SMOKE ) {
    return MQGetPercentage(rs_ro_ratio,SmokeCurve);
  }    
  return 0;
}

int MQGetPercentage(float rs_ro_ratio, float *pcurve) { // wykorzystując nachylenie i punkty charakterystyki obliczana jest współrzędna logarytmiczna
  return (pow(10,(((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}

void MQReadAllDataFromSensor() { // funckja sczytywania danych pomiarowych z czujnika MQ2
  for(uint8_t i=1; i<10; i++) { // pętla uśredniająca pomiar
    lpg = lpg + MQGetGasPercentage(MQRead(MQ2_PIN)/Ro,GAS_LPG); 
    co = co + MQGetGasPercentage(MQRead(MQ2_PIN)/Ro,GAS_CO); 
    smoke =  smoke + MQGetGasPercentage(MQRead(MQ2_PIN)/Ro,GAS_SMOKE); 
  }
  lpg=lpg/10;
  co=co/10;
  smoke=smoke/10;
}

void MQOutOfScope() { // funkcja sprawdzającą czy wartości pomiarowe z czujnika MQ2 wyszły poza skalę
  if(lpg>30 || co>30 || smoke>30) { // gdy przekroczono poziom 30 ppm włącz buzzer i migotanie diody LED
    warning==true; 
    playWarning();
    blinkLED();
  }
  else warning=false;
}

void DHTReadAllDataFromSensor() { // funckja sczytywania danych pomiarowych z czujnika DHT
  h = dht.readHumidity();
  t = dht.readTemperature();
}

void printDHTSensorData() { // funckja wyświetlająca dane pomiare z czujnika DHT na wyświetlaczu
  if (lpg<30 & co<30 & smoke<600) {
    lcd.setCursor(0, 0);
    lcd.print("T: "); 
    lcd.print(String(t, 1));
    lcd.print((char)223); 
    lcd.print("C   ");
    lcd.setCursor(11, 0); 
    lcd.print("H: "); 
    lcd.print(String(h, 1));
    lcd.print("% "); 
  }
  else {
    lcd.setCursor(0, 0);
    lcd.print(" UWAGA WYKRYTO GAZ! ");
  }
}

void printMQSensorData() { // funckja sczytywania danych pomiarów z czujnika DHT
  if(lpg<9999) {
    lcd.setCursor(0, 1);
    lcd.print("LPG: ");
    lcd.print(lpg);
    lcd.print(" ppm    ");
  }
  else {
    lcd.setCursor(0, 1);
    lcd.print("LPG: poza zakresem");
  }
  if(co<9999) {
    lcd.setCursor(0, 2);
    lcd.print("CO:  ");
    lcd.print(co);
    lcd.print(" ppm    ");
  }
  else {
    lcd.setCursor(0, 2);
    lcd.print("CO:  poza zakresem");
  }
  if(smoke<5000) {
    lcd.setCursor(0, 3);
    lcd.print("CO2: ");
    lcd.print(smoke);
    lcd.print(" ppm    ");
  }
  else {
    lcd.setCursor(0, 3);
    lcd.print("CO2: poza zakresem");
  }
}

bool checkSensors() { // funkcja sprawdzająca, czy dana liczba (w tym przypadku wartośći z czujników) zmiennoprzecinkowa jest wartością NaN (Not a Number) czyli w skrócie funkcja sprawdza poprawność działania czujników
  if((isnan(h) || isnan(t)) && (analogRead(A0) < 100)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Blad");
    lcd.setCursor(0, 1);
    lcd.print("obu czujnikow");
    lcd.setCursor(0, 3);
    lcd.print("Resetowanie...");
    NVIC_SystemReset();
    return false;
  }
  else if(isnan(h) || isnan(t)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Blad ");
    lcd.setCursor(0, 1);
    lcd.print("czujnika DHT");
    lcd.setCursor(0, 3);
    lcd.print("Resetowanie...");
    NVIC_SystemReset();
    return false;
  }
  else if(analogRead(A0) < 100) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Blad");
    lcd.setCursor(0, 1);
    lcd.print("czujnika MQ2");
    lcd.setCursor(0, 3);
    lcd.print("Resetowanie...");
    NVIC_SystemReset();
    return false;
  }
  return true; 
}

void sendDataToJSON() { // funkcja, która wstawia zmienne do dokumentu json
  StaticJsonDocument<512> doc; 
  doc["id"] = id;
  doc["t"] = t;
  doc["h"] = h;
  if (lpg > 9999) doc["lpg"] = "poza zakresem";
  else doc["lpg"] = lpg;
  if (co > 9999) doc["co"] = "poza zakresem";
  else doc["co"] = co;
  if (smoke > 9999) doc["smoke"] = "poza zakresem"; 
  else doc["smoke"] = smoke; 
  serializeJson(doc, jsonData); 
}

void WiFiConnect() { // funkcja, która pozwala przyłączyć się do sieci WiFi
  if (WiFi.status() == WL_NO_MODULE) { // sprawdź czy moduł wifi działa:
    Serial.println("Blad modulu WiFi");
    // nie kontynuuj
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Zaktualizuj oprogramowanie");
  }

  wifiStartTime = millis();

  // próba połączenia z siecią WiFi:
  while (status != WL_CONNECTED && wifiConnected) {
    Serial.print("Laczenie z siecia WiFi: ");
    Serial.println(ssid);                   // wyświetl SSID na monitorze szeregowym 
    lcd.setCursor(5, 0);
    lcd.print("Laczenie z");
    lcd.setCursor(3, 1);
    lcd.print("siecia WiFi...");
    lcd.setCursor(0, 4);
    lcd.print("SSID:");
    lcd.setCursor(6, 4);
    lcd.print(String(ssid, 11));
    if(SSID.length()>11) {
      lcd.setCursor(17, 4);
      lcd.print("...");
    }
    delay(2000);
    status = WiFi.begin(ssid, pass);

    if (millis() - wifiStartTime > wifiDuration) {
        lcd.clear();
        Serial.println("Przekroczono czas oczekiwania na polaczenie WiFi.");
        wifiConnected = false;
        lcd.setCursor(8, 0);
        lcd.print("Brak");
        lcd.setCursor(5, 1);
        lcd.print("polaczenia");
        delay(5000);
        lcd.clear();
        break; // Przerwij pętlę while, jeśli minęło 20 sekund
     }
  }

  if(wifiConnected) {
    Serial.print("Polaczono z siecia WiFi: ");
    Serial.println(ssid);
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Polaczono z");
    lcd.setCursor(4, 1);
    lcd.print("siecia WiFi!");
    lcd.setCursor(0, 2);
    lcd.print("IP serwera:");
    lcd.setCursor(0, 3);
    lcd.print(WiFi.localIP());
    server.begin(); // uruchom serwer WWW na porcie 80
    printWifiStatus(); // jesteś teraz połączony, więc wypisz status
    delay(4000);
    lcd.clear();
  }                   
}

void printWifiStatus() {
  Serial.print("SSID: "); // wypisz SSID sieci, do której jesteś podłączony
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI(); // // wypisz moc odebranego sygnłu w dBm
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  
  Serial.print("Aby wyświetlić stronę, otwórz przeglądarkę i wpisz http://"); // wypisz IP 
  Serial.println(ip);
}

void StartWebServer() { // funkcja, która uruchamia serwera web na mikrokontrolerze 
  client = server.available();
  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("new client");
    String currentLine = "";
    //boolean currentLineIsBlank = true; // żądanie HTTP kończy się pustą linią
    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      
    //while (client.connected()) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
        currentLine += c;
        Serial.write(c);
        // żądanie HTTP zostało zakończone, więc możesz wysłać odpowiedź
        // jeśli dotarłeś do końca wiersza (otrzymałeś znak nowej linii) i wiersz jest pusty
        if (c == '\n') {
          Serial.println(currentLine);
          if (currentLine.startsWith("GET /data")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json"); // Żądanie danych JSON
            client.println("Connection: close");
            client.println();
            client.print(jsonData);
          } 
          else if (currentLine.startsWith("GET /index.html ")) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html; charset=utf-8"); // Żądanie strony HTML
            client.println("Connection: close");
            client.println();
            client.print(mainPage);
          } 
          else if (currentLine.startsWith("GET / ")) {
            client.println("HTTP/1.1 301 Moved Permanently");
            client.println("Location: /index.html"); // przejdź ze strony głównej na index.html
            client.println();
          }
          else {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println();
            client.println("404 Not Found"); // Nieznane żądanie - wyślij kod błędu 404
          }
          currentLine = ""; // Resetuj linię żądania
          break;
        } 
        // if (c == '\n') {
        //   currentLineIsBlank = true;
        // } else if (c != '\r') {
        //   currentLineIsBlank = false;
        // }
      }
    }
    delay(100); // poczekaj 100ms, aby serwer mógł dostać dane
    client.stop(); 
    client.flush();
    Serial.println("client disconnected");
  }
}

void sendDataToDatabase() { // funkcja wysyłająca wszystkie dane pomiarowe do lokalnej bazy danych
  if ((millis() - mil_time) >= 5000 || mil_time == 0) { // co 10 sekund wysyła dane pomiarowe do bazy danych
    mil_time = millis();
    clientDB.stop();
    if (clientDB.connect(HOST_NAME, HTTP_PORT) ) {
      Serial.println("Połączono z serwerem.");
      clientDB.println("POST " + PATH_NAME + " HTTP/1.1");
      clientDB.println("Host: " + String(HOST_NAME) + "");
      clientDB.println("Content-Type: application/json");
      clientDB.println("Content-Length: " + String(jsonData.length()));
      clientDB.println("Connection: close");
      clientDB.println();
      clientDB.print(jsonData);
      clientDB.flush();
      clientDB.stop();
      Serial.println("\nRozłączono z serwerem.");
    } 
    else {
      Serial.println("Błąd połączenia z serwerem.");
    }
  }
}