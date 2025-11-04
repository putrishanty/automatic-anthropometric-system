#define BLYNK_TEMPLATE_ID "YourTemplateID"
#define BLYNK_TEMPLATE_NAME "YourTemplateName"
#define BLYNK_AUTH_TOKEN "YourAuthToken"
#define BLYNK_PRINT Serial

#include <HX711_ADC.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Fuzzy.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "YourWiFi";
char pass[] = "YourPassword";

TFT_eSPI tft = TFT_eSPI();

const int trigPin = 21;
const int echoPin = 19;
const int HX711_dout = 23;
const int HX711_sck = 22;

HX711_ADC LoadCell(HX711_dout, HX711_sck);
Fuzzy *fuzzy = new Fuzzy();

float weightkg = 0.0, height = 0.0, bmi = 0.0;
int userAge;

float lastWeight = -1, lastHeight = -1, lastBMI = -1;
float lastOutBBU = -1, lastOutPBU = -1, lastOutIMTU = -1;
String lastStatusBBU = "", lastStatusPBU = "", lastStatusIMTU = "";
String statusBBU = "-", statusPBU = "-", statusIMTU = "-";

unsigned long t = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSerialPrint = 0;
unsigned long lastBlynkUpdate = 0;
const unsigned long lcdInterval = 1000;
const unsigned long serialInterval = 1000;
const unsigned long blynkInterval = 5000;

unsigned long startTime;
unsigned long endTime;
unsigned long execTime;

void setupFuzzy() {
  FuzzyInput *Berat = new FuzzyInput(1);
  FuzzySet *BKurang = new FuzzySet(3.4, 3.4, 5.1, 5.8);
  FuzzySet *BNormal = new FuzzySet(5.1, 5.8, 6.7, 8.8);
  FuzzySet *BLebih = new FuzzySet(6.7, 8.8, 10.8, 10.8);
  Berat->addFuzzySet(BKurang);
  Berat->addFuzzySet(BNormal);
  Berat->addFuzzySet(BLebih);
  fuzzy->addFuzzyInput(Berat);

  FuzzyInput *Panjang = new FuzzyInput(2);
  FuzzySet *PKurang = new FuzzySet(50.8, 50.8, 60.6, 61.4);
  FuzzySet *PNormal = new FuzzySet(60.6, 61.4, 71, 71.9);
  FuzzySet *PLebih = new FuzzySet(71, 71.9, 83, 83);
  Panjang->addFuzzySet(PKurang);
  Panjang->addFuzzySet(PNormal);
  Panjang->addFuzzySet(PLebih);
  fuzzy->addFuzzyInput(Panjang);

  FuzzyInput *IMT = new FuzzyInput(3);
  FuzzySet *IKurang = new FuzzySet(12.4, 12.4, 13.7, 14.8);
  FuzzySet *INormal = new FuzzySet(13.7, 14.8, 16.3, 17.3);
  FuzzySet *ILebih = new FuzzySet(16.3, 17.3, 18.8, 18.8);
  IMT->addFuzzySet(IKurang);
  IMT->addFuzzySet(INormal);
  IMT->addFuzzySet(ILebih);
  fuzzy->addFuzzyInput(IMT);

  FuzzyInput *Usia = new FuzzyInput(4);
  FuzzySet *Neonatal = new FuzzySet(0, 0, 1, 2);
  FuzzySet *infantAwal = new FuzzySet(1, 2, 6, 7);
  FuzzySet *infantLanjut = new FuzzySet(6, 7, 12, 12);
  Usia->addFuzzySet(Neonatal);
  Usia->addFuzzySet(infantAwal);
  Usia->addFuzzySet(infantLanjut);
  fuzzy->addFuzzyInput(Usia);

  FuzzyOutput *BB_U = new FuzzyOutput(1);
  FuzzySet *BBKurang = new FuzzySet(0, 0, 2, 3);
  FuzzySet *BBNormal = new FuzzySet(2, 3, 7, 8);
  FuzzySet *BBObesitas = new FuzzySet(7, 8, 10, 10);
  BB_U->addFuzzySet(BBKurang);
  BB_U->addFuzzySet(BBNormal);
  BB_U->addFuzzySet(BBObesitas);
  fuzzy->addFuzzyOutput(BB_U);

  FuzzyOutput *PB_U = new FuzzyOutput(2);
  FuzzySet *Stunting = new FuzzySet(0, 0, 2, 3);
  FuzzySet *PBNormal = new FuzzySet(2, 3, 7, 8);
  FuzzySet *Tinggi = new FuzzySet(7, 8, 10, 10);
  PB_U->addFuzzySet(Stunting);
  PB_U->addFuzzySet(PBNormal);
  PB_U->addFuzzySet(Tinggi);
  fuzzy->addFuzzyOutput(PB_U);

  FuzzyOutput *IMT_U = new FuzzyOutput(3);
  FuzzySet *Wasting = new FuzzySet(0, 0, 1, 2);
  FuzzySet *GiziBaik = new FuzzySet(1, 2, 6, 8);
  FuzzySet *IObesitas = new FuzzySet(6, 8, 10, 10);
  IMT_U->addFuzzySet(Wasting);
  IMT_U->addFuzzySet(GiziBaik);
  IMT_U->addFuzzySet(IObesitas);
  fuzzy->addFuzzyOutput(IMT_U);

  FuzzyRuleAntecedent *ifBeratKurangAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifBeratKurangAndUsiaNeonatal->joinWithAND(BKurang, Neonatal);
  FuzzyRuleConsequent *thenBB_UNormal = new FuzzyRuleConsequent();
  thenBB_UNormal->addOutput(BBNormal);
  FuzzyRule *fuzzyRule1 = new FuzzyRule(1, ifBeratKurangAndUsiaNeonatal, thenBB_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule1);

  FuzzyRuleAntecedent *ifBeratKurangAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifBeratKurangAndUsiainfantAwal->joinWithAND(BKurang, infantAwal);
  FuzzyRuleConsequent *thenBB_UKurang = new FuzzyRuleConsequent();
  thenBB_UKurang->addOutput(BBKurang);
  FuzzyRule *fuzzyRule2 = new FuzzyRule(2, ifBeratKurangAndUsiainfantAwal, thenBB_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule2);

  FuzzyRuleAntecedent *ifBeratKurangAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifBeratKurangAndUsiainfantLanjut->joinWithAND(BKurang, infantLanjut);
  FuzzyRule *fuzzyRule3 = new FuzzyRule(3, ifBeratKurangAndUsiainfantLanjut, thenBB_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule3);

  FuzzyRuleAntecedent *ifBeratNormalAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifBeratNormalAndUsiaNeonatal->joinWithAND(BNormal, Neonatal);
  FuzzyRuleConsequent *thenBB_UObesitas = new FuzzyRuleConsequent();
  thenBB_UObesitas->addOutput(BBObesitas);
  FuzzyRule *fuzzyRule4 = new FuzzyRule(4, ifBeratNormalAndUsiaNeonatal, thenBB_UObesitas);
  fuzzy->addFuzzyRule(fuzzyRule4);

  FuzzyRuleAntecedent *ifBeratNormalAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifBeratNormalAndUsiainfantAwal->joinWithAND(BNormal, infantAwal);
  FuzzyRule *fuzzyRule5 = new FuzzyRule(5, ifBeratNormalAndUsiainfantAwal, thenBB_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule5);

  FuzzyRuleAntecedent *ifBeratNormalAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifBeratNormalAndUsiainfantLanjut->joinWithAND(BNormal, infantLanjut);
  FuzzyRule *fuzzyRule6 = new FuzzyRule(6, ifBeratNormalAndUsiainfantLanjut, thenBB_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule6);

  FuzzyRuleAntecedent *ifBeratLebihAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifBeratLebihAndUsiaNeonatal->joinWithAND(BLebih, Neonatal);
  FuzzyRule *fuzzyRule7 = new FuzzyRule(7, ifBeratLebihAndUsiaNeonatal, thenBB_UObesitas);
  fuzzy->addFuzzyRule(fuzzyRule7);

  FuzzyRuleAntecedent *ifBeratLebihAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifBeratLebihAndUsiainfantAwal->joinWithAND(BLebih, infantAwal);
  FuzzyRule *fuzzyRule8 = new FuzzyRule(8, ifBeratLebihAndUsiainfantAwal, thenBB_UObesitas);
  fuzzy->addFuzzyRule(fuzzyRule8);

  FuzzyRuleAntecedent *ifBeratLebihAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifBeratLebihAndUsiainfantLanjut->joinWithAND(BLebih, infantLanjut);
  FuzzyRule *fuzzyRule9 = new FuzzyRule(9, ifBeratLebihAndUsiainfantLanjut, thenBB_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule9);

  FuzzyRuleAntecedent *ifPanjangKurangAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifPanjangKurangAndUsiaNeonatal->joinWithAND(PKurang, Neonatal);
  FuzzyRuleConsequent *thenPB_UNormal = new FuzzyRuleConsequent();
  thenPB_UNormal->addOutput(PBNormal);
  FuzzyRule *fuzzyRule10 = new FuzzyRule(10, ifPanjangKurangAndUsiaNeonatal, thenPB_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule10);

  FuzzyRuleAntecedent *ifPanjangKurangAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifPanjangKurangAndUsiainfantAwal->joinWithAND(PKurang, infantAwal);
  FuzzyRuleConsequent *thenPB_UKurang = new FuzzyRuleConsequent();
  thenPB_UKurang->addOutput(Stunting);
  FuzzyRule *fuzzyRule11 = new FuzzyRule(11, ifPanjangKurangAndUsiainfantAwal, thenPB_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule11);

  FuzzyRuleAntecedent *ifPanjangKurangAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifPanjangKurangAndUsiainfantLanjut->joinWithAND(PKurang, infantLanjut);
  FuzzyRule *fuzzyRule12 = new FuzzyRule(12, ifPanjangKurangAndUsiainfantLanjut, thenPB_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule12);

  FuzzyRuleAntecedent *ifPanjangNormalAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifPanjangNormalAndUsiaNeonatal->joinWithAND(PNormal, Neonatal);
  FuzzyRuleConsequent *thenPB_UTinggi = new FuzzyRuleConsequent();
  thenPB_UTinggi->addOutput(Tinggi);
  FuzzyRule *fuzzyRule13 = new FuzzyRule(13, ifPanjangNormalAndUsiaNeonatal, thenPB_UTinggi);
  fuzzy->addFuzzyRule(fuzzyRule13);

  FuzzyRuleAntecedent *ifPanjangNormalAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifPanjangNormalAndUsiainfantAwal->joinWithAND(PNormal, infantAwal);
  FuzzyRule *fuzzyRule14 = new FuzzyRule(14, ifPanjangNormalAndUsiainfantAwal, thenPB_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule14);

  FuzzyRuleAntecedent *ifPanjangNormalAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifPanjangNormalAndUsiainfantLanjut->joinWithAND(PNormal, infantLanjut);
  FuzzyRule *fuzzyRule15 = new FuzzyRule(15, ifPanjangNormalAndUsiainfantLanjut, thenPB_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule15);

  FuzzyRuleAntecedent *ifPanjangLebihAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifPanjangLebihAndUsiaNeonatal->joinWithAND(PLebih, Neonatal);
  FuzzyRule *fuzzyRule16 = new FuzzyRule(16, ifPanjangLebihAndUsiaNeonatal, thenPB_UTinggi);
  fuzzy->addFuzzyRule(fuzzyRule16);

  FuzzyRuleAntecedent *ifPanjangLebihAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifPanjangLebihAndUsiainfantAwal->joinWithAND(PLebih, infantAwal);
  FuzzyRule *fuzzyRule17 = new FuzzyRule(17, ifPanjangLebihAndUsiainfantAwal, thenPB_UTinggi);
  fuzzy->addFuzzyRule(fuzzyRule17);

  FuzzyRuleAntecedent *ifPanjangLebihAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifPanjangLebihAndUsiainfantLanjut->joinWithAND(PLebih, infantLanjut);
  FuzzyRule *fuzzyRule18 = new FuzzyRule(18, ifPanjangLebihAndUsiainfantLanjut, thenPB_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule18);

  FuzzyRuleAntecedent *ifIMTKurangAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifIMTKurangAndUsiaNeonatal->joinWithAND(IKurang, Neonatal);
  FuzzyRuleConsequent *thenIMT_UNormal = new FuzzyRuleConsequent();
  thenIMT_UNormal->addOutput(GiziBaik);
  FuzzyRule *fuzzyRule19 = new FuzzyRule(19, ifIMTKurangAndUsiaNeonatal, thenIMT_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule19);

  FuzzyRuleAntecedent *ifIMTKurangAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifIMTKurangAndUsiainfantAwal->joinWithAND(IKurang, infantAwal);
  FuzzyRuleConsequent *thenIMT_UKurang = new FuzzyRuleConsequent();
  thenIMT_UKurang->addOutput(Wasting);
  FuzzyRule *fuzzyRule20 = new FuzzyRule(20, ifIMTKurangAndUsiainfantAwal, thenIMT_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule20);

  FuzzyRuleAntecedent *ifIMTKurangAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifIMTKurangAndUsiainfantLanjut->joinWithAND(IKurang, infantLanjut);
  FuzzyRule *fuzzyRule21 = new FuzzyRule(21, ifIMTKurangAndUsiainfantLanjut, thenIMT_UKurang);
  fuzzy->addFuzzyRule(fuzzyRule21);

  FuzzyRuleAntecedent *ifIMTNormalAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifIMTNormalAndUsiaNeonatal->joinWithAND(INormal, Neonatal);
  FuzzyRuleConsequent *thenIMT_UObesitas = new FuzzyRuleConsequent();
  thenIMT_UObesitas->addOutput(IObesitas);
  FuzzyRule *fuzzyRule22 = new FuzzyRule(22, ifIMTNormalAndUsiaNeonatal, thenIMT_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule22);

  FuzzyRuleAntecedent *ifIMTNormalAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifIMTNormalAndUsiainfantAwal->joinWithAND(INormal, infantAwal);
  FuzzyRule *fuzzyRule23 = new FuzzyRule(23, ifIMTNormalAndUsiainfantAwal, thenIMT_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule23);

  FuzzyRuleAntecedent *ifIMTNormalAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifIMTNormalAndUsiainfantLanjut->joinWithAND(INormal, infantLanjut);
  FuzzyRule *fuzzyRule24 = new FuzzyRule(24, ifIMTNormalAndUsiainfantLanjut, thenIMT_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule24);

  FuzzyRuleAntecedent *ifIMTLebihAndUsiaNeonatal = new FuzzyRuleAntecedent();
  ifIMTLebihAndUsiaNeonatal->joinWithAND(ILebih, Neonatal);
  FuzzyRule *fuzzyRule25 = new FuzzyRule(25, ifIMTLebihAndUsiaNeonatal, thenIMT_UObesitas);
  fuzzy->addFuzzyRule(fuzzyRule25);

  FuzzyRuleAntecedent *ifIMTLebihAndUsiainfantAwal = new FuzzyRuleAntecedent();
  ifIMTLebihAndUsiainfantAwal->joinWithAND(ILebih, infantAwal);
  FuzzyRule *fuzzyRule26 = new FuzzyRule(26, ifIMTLebihAndUsiainfantAwal, thenIMT_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule26);

  FuzzyRuleAntecedent *ifIMTLebihAndUsiainfantLanjut = new FuzzyRuleAntecedent();
  ifIMTLebihAndUsiainfantLanjut->joinWithAND(ILebih, infantLanjut);
  FuzzyRule *fuzzyRule27 = new FuzzyRule(27, ifIMTLebihAndUsiainfantLanjut, thenIMT_UNormal);
  fuzzy->addFuzzyRule(fuzzyRule27);
}

void setup(void) {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  LoadCell.begin();
  LoadCell.start(2000, true);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("HX711 Timeout. Check wiring!");
  } else {
    LoadCell.setCalFactor(-23.0);
    Serial.println("HX711 Ready.");
  }
  while (!LoadCell.update())
    ;

  setupFuzzy();

  WiFi.begin(ssid, pass);
  Blynk.begin(auth, ssid, pass);
}

BLYNK_WRITE(V3) {
  userAge = param.asInt();
}

float readUltrasonicDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  float duration = pulseIn(echoPin, HIGH, 30000);
  float distance = duration * 0.0343 / 2.0;
  return distance;
}

void updateDisplay() {
  char buf[10], puf[10];
  sprintf(buf, "%.1f", weightkg);
  sprintf(puf, "%.1f", height);

  tft.setTextColor(TFT_WHITE);
  tft.fillRect(37, 33, 177, 96, TFT_BLACK);
  tft.setTextSize(2);
  tft.setFreeFont(&FreeSerifBold24pt7b);
  tft.drawString(puf, 46, 39);
  tft.setFreeFont(&FreeSerif12pt7b);
  tft.setTextSize(1);
  tft.drawString("cm", 107, 124);
  tft.setFreeFont(&FreeSerifBold12pt7b);
  tft.drawString("Panjang", 84, 4);

  tft.fillRect(274, 31, 177, 96, TFT_BLACK);
  tft.setTextSize(2);
  tft.setFreeFont(&FreeSerifBold24pt7b);
  tft.drawString(buf, 278, 39);
  tft.setFreeFont(&FreeSerif12pt7b);
  tft.setTextSize(1);
  tft.drawString("kg", 349, 128);
  tft.setFreeFont(&FreeSerifBold12pt7b);
  tft.drawString("Berat", 334, 6);

  tft.setFreeFont(&FreeSerifBold12pt7b);
  tft.drawString("PB/U", 14, 180);
  tft.drawString("IMT/U", 8, 230);
  tft.drawString("BB/U", 15, 279);

  tft.drawRect(94, 218, 307, 46, TFT_WHITE);
  tft.drawRect(95, 169, 307, 46, TFT_WHITE);
  tft.drawRect(93, 269, 307, 46, TFT_WHITE);

  tft.fillRect(98, 171, 299, 41, TFT_BLACK);
  tft.fillRect(97, 220, 299, 41, TFT_BLACK);
  tft.fillRect(97, 271, 299, 41, TFT_BLACK);
  tft.setFreeFont(&FreeSerifBold12pt7b);
  tft.drawString(statusIMTU.c_str(), 105, 177);
  tft.drawString(statusBBU.c_str(), 101, 227);
  tft.drawString(statusPBU.c_str(), 111, 276);
}

void loop() {
  startTime = millis();
  Blynk.run();

  static boolean newDataReady = 0;

  if (LoadCell.update()) {
    newDataReady = true;
  }

  if (newDataReady && millis() > t + 500) {
    float weight = LoadCell.getData();
    weightkg = weight / 1000.0;
    if (weightkg < 0) weightkg = 0;
    t = millis();
    newDataReady = false;

    float distance = readUltrasonicDistance();
    height = max(0.0, 68.0 - distance);
    float height_m = height / 100.0;
    bmi = (height_m > 0) ? weightkg / (height_m * height_m) : 0;

    fuzzy->setInput(1, weightkg);
    fuzzy->setInput(2, height);
    fuzzy->setInput(3, bmi);
    fuzzy->setInput(4, userAge);
    fuzzy->fuzzify();

    float outBBU = fuzzy->defuzzify(1);
    float outPBU = fuzzy->defuzzify(2);
    float outIMTU = fuzzy->defuzzify(3);

    statusBBU = (outBBU < 3) ? "Risiko BB Kurang" : (outBBU < 8 ? "BB Normal" : "Risiko BB Lebih");
    statusPBU = (outPBU < 3) ? "Risiko Stunting" : (outPBU < 8 ? "PB Normal" : "Tinggi");
    statusIMTU = (outIMTU < 2) ? "Risiko Wasting" : (outIMTU < 8 ? "Gizi Baik" : "Risiko Gizi Lebih");

    bool valueChanged = (abs(weightkg - lastWeight) > 0.1) || (abs(height - lastHeight) > 0.5) || (statusBBU != lastStatusBBU) || (statusPBU != lastStatusPBU) || (statusIMTU != lastStatusIMTU);

    if (millis() - lastLCDUpdate > lcdInterval && valueChanged) {
      updateDisplay();

      lastWeight = weightkg;
      lastHeight = height;

      lastStatusBBU = statusBBU;
      lastStatusPBU = statusPBU;
      lastStatusIMTU = statusIMTU;

      lastLCDUpdate = millis();
    }

    if (millis() - lastSerialPrint > serialInterval) {
      Serial.printf("Weight: %.0f g | Height: %.2f cm | BMI: %.2f | Age: %d\n", weight, height, bmi, userAge);
      Serial.println("BB/U: " + statusBBU + " | PB/U: " + statusPBU + " | IMT/U: " + statusIMTU);
      lastSerialPrint = millis();
    }

    if (millis() - lastBlynkUpdate >= blynkInterval) {
      Blynk.virtualWrite(V0, height);
      // Blynk.virtualWrite(V7, weightkg);
      Blynk.virtualWrite(V4, statusPBU);
      Blynk.virtualWrite(V5, statusBBU);
      Blynk.virtualWrite(V6, statusIMTU);
      lastBlynkUpdate = millis();
    }
  }

  if (Serial.available() > 0) {
    if (Serial.read() == 't') LoadCell.tareNoDelay();
  }

  if (LoadCell.getTareStatus()) {
    Serial.println("Tare complete");
  }

  endTime = millis();
  execTime = endTime - startTime;
  Serial.print("Loop time: ");
  Serial.print(execTime);
  Serial.println(" ms");
}