/*-----------include-----------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
/*-----------include-----------*/

/*----------전역변수 / 클래스 선언부----------*/
/*-----Display Setting-----*/
#define SCREEN_WIDTH 128         // display 가로
#define SCREEN_HEIGHT 64         // display 세로
#define OLED_RESET -1            // reset pin
#define SSD1306_I2C_ADDRESS 0x3C // I2C 주소
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_RESET, OLED_RESET); // I2C 핀 설정

/*-----Temperature Sensor Setting-----*/
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
static float temperatureC = 0; // 현재 온도 저장 변수
float userSetTemperature = 30;

/*-----시스템 관리 / 제어용-----*/
//GPIO아님
#define STOP_MODE 0 // 정지 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define KEEP_TEMPERATURE_MODE 2 // 유지 모드

#define HEATER_MODE 3 // 가열 모드
#define COOLER_MODE 4 // 냉각 모드
char control_mode = STOP_MODE; // 초기 모드 설정

/*-----GPIO 설정 부-----*/
/*---ESP32-C3 SuperMini GPIO 핀 구성---*/
// GPIO 5 : A5 : MISO /   5V   :    : VCC
// GPIO 6 :    : MOSI /  GND   :    : GND
// GPIO 7 :    : SS   /  3.3V  :    :3.3V
// GPIO 8 :    : SDA  / GPIO 4 : A4 : SCK
// GPIO 9 :    : SCL  / GPIO 3 : A3 : 
// GPIO 10:    :      / GPIO 2 : A2 : 
// GPIO 20:    : RX   / GPIO 1 : A1 : 
// GPIO 21:    : TX   / GPIO 0 : A0 : 
//PWM, 통신 관련 핀은 임의로 설정 가능함

/*-----열전소자 전류 제어용 PWM / 출력 PIN 설정부-----*/
#define PWM_FREQ 5000 // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define PWM_CHANNEL 0 // PWM 채널 설정 (0번 채널 사용)
#define PWM_PIN 1 // PWM 핀 설정 (GPIO 1번 사용)
#define COOLER_PIN 2 // 냉각 제어
#define HEATER_PIN 3 // 가열 제어

/*-----Push Button 설정부-----*/
#define BUTTON_UP 5   // GPIO 5번에 연결, 설정온도 상승
#define BUTTON_DOWN 6 // GPIO 6번에 연결, 설정온도 하강
#define BUTTON_BOOT 7 // GPIO 7번에 연결, 모드 변경 버튼
float pwmValue = 0; // PWM 출력 값 저장 변수

/*-----시스템 한계 온도 설정-----*/
#define MAX_TEMPERATURE 125 // 최대 온도 125'C
#define MIN_TEMPERATURE -55 // 최소 온도 -55'C
#define SYSTEM_MIN_TEMPERATURE 5 // 시스템 최소 온도 5'C
#define SYSTEM_MAX_TEMPERATURE 80 // 시스템 최대 온도 80'C

/*-----Interrupt 버튼 triger 선언부-----*/
unsigned char bootButton = false;
unsigned char upButton = false; // 설정온도 상승 버튼 상태 변수
unsigned char downButton = false; // 설정온도 하강 버튼 상태 변수

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTime = 0;   // 마지막 디바운스 시간
const unsigned long debounceDelay = 500;          // 디바운싱 지연 시간 (밀리초)

/*-----Display 절전모드 제어용 변수-----*/
unsigned char displaySleep = false; // display 절전모드 상태 변수
float displaySleepTime = 0; // display 절전모드 시간 변수
/*----------전역변수 / 클래스 선언부----------*/



/*----------함수 선언부----------*/
/*------Display / Display Print 제어 함수 설정부------*/
/*-----Main Display Print-----*/
void mainDisplayPrint() //Main Display출력 함수
{
  
}

/*-----Smooth Display-----*/
void contrastUpDisplay()// 대비 조정 함수 UP
{
  for (int i = 0; i < 255; i++)
  {
    u8g2.setContrast(i); // 대비 조정
    u8g2.sendBuffer();   // 버퍼 전송
    delay(10);           // 5ms 대기
  }
}
void contrastDownDisplay()// 대비 조정 함수 DOWN
{
  for (int i = 255; i >= 0; i--)
  {
    u8g2.setContrast(i); // 대비 조정
    u8g2.sendBuffer();   // 버퍼 전송
    delay(10);           // 5ms 대기
  }
}

/*-----Display 기본 출력 함수-----*/
void displayPrint(const char *text)// display 출력 함수
{
  u8g2.setFont(u8g2_font_ncenB08_tr); // 폰트 설정
  u8g2.setCursor(0, 30);
  u8g2.println("설정온도 : ");
  u8g2.setCursor(u8g2.getCursorX() + 10, u8g2.getCursorY());
  u8g2.print(userSetTemperature);
  u8g2.print(" C ");
  u8g2.setCursor(0, u8g2.getCursorY() + 15);
  u8g2.println("현재온도 : ");
  u8g2.setCursor(u8g2.getCursorX() + 10, u8g2.getCursorY());
  u8g2.print(temperatureC);
  u8g2.println(" C");
}

/*------Interrupt 함수 정의 부분------*/
void IRAM_ATTR downButtonF() //Down Button Interrupt Service Routine
{ 
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay)
  {
    lastDebounceTime = currentTime;
    downButton = true; // 설정온도 하강 버튼 상태 변수
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
}
void IRAM_ATTR upButtonF() //Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime> debounceDelay)
  {
    lastDebounceTime = currentTime;
    upButton = true; // 설정온도 상승 버튼 상태 변수
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
}
void IRAM_ATTR bootButtonF() //Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay)
  {
    lastDebounceTime = currentTime;
    bootButton = true;
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
}

/*------가열 / 냉각 모드 변경 함수------*/
void changeControlMode(char control_device_mode) //열전소자 제어 함수
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
/*----------함수 선언부----------*/

/*----------시스템 구상----------*/
/*---GPIO핀 할당내용---*/
//GPIO 1 : PWM 핀 (OUTPUT) -> 1
//GPIO 2 : 냉각 신호 핀 (OUTPUT) -> 2
//GPIO 3 : 가열 신호 핀 (OUTPUT) -> 3
//GPIO 4 : DS18B20 데이터 핀
//GPIO 5 : 설정온도 상승 버튼 핀
//GPIO 6 : 설정온도 하강 버튼 핀
//GPIO 7 : Booting / Home 버튼 핀
//GPIO 8 : DISPLAY 핀 I2C SCL
//GPIO 9 : DISPLAY 핀 I2C SDA 

/*---Display Setting---*/
// OLED display 설정
//디스플레이 구현 시 고려할 점
  // 부드러운 전환을 위해 0.5초 간격으로 화면을 업데이트
  // 화면 전환 시 이전 화면을 지우고 새로운 화면을 그리는 방식으로 구현
  // 모드당 화면을 나눔 ; 온도 측정 시 화면에 표현
  // 시작시 전용 화면 출력 후
  // 1. 온도 체크 알림 화면
  // 2. 정지 화면 - 메인 화면임 
  // 3. 작동 중 화면 - 현재 온도 / 설정온도 표시 ; 1초 간격으로 업데이트 ; 가열 / 냉각 모드 표시
  // 4. 온도 유지 화면 - 설정 온도를 표시 / "{설정 온도}'C 유지중..." ; 1초 간격으로 업데이트 
  // 배터리 잔량 표시 - 가능시 추가
  // 온도 변환 시 전용 화면으로 전환 ; 전환 트리거 : 전원 버튼, 트리거 발생 시 화면 전환 후 버튼으로 
  // 온도 설정, 전원버튼 한번 더 누를 시 설정 온도에 따라 모드 변환;
//OLED display 함수

/*---Display Structure---*/
/* UI 만드는 사람들 ㄹㅇ 존경스럽다
starting Display :
|Smart Tumbler System      [Batery : 100%]|
|-----------------------------------------|
|제작 : 5조 임선진 안대현 유경도            |
|작품명 : Smart Tumbler System            |
|----------------------------------------|

basic Display :
|Smart Tumbler System      [Batery : 100%]|
|-----------------------------------------|
|현재 온도 : XX.X C                        |
|설정 온도 : XX.X C                        |
|대기중...(..개수 변화)                    |

active Display :
|Smart Tumbler System      [Batery : 100%]|
|-----------------------------------------|
|현재 온도 : XX.X C (가열 / 냉각 중)        |
|목표 온도 : XX.X C                        |
|온도 조절중...                            |

temperature maintanence Display :
|Smart Tumbler System      [Batery : 100%]|
|-----------------------------------------|
|현재 온도 : XX.X C (설정 온도 : )         |
|가열 / 냉각 상태                          |
|온도 유지중...                           |

Setting Temperature Display :
|Smart Tumbler System      [Batery : 100%]|
|-----------------------------------------|
|          목표 온도 : XX.X C             |
|       온도증가 : ▲ / 온도감소 : ▼        |
|완료하고 싶으시면 전원 버튼을 눌러주세요.   |

Ended Setting Display :
|Smart Tumbler System      [Batery : 100%]|
|-----------------------------------------|
|목표 온도 : XX.X C로 설정 완료 했어요!     |
|온도를 조절하는 동안 기다려주세요.         |
|※온도 조절 중 화상에 주의하세요!          |
|-----------------------------------------|


*/


/*----------시스템 구상----------*/

/*----------setup----------*/
void setup()
{
  Serial.begin(115200);
  /*------pinMode INPUT_PULLUP------*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);

  /*------pinMode OUTPUT------*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);

  /*------DS18B20설정부------*/
  sensors.begin(); // DS18B20 센서 초기화
  sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
  sensors.requestTemperatures(); // 온도 측정 요청
  
  /*------display설정부------*/
  u8g2.begin(); // display 초기화
  u8g2.enableUTF8Print(); // UTF-8 문자 인코딩 사용
  if (!u8g2.begin())
  {
    Serial.println("Display initialization failed!");
    for (;;); // 초기화 실패 시 무한 루프
  }
  
  u8g2.setFont(u8g2_font_ncenB08_tr); // 폰트 설정
  u8g2.setFontMode(1); // 폰트 모드 설정
  u8g2.setDrawColor(1); // 글자 색상 설정

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, FALLING);
  attachInterrupt(BUTTON_DOWN, downButtonF, FALLING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, FALLING); //

  /*------PWM설정부------*/
  pinMode(PWM_PIN, OUTPUT); // PWM 핀 설정
  //ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  //ledcAttachPin(PWM_PIN, PWM_CHANNEL); // PWM 핀과 채널 연결
  
  ledcWrite(PWM_CHANNEL, pwmValue); // 초기 PWM 값 설정
}
/*----------setup----------*/

/*----------loop----------*/
void loop()
{ 
  /*------Starting DisplayPrint------*/
  if(millis() < 3000) {
    u8g2.setPowerSave(0); // 절전모드 해제
    u8g2.setFont(u8g2_font_ncenB12_tr); // 폰트 설정
    u8g2.clearBuffer(); // 버퍼 초기화
    u8g2.setCursor(0, 10); // 커서 위치 설정
    u8g2.println("5조 : 임선진 안대현 유경도"); // 시작 메시지 출력
    u8g2.println("작품명 : Samrt Tumbler"); // 작품명 출력작
    u8g2.print("2025년 졸업작품작");
    contrastUpDisplay();
    delay(1000); // 100ms 대기
    contrastDownDisplay();
    delay(1000); // 100ms 대기
  }
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static float lastTemperature = 0; // 마지막 온도 저장 변수
  temperatureC = 50;
  volatile static float setTemperature = userSetTemperature;
  volatile static char lastmode = STOP_MODE; // 마지막 모드 저장 변수

  /*-----온도 측정부-----*/
  if(sensors.isConversionComplete()){
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures(); // 다음 측정을 위해 온도 요청
  }
  /*-----온도 센서 오류 발생 시 오류 메세지 출력-----*/
  if(temperatureC == DEVICE_DISCONNECTED_C)
  {
    u8g2.println("센서에 문제가 있어요");
    Serial.println("센서에 문제가 있어요");
    delay(3);
    while(1) {
    u8g2.clearBuffer();
    if(temperatureC != DEVICE_DISCONNECTED_C)
      break; // 센서가 정상적으로 연결되면 루프 종료
    u8g2.setCursor(0, 30);
    u8g2.print("센서 감지중");
    u8g2.print("센서 감지중.");
    u8g2.print("센서 감지중..");
    u8g2.print("센서 감지중...");
    u8g2.sendBuffer();
    delay(500); // 0.5초 대기 후 다시 시도
    }
  }


  /*------Main System Setting------*/
  /*-----Main Display for User-----*/
  

  /*-----PWM 설정부 / 동작-----*/


  /*-----ACTIVE_MODE 동작-----*/
  if (control_mode == ACTIVE_MODE) // 활성화 모드일 경우
  {
    if (temperatureC < userSetTemperature) // 현재 온도가 설정온도보다 낮을 경우
    {
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC > userSetTemperature) // 현재 온도가 설정온도보다 높을 경우
    {
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC == userSetTemperature) // 현재 온도가 설정온도와 같을 경우
    {
      changeControlMode(STOP_MODE); // 정지 모드로 변경
      ledcWrite(PWM_CHANNEL, 0); // PWM 출력 중지
    }
  }

  /*-----KEEP_TEMPERATURE_MODE 동작-----*/
  if(control_mode == KEEP_TEMPERATURE_MODE) // 유지 모드일 경우
  {
    if (temperatureC < userSetTemperature) // 현재 온도가 설정온도보다 낮을 경우
    {
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC > userSetTemperature) // 현재 온도가 설정온도보다 높을 경우
    {
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC == userSetTemperature) // 현재 온도가 설정온도와 같을 경우
    {
      changeControlMode(STOP_MODE); // 정지 모드로 변경
      ledcWrite(PWM_CHANNEL, 0); // PWM 출력 중지
    }
  }

  /*-----Display Energe Save Mode-----*/
  if (displaySleepTime + 10000 < millis()) // 10초 이상 버튼이 눌리지 않으면 절전모드로 전환
  {
    displaySleep = true; // 절전모드 상태 변수 설정
    u8g2.setPowerSave(1); // 절전모드 설정
  }
  else if (displaySleepTime + 10000 > millis()) // 버튼이 눌리면 절전모드 해제
  {
    displaySleep = false; // 절전모드 해제
    u8g2.setPowerSave(0); // 절전모드 해제
  }
}
/*----------loop----------*/
