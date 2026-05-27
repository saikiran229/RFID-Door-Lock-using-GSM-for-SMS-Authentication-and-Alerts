#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <SPI.h>
#include <EEPROM.h> 

// Create instances
SoftwareSerial SIM900(3, 4); // SoftwareSerial SIM900(Rx, Tx)
MFRC522 mfrc522(10, 9); // MFRC522 mfrc522(SS_PIN, RST_PIN)
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo sg90;

// Initialize Pins for LEDs, servo, and buzzer
constexpr uint8_t greenLed = 7;
constexpr uint8_t redLed = 6;
constexpr uint8_t servoPin = 8;
constexpr uint8_t buzzerPin = 5;

const String tagUID = "2389992F";
String password = ""; 
String otp = "";    

boolean RFIDMode = true;
boolean NormalMode = true;

char key_pressed = 0; 
uint8_t i = 0;

// Defining how many rows and columns our keypad has
const byte rows = 4;
const byte columns = 4;

// Keypad pin map
char hexaKeys[rows][columns] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Initializing pins for keypad
byte row_pins[rows] = {A0, A1, A2, A3};
byte column_pins[columns] = {2, 1, 0};

// Create instance for keypad
Keypad keypad_key = Keypad(makeKeymap(hexaKeys), row_pins, column_pins, rows, columns);

void setup() {
  // Arduino Pin configuration
  pinMode(buzzerPin, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  sg90.attach(servoPin);
  sg90.write(0);

  lcd.init(); 
  lcd.backlight();
  SPI.begin();  
  mfrc522.PCD_Init(); 

  // Initialize GSM module
  SIM900.begin(9600); 
  SIM900.println("AT+CMGF=1");
  delay(100);
  SIM900.println("AT+CNMI=2,2,0,0,0");
  delay(100);
  lcd.clear();
}

void loop() {
  if (!NormalMode) {
    // Function to receive message
    receive_message();
  } else {
    if (RFIDMode) {
      lcd.setCursor(0, 0);
      lcd.print("   Door Lock");
      lcd.setCursor(0, 1);
      lcd.print(" Scan Your Tag ");

      // Look for new cards
      if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
      }

      // Reading from the card
      String tag = "";
      for (byte j = 0; j < mfrc522.uid.size; j++) {
        tag.concat(String(mfrc522.uid.uidByte[j] < 0x10 ? "0" : ""));
        tag.concat(String(mfrc522.uid.uidByte[j], HEX));
      }
      tag.toUpperCase();

      // Checking the card
      if (tag == tagUID) {
        otp = generateOTP(); //
        send_message("Your OTP is: " + otp);
        lcd.clear();
        lcd.print("Tag Matched");
        lcd.setCursor(0, 1);
        lcd.print("Enter OTP:");
        RFIDMode = false;
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wrong Tag");
        lcd.setCursor(0, 1);
        lcd.print("Access Denied");
        digitalWrite(buzzerPin, HIGH);
        digitalWrite(redLed, HIGH);
        send_message("Wrong tag detected.\nType 'close' to halt the system.");
        delay(3000);
        digitalWrite(buzzerPin, LOW);
        digitalWrite(redLed, LOW);
        lcd.clear();
      }
    } else {
      key_pressed = keypad_key.getKey(); 
      if (key_pressed) {
        if (key_pressed == '*') {
          password = ""; 
          lcd.clear();
          lcd.print("Enter OTP:");
          lcd.setCursor(0, 1);
          i = 0;
        } else if (key_pressed == '#') {
          if (password.length() == 4) {
            delay(200);
            if (password == otp) { 
              lcd.clear();
              lcd.print("OTP Accepted");
              sg90.write(90);
              digitalWrite(greenLed, HIGH);
              send_message("Door opened.\nIf it wasn't you, type 'close' to halt the system.");
              delay(3000);
              digitalWrite(greenLed, LOW);
              sg90.write(0); 
              lcd.clear();
              i = 0;
              RFIDMode = true;
            } else {
              lcd.clear();
              lcd.print("Wrong OTP");
              digitalWrite(buzzerPin, HIGH);
              digitalWrite(redLed, HIGH);
              send_message("Wrong OTP attempt.\nType 'close' to halt the system.");
              delay(3000);
              digitalWrite(buzzerPin, LOW);
              digitalWrite(redLed, LOW);
              lcd.clear();
              i = 0;
              RFIDMode = true; 
            }
          }
        } else {
          if (password.length() < 4) {
            password += key_pressed;
            lcd.print("*");
          }
        }
      }
    }
  }
}

// Function to generate a random 4-digit OTP
String generateOTP() {
  String otp = "";
  for (int i = 0; i < 4; i++) {
    otp += String(random(0, 10)); // Generate random digit between 0 and 9
  }
  return otp;
}

// Receiving the message
void receive_message() {
  String incomingData = "";

  if (SIM900.available() > 0) {
    incomingData = SIM900.readString();
    delay(10);
  }

  // if received command is to open the door
  if (incomingData.indexOf("open") >= 0) {
    sg90.write(90);
    NormalMode = true;
    send_message("Door opened.");
    delay(10000);
    sg90.write(0);
  }
  // if received command is to halt the system
  if (incomingData.indexOf("close") >= 0) {
    NormalMode = false;
    send_message("System halted.");
  }
}

// Function to send the message
void send_message(String message) {
  SIM900.println("AT+CMGF=1"); 
  delay(100);
  SIM900.print("AT+CMGS=\"+91XXXXXXXXXX\"\r"); // Replace with your mobile number
  delay(100);
  SIM900.println(message);
  delay(100);
  SIM900.write(26); 
  delay(100);
  SIM900.println();
}