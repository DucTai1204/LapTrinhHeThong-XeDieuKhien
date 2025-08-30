#include "BluetoothSerial.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

BluetoothSerial SerialBT;

// Chân điều khiển L298N
#define IN1  26   // Motor trái
#define IN2  25
#define ENA  27   // PWM trái

#define IN3  33   // Motor phải
#define IN4  32
#define ENB  14   // PWM phải

// Biến toàn cục được bảo vệ bởi mutex
int speedValue = 200; // Tốc độ mặc định (0-255)
char currentCommand = 'S'; // Lệnh hiện tại
char lastCommand = 'S'; // Lệnh cuối cùng

// FreeRTOS handles
QueueHandle_t commandQueue;
SemaphoreHandle_t motorMutex;
TaskHandle_t bluetoothTaskHandle;
TaskHandle_t serialTaskHandle;
TaskHandle_t motorTaskHandle;
TaskHandle_t statusTaskHandle;

// Cấu trúc lệnh
struct Command {
  char cmd;
  unsigned long timestamp;
};

void setup() {	
  Serial.begin(115200);
  delay(1000);
  
  // Khởi tạo Bluetooth
  SerialBT.begin("ESP32_BT_CAR"); // Tên xuất hiện trên điện thoại
  Serial.println("==== ESP32 BLUETOOTH CAR with FreeRTOS ====");
  Serial.println("Device name: ESP32_BT_CAR");
  Serial.println("Waiting for connection...");
  printCommands();

  // Cấu hình chân GPIO
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Dừng motor ban đầu
  stopMotors();
  Serial.println("Motors initialized - Ready!");

  // Tạo Queue và Mutex
  commandQueue = xQueueCreate(10, sizeof(Command));
  motorMutex = xSemaphoreCreateMutex();

  if (commandQueue == NULL || motorMutex == NULL) {
    Serial.println("ERROR: Failed to create Queue or Mutex!");
    while(1);
  }

  // Tạo các Task
  xTaskCreatePinnedToCore(
    bluetoothTask,      // Function
    "BT_Task",          // Name
    4096,               // Stack size
    NULL,               // Parameters
    2,                  // Priority
    &bluetoothTaskHandle, // Handle
    0                   // Core 0
  );

  xTaskCreatePinnedToCore(
    serialTask,         // Function
    "Serial_Task",      // Name
    4096,               // Stack size
    NULL,               // Parameters
    2,                  // Priority
    &serialTaskHandle,  // Handle
    0                   // Core 0
  );

  xTaskCreatePinnedToCore(
    motorTask,          // Function
    "Motor_Task",       // Name
    4096,               // Stack size
    NULL,               // Parameters
    3,                  // Priority (cao hơn để phản hồi nhanh)
    &motorTaskHandle,   // Handle
    1                   // Core 1
  );

  xTaskCreatePinnedToCore(
    statusTask,         // Function
    "Status_Task",      // Name
    2048,               // Stack size
    NULL,               // Parameters
    1,                  // Priority thấp
    &statusTaskHandle,  // Handle
    0                   // Core 0
  );

  Serial.println("All tasks created successfully!");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  // Loop() trống vì tất cả xử lý đã chuyển sang tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// ========== TASKS ==========

// Task xử lý Bluetooth
void bluetoothTask(void* parameter) {
  Serial.println("Bluetooth Task started on Core: " + String(xPortGetCoreID()));
  
  while(1) {
    if (SerialBT.available()) {
      char command = SerialBT.read();
      
      Command cmd;
      cmd.cmd = command;
      cmd.timestamp = millis();
      
      // Gửi lệnh vào queue
      if (xQueueSend(commandQueue, &cmd, portMAX_DELAY) != pdTRUE) {
        Serial.println("BT: Failed to send command to queue");
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Task xử lý Serial Monitor
void serialTask(void* parameter) {
  Serial.println("Serial Task started on Core: " + String(xPortGetCoreID()));
  
  while(1) {
    if (Serial.available()) {
      char command = Serial.read();
      
      Command cmd;
      cmd.cmd = command;
      cmd.timestamp = millis();
      
      // Gửi lệnh vào queue
      if (xQueueSend(commandQueue, &cmd, portMAX_DELAY) != pdTRUE) {
        Serial.println("Serial: Failed to send command to queue");
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Task điều khiển Motor (Priority cao nhất)
void motorTask(void* parameter) {
  Serial.println("Motor Task started on Core: " + String(xPortGetCoreID()));
  Command receivedCmd;
  
  while(1) {
    // Đọc lệnh từ queue
    if (xQueueReceive(commandQueue, &receivedCmd, 10 / portTICK_PERIOD_MS) == pdTRUE) {
      
      // Lock mutex trước khi xử lý
      if (xSemaphoreTake(motorMutex, portMAX_DELAY) == pdTRUE) {
        processCommand(receivedCmd.cmd);
        xSemaphoreGive(motorMutex);
      }
    }
    
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// Task hiển thị trạng thái (Priority thấp)
void statusTask(void* parameter) {
  Serial.println("Status Task started on Core: " + String(xPortGetCoreID()));
  TickType_t lastWakeTime = xTaskGetTickCount();
  
  while(1) {
    // Hiển thị trạng thái mỗi 5 giây
    if (xSemaphoreTake(motorMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
      Serial.printf("Status - Command: %c, Speed: %d, Free Heap: %d\n", 
                    currentCommand, speedValue, ESP.getFreeHeap());
      xSemaphoreGive(motorMutex);
    }
    
    vTaskDelayUntil(&lastWakeTime, 5000 / portTICK_PERIOD_MS);
  }
}

// ========== Hàm xử lý lệnh ==========
void processCommand(char command) {
  Serial.print("Processing: ");
  Serial.println(command);
  
  // Xử lý lệnh điều khiển tốc độ (1-9)
  if (command >= '1' && command <= '9') {
    int speed = (command - '0') * 28; // 1=28, 2=56, ..., 9=252
    speedValue = speed;
    Serial.print("Speed set to: ");
    Serial.println(speedValue);
    SerialBT.print("Speed: ");
    SerialBT.println(speedValue);
    return;
  }
  
  // Xử lý lệnh di chuyển
  switch (command) {
    case 'F':
    case 'f':
      moveForward();
      Serial.println("Moving Forward");
      SerialBT.println("Forward");
      currentCommand = 'F';
      break;
      
    case 'B':
    case 'b':
      moveBackward();
      Serial.println("Moving Backward");
      SerialBT.println("Backward");
      currentCommand = 'B';
      break;
      
    case 'L':
    case 'l':
      turnLeft();
      Serial.println("Turning Left");
      SerialBT.println("Left");
      currentCommand = 'L';
      break;
      
    case 'R':
    case 'r':
      turnRight();
      Serial.println("Turning Right");
      SerialBT.println("Right");
      currentCommand = 'R';
      break;
      
    case 'S':
    case 's':
      stopMotors();
      Serial.println("Stopped");
      SerialBT.println("Stop");
      currentCommand = 'S';
      break;
      
    default:
      Serial.print("Unknown command: ");
      Serial.println(command);
      return;
  }
  
  lastCommand = currentCommand;
}

// ========== Hàm điều khiển motor ==========
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void moveForward() {
  digitalWrite(IN1, HIGH);  // Motor trái tiến
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);  // Motor phải tiến
  digitalWrite(IN4, LOW);
  analogWrite(ENA, speedValue);
  analogWrite(ENB, speedValue);
}

void moveBackward() {
  digitalWrite(IN1, LOW);   // Motor trái lùi
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);   // Motor phải lùi
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, speedValue);
  analogWrite(ENB, speedValue);
}

void turnLeft() {
  digitalWrite(IN1, LOW);   // Motor trái dừng/lùi
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);  // Motor phải tiến
  digitalWrite(IN4, LOW);
  analogWrite(ENA, speedValue);
  analogWrite(ENB, speedValue);
}

void turnRight() {
  digitalWrite(IN1, HIGH);  // Motor trái tiến
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);   // Motor phải dừng/lùi
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, speedValue);
  analogWrite(ENB, speedValue);
}

void printCommands() {
  Serial.println("Commands:");
  Serial.println("F/f = Forward");
  Serial.println("B/b = Backward"); 
  Serial.println("L/l = Turn Left");
  Serial.println("R/r = Turn Right");
  Serial.println("S/s = Stop");
  Serial.println("1-9 = Speed control (1=slow, 9=fast)");
}