#include <CytronMotorDriver.h>
#include <EEPROM.h>

// ================= MOTOR =================
CytronMD motorL(PWM_DIR, 5, 4); // kiri
CytronMD motorR(PWM_DIR, 6, 7); // kanan

// ================= RC PIN =================
#define CH1_PIN A1   // steering
#define CH2_PIN A2   // throttle

// ================= EEPROM =================
#define EEPROM_MAGIC 0xBEEF   // penanda data sah, elak baca sampah bila board baru/kosong
#define EEPROM_ADDR  0

struct Settings {
  uint16_t magic;
  int fwdTrim;
  int bwdTrim;
  int maxSpeed;
  int deadzone;
  int turnGain;   // 0-200 (%), 100 = normal, >100 = pusing lebih tajam
  int expo;       // 0-100, 0 = linear, 100 = curve lembut kat tengah
  int ch1Min;     // kalibrasi CH1 (steering)
  int ch1Center;
  int ch1Max;
  int ch2Min;     // kalibrasi CH2 (throttle)
  int ch2Center;
  int ch2Max;
};

Settings cfg;

// Default kalau EEPROM kosong / rosak
void loadDefaults() {
  cfg.magic     = EEPROM_MAGIC;
  cfg.fwdTrim   = 0;
  cfg.bwdTrim   = 0;
  cfg.maxSpeed  = 200;
  cfg.deadzone  = 20;
  cfg.turnGain  = 100;
  cfg.expo      = 0;
  cfg.ch1Min    = 1000;
  cfg.ch1Center = 1500;
  cfg.ch1Max    = 2000;
  cfg.ch2Min    = 1000;
  cfg.ch2Center = 1500;
  cfg.ch2Max    = 2000;
}

// Expo curve: output = expo*x^3 + (1-expo)*x, x dinormalisasi -1..1
// expoPercent 0 = terus linear (tak ubah apa-apa), 100 = curve penuh cubic
int applyExpo(int value, int expoPercent) {
  if (expoPercent <= 0) return value;
  float x = value / 255.0;
  float e = expoPercent / 100.0;
  float y = e * x * x * x + (1.0 - e) * x;
  int out = (int)(y * 255.0);
  return constrain(out, -255, 255);
}

// Mapping asymmetric guna nilai kalibrasi sebenar (bukan hardcode 1000-2000)
// Elak kesan tak simetri antara min->center dan center->max
int mapChannel(int raw, int minV, int centerV, int maxV) {
  raw = constrain(raw, minV, maxV);
  if (raw >= centerV) {
    if (maxV == centerV) return 0; // elak divide by zero kalau kalibrasi rosak
    return map(raw, centerV, maxV, 0, 255);
  } else {
    if (centerV == minV) return 0;
    return map(raw, minV, centerV, -255, 0);
  }
}

void loadSettings() {
  EEPROM.get(EEPROM_ADDR, cfg);
  if (cfg.magic != EEPROM_MAGIC) {
    loadDefaults();     // EEPROM belum pernah save, guna default
  }
}

void saveSettings() {
  cfg.magic = EEPROM_MAGIC;
  EEPROM.put(EEPROM_ADDR, cfg);
}

// ================= SERIAL PROTOCOL =================
// Command masuk (dari app), satu line, hujung '\n':
//   GET                  -> board balas CFG,...
//   SET,FWD,<val>        -> tukar FWD_TRIM (RAM sahaja, belum simpan)
//   SET,BWD,<val>        -> tukar BWD_TRIM
//   SET,MAX,<val>        -> tukar MAX_SPEED
//   SET,DEAD,<val>       -> tukar DEADZONE
//   SET,TURN,<val>       -> tukar TURN_GAIN
//   SET,EXPO,<val>       -> tukar EXPO
//   SAVE                 -> simpan semua setting ke EEPROM
//   LOAD                 -> baca balik dari EEPROM (buang perubahan RAM)
//   CAL,START            -> masuk mod kalibrasi (motor auto stop demi safety)
//   CAL,CENTER           -> tangkap posisi stick semasa sebagai TITIK TENGAH
//   CAL,SAVE             -> sahkan & simpan kalibrasi ke EEPROM, keluar mod kalibrasi
//   CAL,STOP             -> batal, keluar mod kalibrasi (buang perubahan kalibrasi)
//   CAL,GET              -> board balas CALCFG,... (nilai kalibrasi tersimpan)
//
// Balasan/output dari board:
//   CFG,FWD,<v>,BWD,<v>,MAX,<v>,DEAD,<v>,TURN,<v>,EXPO,<v>
//   CALCFG,CH1MIN,<v>,CH1CTR,<v>,CH1MAX,<v>,CH2MIN,<v>,CH2CTR,<v>,CH2MAX,<v>
//   OK,<PARAM>,<val>
//   OK,SAVED / OK,CALSTART / OK,CALSTOP / OK,CALSAVED
//   OK,CALCENTER,<ch1>,<ch2>
//   ERR,CAL,<sebab>
//   TEL,ch1,ch2,steer,throttle,left,right        (mod biasa, ~10Hz)
//   CALTEL,ch1,ch2,ch1min,ch1max,ch2min,ch2max   (mod kalibrasi, ~10Hz)

String rxBuffer = "";
unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL = 100; // ms (~10Hz)

// ---- Mod kalibrasi ----
bool calMode = false;
int calCh1Min, calCh1Max, calCh1Center;
int calCh2Min, calCh2Max, calCh2Center;
int lastCh1Raw = 1500, lastCh2Raw = 1500; // nilai raw terkini, untuk CAL,CENTER
const int CAL_MIN_RANGE = 150; // beza minimum min<->max supaya kalibrasi dianggap sah

void sendConfig() {
  Serial.print("CFG,FWD,");
  Serial.print(cfg.fwdTrim);
  Serial.print(",BWD,");
  Serial.print(cfg.bwdTrim);
  Serial.print(",MAX,");
  Serial.print(cfg.maxSpeed);
  Serial.print(",DEAD,");
  Serial.print(cfg.deadzone);
  Serial.print(",TURN,");
  Serial.print(cfg.turnGain);
  Serial.print(",EXPO,");
  Serial.println(cfg.expo);
}

void sendCalConfig() {
  Serial.print("CALCFG,CH1MIN,");
  Serial.print(cfg.ch1Min);
  Serial.print(",CH1CTR,");
  Serial.print(cfg.ch1Center);
  Serial.print(",CH1MAX,");
  Serial.print(cfg.ch1Max);
  Serial.print(",CH2MIN,");
  Serial.print(cfg.ch2Min);
  Serial.print(",CH2CTR,");
  Serial.print(cfg.ch2Center);
  Serial.print(",CH2MAX,");
  Serial.println(cfg.ch2Max);
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "GET") {
    sendConfig();
    return;
  }

  if (line == "SAVE") {
    saveSettings();
    Serial.println("OK,SAVED");
    return;
  }

  if (line == "LOAD") {
    loadSettings();
    sendConfig();
    return;
  }

  if (line == "CAL,GET") {
    sendCalConfig();
    return;
  }

  if (line == "CAL,START") {
    calMode = true;
    calCh1Min = 9999; calCh1Max = 0; calCh1Center = 0;
    calCh2Min = 9999; calCh2Max = 0; calCh2Center = 0;
    Serial.println("OK,CALSTART");
    return;
  }

  if (line == "CAL,STOP") {
    calMode = false;
    Serial.println("OK,CALSTOP");
    return;
  }

  if (line == "CAL,CENTER") {
    calCh1Center = lastCh1Raw;
    calCh2Center = lastCh2Raw;
    Serial.print("OK,CALCENTER,");
    Serial.print(calCh1Center);
    Serial.print(",");
    Serial.println(calCh2Center);
    return;
  }

  if (line == "CAL,SAVE") {
    // sah: min/max kena ada jarak cukup, dan center kena berada dalam julat min..max
    bool ch1Ok = (calCh1Max - calCh1Min >= CAL_MIN_RANGE) &&
                 (calCh1Center > calCh1Min) && (calCh1Center < calCh1Max);
    bool ch2Ok = (calCh2Max - calCh2Min >= CAL_MIN_RANGE) &&
                 (calCh2Center > calCh2Min) && (calCh2Center < calCh2Max);

    if (calCh1Center == 0 || calCh2Center == 0) {
      Serial.println("ERR,CAL,TIADA TITIK TENGAH - tekan CENTER dulu");
      return;
    }
    if (!ch1Ok) {
      Serial.println("ERR,CAL,JULAT CH1 TAK CUKUP - gerakkan stick lagi");
      return;
    }
    if (!ch2Ok) {
      Serial.println("ERR,CAL,JULAT CH2 TAK CUKUP - gerakkan stick lagi");
      return;
    }

    cfg.ch1Min = calCh1Min; cfg.ch1Center = calCh1Center; cfg.ch1Max = calCh1Max;
    cfg.ch2Min = calCh2Min; cfg.ch2Center = calCh2Center; cfg.ch2Max = calCh2Max;
    saveSettings();
    calMode = false;
    Serial.println("OK,CALSAVED");
    sendCalConfig();
    return;
  }

  if (line.startsWith("SET,")) {
    // format: SET,PARAM,VALUE
    int firstComma  = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    if (secondComma == -1) return; // format salah, abaikan

    String param = line.substring(firstComma + 1, secondComma);
    int value = line.substring(secondComma + 1).toInt();

    if (param == "FWD") {
      cfg.fwdTrim = value;
      Serial.print("OK,FWD,"); Serial.println(cfg.fwdTrim);
    } else if (param == "BWD") {
      cfg.bwdTrim = value;
      Serial.print("OK,BWD,"); Serial.println(cfg.bwdTrim);
    } else if (param == "MAX") {
      value = constrain(value, 0, 255);
      cfg.maxSpeed = value;
      Serial.print("OK,MAX,"); Serial.println(cfg.maxSpeed);
    } else if (param == "DEAD") {
      value = constrain(value, 0, 200);
      cfg.deadzone = value;
      Serial.print("OK,DEAD,"); Serial.println(cfg.deadzone);
    } else if (param == "TURN") {
      value = constrain(value, 0, 200);
      cfg.turnGain = value;
      Serial.print("OK,TURN,"); Serial.println(cfg.turnGain);
    } else if (param == "EXPO") {
      value = constrain(value, 0, 100);
      cfg.expo = value;
      Serial.print("OK,EXPO,"); Serial.println(cfg.expo);
    }
    return;
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      handleCommand(rxBuffer);
      rxBuffer = "";
    } else if (c != '\r') {
      rxBuffer += c;
      if (rxBuffer.length() > 64) rxBuffer = ""; // safety, elak overflow line sampah
    }
  }
}

// ================= SETUP =================
void setup() {
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  Serial.begin(9600);
  loadSettings();
}

// ================= LOOP =================
void loop() {

  pollSerial();

  // Baca signal RC
  int ch1_value = pulseIn(CH1_PIN, HIGH, 25000);
  int ch2_value = pulseIn(CH2_PIN, HIGH, 25000);

  // FAILSAFE (kalau channel mati / tiada signal)
  if (ch1_value < 900 || ch1_value > 2100) ch1_value = 1500;
  if (ch2_value < 900 || ch2_value > 2100) ch2_value = 1500;

  lastCh1Raw = ch1_value;
  lastCh2Raw = ch2_value;

  // ================= MOD KALIBRASI =================
  // Motor auto-stop demi safety - tak drive robot masa kalibrasi stick
  if (calMode) {
    if (ch1_value < calCh1Min) calCh1Min = ch1_value;
    if (ch1_value > calCh1Max) calCh1Max = ch1_value;
    if (ch2_value < calCh2Min) calCh2Min = ch2_value;
    if (ch2_value > calCh2Max) calCh2Max = ch2_value;

    motorL.setSpeed(0);
    motorR.setSpeed(0);

    unsigned long now = millis();
    if (now - lastTelemetry >= TELEMETRY_INTERVAL) {
      lastTelemetry = now;
      Serial.print("CALTEL,");
      Serial.print(ch1_value); Serial.print(",");
      Serial.print(ch2_value); Serial.print(",");
      Serial.print(calCh1Min); Serial.print(",");
      Serial.print(calCh1Max); Serial.print(",");
      Serial.print(calCh2Min); Serial.print(",");
      Serial.println(calCh2Max);
    }
    return; // skip mixing/drive biasa masa kalibrasi
  }

  // Mapping asymmetric guna nilai kalibrasi tersimpan
  int steering = mapChannel(ch1_value, cfg.ch1Min, cfg.ch1Center, cfg.ch1Max);
  int throttle = mapChannel(ch2_value, cfg.ch2Min, cfg.ch2Center, cfg.ch2Max);

  // Deadzone (check atas raw value dulu, sebelum expo ubah bentuk curve)
  if (abs(steering) < cfg.deadzone) steering = 0;
  if (abs(throttle) < cfg.deadzone) throttle = 0;

  // Expo curve (lembutkan respon dekat tengah stick)
  steering = applyExpo(steering, cfg.expo);
  throttle = applyExpo(throttle, cfg.expo);

  // Turn gain (buat pusing lebih/kurang tajam)
  steering = (steering * cfg.turnGain) / 100;
  steering = constrain(steering, -255, 255);

  // ================= PILIH TRIM IKUT ARAH =================
  int trim = 0;
  if (throttle > 0)      trim = cfg.fwdTrim;   // gerak depan
  else if (throttle < 0) trim = cfg.bwdTrim;   // gerak belakang

  // ================= MIXING =================
  int leftMotor  = throttle + steering - trim;
  int rightMotor = throttle - steering + trim;

  // Constrain guna MAX_SPEED dari cfg (boleh tune live)
  leftMotor  = constrain(leftMotor,  -cfg.maxSpeed, cfg.maxSpeed);
  rightMotor = constrain(rightMotor, -cfg.maxSpeed, cfg.maxSpeed);

  // ================= DRIVE =================
  motorL.setSpeed(leftMotor);
  motorR.setSpeed(rightMotor);

  // ================= TELEMETRY (berkala, elak flood serial) =================
  unsigned long now = millis();
  if (now - lastTelemetry >= TELEMETRY_INTERVAL) {
    lastTelemetry = now;
    Serial.print("TEL,");
    Serial.print(ch1_value); Serial.print(",");
    Serial.print(ch2_value); Serial.print(",");
    Serial.print(steering);  Serial.print(",");
    Serial.print(throttle);  Serial.print(",");
    Serial.print(leftMotor); Serial.print(",");
    Serial.println(rightMotor);
  }
}
