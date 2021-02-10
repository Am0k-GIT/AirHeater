#include <Arduino.h>
#include <avr/wdt.h>                                                           // Подключаем watchdog

#define Mode1PIN 5                                                             // PIN управления режимом 1 (0 - выкл, 1 - вкл.)
#define Mode2PIN 6                                                             // PIN управления режимом 2 (0 - выкл, 1 - вкл.)
#define FuelPIN 7                                                              // PIN управления топливным клапаном (0 - выкл, 1 - вкл.)
#define SparkPlugPIN 8                                                         // PIN управления свечей (0 - выкл, 1 - вкл.)
#define LEDPIN 9                                                               // PIN для подключения светодиода
#define OverheatSensorPIN A0                                                   // PIN для подключения датчика перегрева (1 - норма, 0 - авария)
#define WorkSensorPIN A1                                                       // PIN для подключения датчика перегрева (0 - работа, 1 - нет)

//#define WDT_ENABLE                                                           // Включаем WDT (не работает на старых bootloader'ах)
//#define DEBUG_ENABLE

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println(x)
#else
#define DEBUG(x)
#endif

uint16_t timeBlow = 20;                                                        // Время работы 2 режима при запуске
uint16_t timeSpark = 15;                                                       // Время работы свечей при запуске
uint16_t timeDelay = 5;                                                        // Задержка включения 1 режима при запуске
volatile uint8_t workMode = 0;                                                 // Переменная для хранения текущего режима работы
volatile bool flagStartButtonAvailable = true;                                 // Флаг доступности кнопки запуска
volatile bool startMode = false;                                               // Флаг режима запуска
volatile bool stopMode = false;                                                // Флаг режима остановки
volatile bool flagTimeLedAvailable = true;                                     // Флаг доступности таймера изменения состояния светодиода "Работа"
volatile bool LED_status = false;
unsigned long startTimer;                                                      // Переменная для хранения времени начала запуска
unsigned long LedTimer;

void startButton();                                                            // Прототип функции
void changeMode();                                                             // Прототип функции

void setup() {
  pinMode(Mode1PIN, OUTPUT);                                                   // Настраиваем порт для вывода
  pinMode(Mode2PIN, OUTPUT);                                                   // Настраиваем порт для вывода
  pinMode(FuelPIN, OUTPUT);                                                    // Настраиваем порт для вывода
  pinMode(SparkPlugPIN, OUTPUT);                                               // Настраиваем порт для вывода
  pinMode(LEDPIN, OUTPUT);                                                     // Настраиваем порт для вывода
  pinMode(OverheatSensorPIN, INPUT_PULLUP);                                    // Настраиваем порт для ввода (включаем внутреннюю подтяжку к +5V)
  pinMode(WorkSensorPIN, INPUT_PULLUP);                                        // Настраиваем порт для ввода (включаем внутреннюю подтяжку к +5V)
  attachInterrupt(1, startButton, CHANGE);                                     // Запускаем отслеживание прерываний (нажатие кнопки "Старт" 3 PIN)
  attachInterrupt(0, changeMode, CHANGE);                                      // Запускаем отслеживание прерываний (нажатие кнопки "Режим" 2 PIN)
  #ifdef DEBUG_ENABLE
  Serial.begin(9600);
  #endif
  #ifdef WDT_ENABLE
  wdt_enable(WDTO_4S);                                                         // Включаем watchdog на 4 секунды
  #endif
}

//============ Управление режимом светодиода "Работа" =========================
void LED_func(uint16_t time) {
 if (flagTimeLedAvailable) {
   flagTimeLedAvailable = false;
   LedTimer = millis();
   LED_status = !LED_status;
   digitalWrite (LEDPIN, LED_status);
 }
 else {
   if ((millis() - LedTimer) > (time)) {
     flagTimeLedAvailable = true;
   }
 }
}
//=============================================================================

//================== Функция для управления внешними подключениями ============
void IOcontrol (bool Mode1PINstate, bool Mode2PINstate, bool FuelPINstate, bool SparkPlugPINstate) {
  digitalWrite (Mode1PIN, Mode1PINstate);
  digitalWrite (Mode2PIN, Mode2PINstate);
  digitalWrite (FuelPIN, FuelPINstate);
  digitalWrite (SparkPlugPIN, SparkPlugPINstate);
}
//=============================================================================

//============ Режим запуска ==================================================
void start_func() {
  if (flagStartButtonAvailable) {                                              // Если кнопка еще доступна (мы попали в функцию в первый раз)
    DEBUG("Start function called");
    startTimer = millis();                                                     // Записываем время (однократно для запуска)
    flagStartButtonAvailable = false;                                          // Кнопка запуска недоступна
  }
  if ((millis() - startTimer) < (timeBlow * 1000)) {                           // Если прошло меньше времени чем timeBlow
    DEBUG("BLOW ( IOcontrol 0,1,0,0 )");
    IOcontrol (0,1,0,0);                                                       // Управляем внешними подключениями
    LED_func(800);
  }
  else {                                                                       // Прошло больше времени чем timeBlow
    if ((millis() - startTimer) < ((timeBlow + timeDelay) * 1000)) {           // Прошло меньше времени чем timeBlow + timeDelay
      DEBUG("Spark ( IOcontrol 0,0,0,1 )");
      IOcontrol (0,0,0,1);                                                     // Управляем внешними подключениями
      LED_func(300);
    }
    else {                                                                     // Прошло больше времени чем timeBlow + timeDelay
      if ((millis() - startTimer) < ((timeBlow + timeSpark) * 1000)) {         // Прошло меньше времени чем timeBlow + timeSpark
        DEBUG("Spark + Fuel ( IOcontrol 1,0,1,1 )");
        IOcontrol (1,0,1,1);                                                   // Управляем внешними подключениями
        LED_func(100);
      }
      else {                                                                   // Прошло больше времени чем timeBlow + timeSpark
        DEBUG("Start end");
        startMode = false;                                                     // Запуск завершен
        flagStartButtonAvailable = true;                                       // Кнопка запуска доступна
        workMode = 1;                                                          // Режим работы 1
      }
    }
  }
}
//=============================================================================

//============ Режим остановки ================================================
void stop_func() {
  flagStartButtonAvailable = false;                                            // Делаем кнопки недоступными
  if (!digitalRead(WorkSensorPIN)) {                                           // Если на выходе датчика работы 0 (он нагрет)
    DEBUG("STOP BLOW ( IOcontrol 0,1,0,0 )");
    IOcontrol (0,1,0,0);                                                       // Управляем внешними подключениями
    LED_func(1000);
  }
  else {                                                                       // Если на выходе датчика работы 1 (он остыл)
    workMode = 0;                                                              // Режим работы переводим в выключенно
    stopMode = false;                                                          // Режим остановки завершен
    flagStartButtonAvailable = true;                                           // Кнопки снова доступны
  }
}
//=============================================================================

void loop() {
  if (!digitalRead(OverheatSensorPIN)) {                                       // Если сработал датчик перегрева
    workMode = 0;                                                              // Режим работы 0
    DEBUG("Overheat");
  }
  else {                                                                       // Перегрева нет
    if (startMode) {                                                           // Если включен запуск
      start_func();                                                            // Запускаем
    }
    if (stopMode) {                                                            // Если включена остановка
      stop_func();                                                             // Останавливаем
    }
    if (!startMode && !stopMode) {                                             // Если это не режим запуска или остановки
      switch (workMode) {                                                      // В зависимости от режима работы управляем внешними подключениями
        case 0:
        IOcontrol (0,0,0,0);
        DEBUG("Work Mode 0 (off)");
        break;
        case 1:
        IOcontrol (1,0,1,0);
        DEBUG("Work Mode 1");
        break;
        case 2:
        IOcontrol (0,1,1,0);
        DEBUG("Work Mode 2");
        break;
      }
      digitalWrite (LEDPIN, !digitalRead(WorkSensorPIN));
    }
  }
  #ifdef WDT_ENABLE
  wdt_reset();                                                                 // Сбрасываем watchdog 
  #endif
}

//============ Нажатие кнопки Старт ===========================================
void startButton() {
  if (flagStartButtonAvailable) {
    DEBUG("Start button press");
    static unsigned long millis_prev;
    if (millis() - 1000 > millis_prev) {
      if ((workMode == 0) && !(startMode)) {
        startMode = true;
      }
      else {
        stopMode = true;
      }
    }
    millis_prev = millis();
  }
}
//=============================================================================

//============ Нажатие кнопки Режим ===========================================
void changeMode() {
  if (flagStartButtonAvailable) {
    DEBUG("Change mode button press");
    static unsigned long millis_prev;
    if (millis() - 1000 > millis_prev) {
      if (workMode == 1) {
        workMode = 2;
      }
      else if (workMode == 2) {
        workMode = 1;
      }
    }
    millis_prev = millis();
  }
}
//=============================================================================