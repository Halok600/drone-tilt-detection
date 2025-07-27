#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define I2C_SDA         8
#define I2C_SCL         9
#define GREEN_LED_PIN   4
#define RED_LED_PIN     5
#define BUZZER_PIN      6

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define TILT_THRESHOLD  45.0f
#define ALPHA           0.96f  

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

float roll = 0.0f, pitch = 0.0f;
uint32_t lastMicros = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("System Initialized");
  display.display();
  delay(1000);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lastMicros = micros();
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  uint32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  lastMicros = now;

  float accRoll  = atan2(a.acceleration.y, a.acceleration.z) * 57.2958f;
  float accPitch = atan2(-a.acceleration.x,
                         sqrt(a.acceleration.y * a.acceleration.y +
                              a.acceleration.z * a.acceleration.z)) * 57.2958f;


  float gyroRollRate  = g.gyro.x * 57.2958f; // rad/s -> deg/s
  float gyroPitchRate = g.gyro.y * 57.2958f;

  roll  = ALPHA * (roll  + gyroRollRate  * dt) + (1 - ALPHA) * accRoll;
  pitch = ALPHA * (pitch + gyroPitchRate * dt) + (1 - ALPHA) * accPitch;

  
  bool danger = (fabs(roll) > TILT_THRESHOLD || fabs(pitch) > TILT_THRESHOLD);

  if (danger) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    tone(BUZZER_PIN, 1000);
    updateDisplay("DANGER!", "Tilt > 45 deg");
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    noTone(BUZZER_PIN);
    updateDisplay("Status: Normal", "");
  }

  Serial.printf("Roll: %.2f | Pitch: %.2f\n", roll, pitch);
  delay(10);
}

void updateDisplay(String status, String details) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Roll: ");
  display.print(roll, 1);
  display.setCursor(64, 0);
  display.print("Pitch: ");
  display.print(pitch, 1);

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(15, 25);
  display.print(status);

  display.setTextSize(1);
  display.setCursor(20, 50);
  display.print(details);
  display.display();
}
