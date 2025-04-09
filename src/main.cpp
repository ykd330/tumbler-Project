/*-------include-------*/
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/*------소자 설정------*/

/*-----Display Setting-----*/
// OLED display 설정
#define SCREEN_WIDTH 128         // display 가로
#define SCREEN_HEIGHT 64         // display 세로
#define OLED_RESET -1            // reset pin
#define SSD1306_I2C_ADDRESS 0x3C // I2C 주소
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//OLED display 함수
void mainDisplayPrint(); // display 함수 선언부
void displayPrint(const char *text);

/*-----Temperature Sensor Setting-----*/
// DS18B20 온도 감지 센서 설정
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
static float temperatureC = 0; // 현재 온도 저장 변수
// SYSTEM 기본 온도 설정
float userSetTemperature = 30;

/*-----작동 모드-----*/
//시스템 작동 모드 설정
#define STOP_MODE 0 // 정지 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define KEEP_TEMPERATURE_MODE 2 // 유지 모드

//열전소자 작동 모드 설정 
#define HEATER_MODE 3 // 가열 모드
#define COOLER_MODE 4 // 냉각 모드
char control_mode = STOP_MODE; // 초기 모드 설정

/*-----GPIO Pin 설정부-----*/
// Interrupt 핀 설정
#define BUTTON_UP 5   // GPIO 5번에 연결, 설정온도 상승
#define BUTTON_DOWN 6 // GPIO 6번에 연결, 설정온도 하강

// 모드 변경 버튼 설정
#define BUTTON_ENTER 7 // GPIO 7번에 연결, 모드 변경 버튼
volatile static char lastControlMode = STOP_MODE; // 마지막 모드 저장 변수
volatile static char user_control_mode = STOP_MODE; // 사용자 선택 모드 저장 변수 / 기본 : 정지 모드
volatile static char user_enter = false; // 사용자 선택 모드 저장 변수 / 기본 : 정지 모드
volatile static char keep_stop = false; // 사용자 모드 저장 변수 / 기본 : 정지 모드

// 열전소자 모드 제어
#define HEATER_PIN 3 // 가열 모드
#define COOLER_PIN 2 // 냉각 모드
#define ACTIVE_PIN 1 // 활성화 핀 (가열/냉각 모드에 따라 설정)



/*-----시스템 한계 온도 설정-----*/
#define MAX_TEMPERATURE 125 // 최대 온도 125'C
#define MIN_TEMPERATURE -55 // 최소 온도 -55'C
#define SYSTEM_MIN_TEMPERATURE 15 // 시스템 최소 온도 15'C
#define SYSTEM_MAX_TEMPERATURE 85 // 시스템 최대 온도 85'C

//PWM 설정
#define PWM_FREQ 5000 // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define PWM_CHANNEL 0 // PWM 채널 설정 (0번 채널 사용)
#define PWM_PIN 2 // PWM 핀 설정 (GPIO 2번 사용)
float pwmValue = 0;

// Interrupt 버튼 함수 선언부
void IRAM_ATTR upButtonF();
void IRAM_ATTR downButtonF();
void IRAM_ATTR bootButtonF();
void changeControlMode(char control_mode); // 모드 변경 함수
// Interrupt 버튼 변수 선언부
unsigned char bootButton = false;
unsigned char upButton = false; // 설정온도 상승 버튼 상태 변수
unsigned char downButton = false; // 설정온도 하강 버튼 상태 변수


// active pin 설정
char activecontrol = 0; // 활성화 핀 상태 변수

/*
---GPIO핀 사용 현황---
GPIO 3 : PWM 핀
GPIO 4 : DS18B20 데이터 핀
GPIO 5 : 설정온도 상승 버튼 핀
GPIO 6 : 설정온도 하강 버튼 핀
GPIO 7 : 확인
GPIO 8 : DISPLAY 핀 I2C SCL
GPIO 9 : DISPLAY 핀 I2C SDA 
GPIO 10 : 가열 모드 핀 (OUTPUT) -> 3
GPIO 11 : 냉각 모드 핀 (OUTPUT) -> 2
GPIO 12 : 시스템 설정 핀 -> 1
*/

/*------setup------*/
void setup()
{
  Serial.begin(115200);
  /*-----pinMode INPUT_PULLUP-----*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(ACTIVE_PIN, INPUT_PULLUP);

  /*-----pinMode OUTPUT-----*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);

  /*------DS18B20설정부------*/
  sensors.begin(); // DS18B20 센서 초기화
  sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
  sensors.requestTemperatures(); // 온도 측정 요청
  
  /*------display설정부------*/
  if (!display.begin(SSD1306_I2C_ADDRESS, SSD1306_I2C_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, FALLING);
  attachInterrupt(BUTTON_DOWN, downButtonF, FALLING);
  attachInterrupt(ACTIVE_PIN, bootButtonF, FALLING); //

  /*------PWM설정부------*/
  pinMode(PWM_PIN, OUTPUT); // PWM 핀 설정
  //ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  //ledcAttachPin(PWM_PIN, PWM_CHANNEL); // PWM 핀과 채널 연결
  
  ledcWrite(PWM_CHANNEL, pwmValue); // 초기 PWM 값 설정
}

/*------loop-------*/
void loop()
{ 

  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static float lastTemperature = 0; // 마지막 온도 저장 변수
  temperatureC = 50;
  volatile static float setTemperature = userSetTemperature;
  volatile static char lastmode = STOP_MODE; // 마지막 모드 저장 변수

  /*-----온도 측정부-----*/
  /*if(sensors.isConversionComplete()){
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures(); // 다음 측정을 위해 온도 요청
  }*/
  /*-----온도 센서 오류 발생 시 오류 메세지 출력-----*/
  /*if (temperatureC == DEVICE_DISCONNECTED_C)
  {
    Serial.println("Error: Sensor not found!");
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Sensor Error");
    display.display();
    delay(1000); // 1초 대기 후 다시 시도
    return;      // 에러 발생 시 루프 종료
  }*/


  /*-----User Mode Setting I/O-----*/
  if(control_mode == STOP_MODE) // 정지 모드일 때
  { 
    user_enter = false; // 사용자 모드 설정 초기화
    mainDisplayPrint();
    display.display(); // display 출력
    if (upButton == true) 
    {
    user_control_mode += 1; // upButton이 눌리면 모드 증가
      if (user_control_mode > 2) 
        user_control_mode = 0; // 상태 초기화
      Serial.println("change mode");
      upButton = false; // upButton 초기화
    }
    if(downButton == true) 
    { // downButton이 눌리면 모드 변경
      if (user_control_mode > 0) 
        user_control_mode -= 1; // downButton이 눌리면 모드 감소
      if (user_control_mode == 0)
        user_control_mode = 2; // 상태 초기화
      Serial.println("change mode");
      downButton = false; // downButton 초기화
    }
    if (bootButton == true) 
    { // bootButton이 눌리면 모드 변경
      control_mode = user_control_mode; // 모드 변경
    }
    lastmode = STOP_MODE;
  }

   /*-----------PWM 설정부 / 동작----------*/
  //ACTIVE_MODE 동작
  if (control_mode == ACTIVE_MODE) { 
    pwmValue = map(temperatureC, lastTemperature, setTemperature, 0, 255); // 온도에 따라 PWM 값 설정
    if (bootButton == true) { // bootButton이 눌리면 모드 변경
      setTemperature = userSetTemperature; // 실행
      bootButton = false; // bootButton 초기화
    }
    if (temperatureC < setTemperature) // 현재 온도가 설정 온도보다 낮을 때
    {
      pwmValue = map(temperatureC, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      //ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
      displayPrint("Heating");
      displayPrint("Heating.");
      displayPrint("Heating..");
      displayPrint("Heating...");
    }
    else if (temperatureC > setTemperature) // 현재 온도가 설정 온도보다 높을 때
    {
      pwmValue = map(SYSTEM_MAX_TEMPERATURE - temperatureC, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      //ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
      displayPrint("Cooling");
      displayPrint("Cooling.");
      displayPrint("Cooling..");
      displayPrint("Cooling...");
    }
    else if(temperatureC == setTemperature) // 현재 온도가 설정 온도와 같을 때
    {
      pwmValue = 0; // PWM 값 0으로 설정
      control_mode = KEEP_TEMPERATURE_MODE; // 유지 모드로 변경
      lastTemperature = temperatureC; // 마지막 온도 업데이트
      //ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    }
    //버튼 작동 부분  
    if(upButton == true) // 설정온도 상승 버튼이 눌리면
    {
      userSetTemperature += 1; // 설정온도 상승
      if(userSetTemperature > MAX_TEMPERATURE) // 최대 온도 초과 시
        userSetTemperature = MAX_TEMPERATURE; // 최대 온도로 설정
      upButton = false; // 버튼 초기화
    }
    if(downButton == true) // 설정온도 하강 버튼이 눌리면
    {
      userSetTemperature -= 1; // 설정온도 하강
      if(userSetTemperature < MIN_TEMPERATURE) // 최소 온도 초과 시
        userSetTemperature = MIN_TEMPERATURE; // 최소 온도로 설정
      downButton = false; // 버튼 초기화
    }
    lastmode = ACTIVE_MODE; // 마지막 모드 업데이트
  }

  //KEEP_TEMPERATURE_MODE 동작
  if (control_mode == KEEP_TEMPERATURE_MODE) { // 유지 모드일 때
    setTemperature = temperatureC;
    if (temperatureC > setTemperature) // 현재 온도가 설정 온도보다 낮을 때  
    {
      if (lastTemperature-3 > temperatureC)
      {
      pwmValue = map(lastTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 128); // PWM 값 설정
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      //ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
      }
    }
    else if (temperatureC < setTemperature) // 현재 온도가 설정 온도보다 높을 때
    {
      if (lastTemperature+3 < temperatureC) // 마지막 온도와 현재 온도가 같을 때
      {
        pwmValue = map(SYSTEM_MAX_TEMPERATURE - lastTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 128); // PWM 값 설정
        changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      //ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
      }
    }
    else if(temperatureC == setTemperature) // 현재 온도가 설정 온도와 같을 때
    {
      pwmValue = 0; // PWM 값 0으로 설정
      //ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    }
    if(upButton == true) // 설정온도 상승 버튼이 눌리면
    {
      userSetTemperature += 1; // 설정온도 상승
      if(userSetTemperature > MAX_TEMPERATURE) // 최대 온도 초과 시
        userSetTemperature = MAX_TEMPERATURE; // 최대 온도로 설정
      upButton = false; // 버튼 초기화
    }
    if(downButton == true) // 설정온도 하강 버튼이 눌리면
    {
      userSetTemperature -= 1; // 설정온도 하강
      if(userSetTemperature < MIN_TEMPERATURE) // 최소 온도 초과 시
        userSetTemperature = MIN_TEMPERATURE; // 최소 온도로 설정
      downButton = false; // 버튼 초기화
    }
    if (bootButton == true) { // bootButton이 눌리면 모드 변경
      setTemperature = userSetTemperature; // 모드 변경
      control_mode = ACTIVE_MODE;
      bootButton = false; // bootButton 초기화
    }
    lastmode = KEEP_TEMPERATURE_MODE; // 마지막 모드 업데이트
  }
  delay(100); // 100ms 대기
}
/*-----loop 종료-----*/

/*-----display 출력 간소화 함수-----*/
void displayPrint(const char *text)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(text);
  //
  display.setTextSize(0.5);
  display.setCursor(0, 20);
  display.println("setTemperature: ");
  display.setCursor(30, 30);
  display.print(userSetTemperature);
  display.println(" C ");
  display.println("Temperature: ");
  display.setCursor(30, 50);
  display.print(temperatureC);
  display.println(" C");
  display.display();
}

/*-----Main Display Print-----*/
void mainDisplayPrint()
{
  display.clearDisplay(); // display 초기화
  display.setTextSize(2); // 텍스트 크기 설정
  display.setCursor(0, 0); // 커서 위치 설정
  display.println("Set Mode");
  display.setTextSize(1); // 텍스트 크기 설정
  display.setCursor(0, 25); // 커서 위치 설정
  if(user_control_mode == STOP_MODE) {
    display.write("STOP_MODE");
  }
  else if(user_control_mode == ACTIVE_MODE) {
    display.write("ACTIVE_MODE");
  }
  else if(user_control_mode == KEEP_TEMPERATURE_MODE) {
    display.write("KEEP_MODE");
  }
  display.setCursor(0, 50); // 커서 위치 설정
  display.print(temperatureC);
  display.print(" C ");
}

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTimeUp = 0;   // BUTTON_UP 디바운싱 시간
volatile unsigned long lastDebounceTimeDown = 0; // BUTTON_DOWN 디바운싱 시간
volatile unsigned long lastDebounceTimeMode = 0; // BUTTON_MODE 디바운싱 시간
volatile unsigned long lastActiveTime = 0;       // ACTIVE_PIN 디바운싱 시간
const unsigned long debounceDelay = 500;          // 디바운싱 지연 시간 (밀리초)

/*-----Interrupt 함수 정의 부분-----*/
//-> 추후 인터럽트 감지 / 실행부 함수로 분리할 필요 있음  
// 값을 감소시키는 버튼
void IRAM_ATTR downButtonF() //
{ 
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeDown > debounceDelay)
  {
    lastDebounceTimeDown = currentTime;
    downButton = true; // 설정온도 하강 버튼 상태 변수
  }
}
// 값을 증가시키는 버튼
void IRAM_ATTR upButtonF()
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeUp > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    upButton = true; // 설정온도 상승 버튼 상태 변수
  }
}

void IRAM_ATTR bootButtonF()
{
  unsigned long currentTime = millis();
  if (currentTime - lastActiveTime > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    bootButton = true;
  }
}


/*------모드 변경 함수------*/
void changeControlMode(char control_device_mode)
{
  if (control_device_mode == HEATER_MODE)
  {
    Serial.println("Heater Mode");
    digitalWrite(HEATER_PIN, HIGH); // 가열 모드 핀 HIGH
    digitalWrite(COOLER_PIN, LOW);  // 냉각 모드 핀 LOW
  }
  else if (control_device_mode == COOLER_MODE)
  {
    Serial.println("Cooler Mode");
    digitalWrite(HEATER_PIN, LOW);  // 가열 모드 핀 LOW
    digitalWrite(COOLER_PIN, HIGH); // 냉각 모드 핀 HIGH
  }
  else if (control_device_mode == STOP_MODE)
  {
    Serial.println("Stop Mode");
    digitalWrite(HEATER_PIN, LOW);  // 가열 모드 핀 LOW
    digitalWrite(COOLER_PIN, LOW);  // 냉각 모드 핀 LOW
  }
  else
   return; // 유지 모드일 경우 아무 동작도 하지 않음
}
