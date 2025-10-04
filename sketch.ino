//defined libraries needed
#include <Arduino.h>  // Include the Arduino library
#include <Wire.h>  // Include the Wire library for I2C communication
#include <Adafruit_GFX.h>  // Include the Adafruit GFX library
#include <Adafruit_SSD1306.h> // Include the Adafruit SSD1306 library
#include <DHTesp.h>  // Include the DHT library
#include <WiFi.h>  // Include the WiFi library
#include <NTPClient.h>   // Include the NTPClient library
#include <WiFiUdp.h>   // Include the WiFi library
#include <ESP32Servo.h>  // Include the Servo library
#include <PubSubClient.h>  // Include the PubSubClient library for MQTT


// Define OLED display parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Define hardware pins
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 25
#define PB_UP 26
#define PB_DOWN 35
#define DHTPIN 12
#define PB_SNOOZE 16
#define LDR_PIN 33
#define SERVO_PIN 18

 

//responsible for server address,time zone,day light saving settings
#define NTP_SERVER     "pool.ntp.org"
int UTC_OFFSET = 19800;  //offset of UTC in seconds
#define UTC_OFFSET_DST 0

//define object to work with OLED display and DHT sensor
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

//Declared a function to print relevant texts
void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);  // 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Display white Text
  display.setCursor(column, row);  // Display from left top corner
  display.println(text);  //Print the relevant text message
  display.display();
}

// Global variable declaration
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

// Global variables for time management
unsigned long time_Now = 0;
unsigned long time_last = 0;

//Global variables for update_time_with_check_alarm function
bool alarm_enabled = true; 
int n_alarms = 2;
int alarm_hours[] = {0, 1}; 
int alarm_minutes[] = {1, 10};  
bool alarm_triggered[] = {false, false};

// Notes for buzzer
int n_notes = 8;
int C = 262, D = 294, E = 330, F = 349, G = 392, A = 440, B = 494, C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

// Menu system
int current_mode = 0;
int max_modes = 7;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Disable Alarms", "5 - Set Time Zone", "6 - View Alarms", "7 - Delete Alarm"}; 

char Tmp_Array[6]; // Temperature array to store temperature values
char LDR_Array [6]; //Array to store LDR values
char Servo_Angle_Array [6]; //Array to store Servo Angle

int Theta_offset; //The offset for the servo position.
float Controlling_Factor; //The Controlling Factor for LDR .
float MAX_LDR=4064.00; //Calibrated analog max value for LDR.
float MIN_LDR=32.00; //Calibrated analog min value for LDR.
int Sampling_interval = 5000; // Default: 5 seconds 
int Sending_interval = 120000; // Default: 120 seconds 
int T_med = 30; // Default value is 30°C



WiFiClient espClient;  // Create a WiFi client by initiating MQTT

PubSubClient mqttClient(espClient);  // Create a PubSubClient object for MQTT

WiFiUDP ntpUDP;  // Create a UDP object
NTPClient timeClient(ntpUDP);  // Create an NTP client object
Servo servo;  // Create a Servo object


void setupWifi(){      // Function to set up WiFi connection
  Serial.println();
  Serial.println("Connecting to WiFi");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");

  
  while (WiFi.status() != WL_CONNECTED){   // Wait for the WiFi connection to be established
    delay(500);
    Serial.print(".");
  }
  
  //print the IP address if wifi is connected 
  Serial.println("WiFi connected");
  Serial.println("TP address: ");
  Serial.println(WiFi.localIP());
}


void receive_Callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message has been arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length + 1]; // +1 for null terminator
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  payloadCharAr[length] = '\0'; // Null-terminate the string
  Serial.println();

  if (strcmp(topic, "220473X_sampling_interval") == 0) {
    int value = atoi(payloadCharAr);
    if (value > 0) {
      Sampling_interval = value * 1000; // Convert to milliseconds
      Serial.print("LDR reading of sampling interval set to: ");
      Serial.println(Sampling_interval);
    } else {
      Serial.println("Invalid SAMPLING INTERVAL Payload");
    }
  } 
  else if (strcmp(topic, "220473X_sending_interval") == 0) {
    float value = atof(payloadCharAr);
    if (value > 0) {
      Sending_interval = value * 60 * 1000; // Convert to milliseconds
      Serial.print("LDR reading of sending interval set to: ");
      Serial.println(Sending_interval);
    } else {
      Serial.println("Invalid SENDING INTERVAL Payload");
    }
  } 
  else if (strcmp(topic, "220473X_Theta-Offset") == 0) {
    float value = atof(payloadCharAr);
    if (value >= 0) {
      Theta_offset = value;
      Serial.print("Theta offset is set to: ");
      Serial.println(Theta_offset);
    } else {
      Serial.println("Invalid THETA OFFSET Payload");
    }
  }
  else if (strcmp(topic, "220473X_Controlling_Factor") == 0) {
    float value = atof(payloadCharAr);
    if (value >= 0) {
      Controlling_Factor = value;
      Serial.print("Controlling factor is set to: ");
      Serial.println(Controlling_Factor);
    } else {
      Serial.println("Invalid CONTROLING FACTOR Payload");
    }
  }
  else if (strcmp(topic, "220473X_T-MED") == 0) {
    float value = atof(payloadCharAr);
    if (value > 0) {
      T_med = value;
      Serial.print("T_med is set to: ");
      Serial.println(T_med);
    } else {
      Serial.println("Invalid T_MED Payload");
    }
  }
}

boolean connectToMQTTBroker(); // Define the function prototype with boolean return type

void setupMqtt(){
  // Set the MQTT broker address and port
  mqttClient.setServer("broker.hivemq.com", 1883);
  
  // Set the callback function for incoming messages
  mqttClient.setCallback(receive_Callback);
  
  // Initial connection attempt
  connectToMQTTBroker();
}


boolean connectToMQTTBroker(){
  // Initializing a unique client ID using millis() to avoid connection conflicts
  String clientId = "ESP32-Medibox-";
  clientId += String(random(0xffff), HEX);
  
  Serial.print("Attempting MQTT connection with ID: ");
  Serial.println(clientId);
  
  // Loop until connected
  int attempts = 0;
  while(!mqttClient.connected() && attempts < 5){  
    Serial.print("MQTT connection attempt #");   // Print the attempt number
    Serial.print(attempts + 1);
    Serial.print("...");
    
    // Attempt to connect with the unique client ID
    if(mqttClient.connect(clientId.c_str())){
      Serial.println("connected");
      
      // Subscribe to all required topics
      mqttClient.subscribe("220473X_sampling_interval");
      mqttClient.subscribe("220473X_sending_interval");
      mqttClient.subscribe("220473X_Theta-Offset");
      mqttClient.subscribe("220473X_Controlling_Factor");
      mqttClient.subscribe("220473X_T-MED");
      
      // Publish initial values to show we're connected
      mqttClient.publish("220473X_status", "Medibox connected");
      
      // Initialize default values for parameters
      Theta_offset = 30;       // Default offset angle
      Controlling_Factor = 0.75; // Default control factor
      
      // Publish initial parameters to Node-RED
      String(Theta_offset).toCharArray(Servo_Angle_Array, 6);
      mqttClient.publish("220473X_init_Theta_Offset", Servo_Angle_Array);
      
      String(Controlling_Factor).toCharArray(Tmp_Array, 6);
      mqttClient.publish("220473X_init_Controlling_Factor", Tmp_Array);
      
      return true; // Successfully connected
    }
    else{
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
      attempts++;
    }
  }
  
  // If we exited the loop without connecting, report failure
  if (!mqttClient.connected()) {
    Serial.println("Failed to connect to MQTT after multiple attempts");
    // Will try again in the main loop
  }
  return false; // Connection failed
}





void buzzerOn(bool on){
  if(on){
    tone(BUZZER, 256); // Turn on the buzzer
  }
  else{
    noTone(BUZZER); // Turn off the buzzer
  }
}






void updateTemperature(){
  
  //read the temperature and humidity
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  //convert the temperature to string
  String(data.temperature,2).toCharArray(Tmp_Array, 6);
}


void updateLight() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();  // Read temperature and humidity
  int temp = (int)data.temperature;    // Get temperature as an integer
  int rawLight = analogRead(LDR_PIN);   // Read raw light value from LDR

  float normalizedLight = (rawLight - MAX_LDR) / (MIN_LDR - MAX_LDR);   // Normalize the light value
  normalizedLight = constrain(normalizedLight, 0.0, 1.0);   // Constrain the value between 0 and 1

  Serial.print("rawLight: "); Serial.println(rawLight);
  Serial.print("normalizedLight: "); Serial.println(normalizedLight);
  Serial.print("temperature: "); Serial.println(temp);
  Serial.print("T_med: "); Serial.println(T_med);
  Serial.print("Theta_offset: "); Serial.println(Theta_offset);
  Serial.print("Controlling_Factor: "); Serial.println(Controlling_Factor);
  Serial.print("Sampling_interval: "); Serial.println(Sampling_interval);
  Serial.print("Sending_interval: "); Serial.println(Sending_interval);

  if (Sending_interval == 0) {  // Check if Sending_interval is 0
    Serial.println("Invalid input: Sending interval is 0");
    return;
  }
  if (T_med == 0) {   // Check if T_med is 0
    Serial.println("Invalid input: T_med is 0");
    return;
  }

  float ratio = (float)Sending_interval / Sampling_interval; // Reverse the ratio for positive log
  float logFactor = log10(ratio);
  Serial.print("ratio: "); Serial.println(ratio);
  Serial.print("logFactor: "); Serial.println(logFactor);
  Serial.print("temp / T_med: "); Serial.println(temp / (float)T_med);

  float servoCalc = Theta_offset + (180 - Theta_offset) * normalizedLight * Controlling_Factor * logFactor * (temp / (float)T_med);
  Serial.print("Calculated servoAngle before constrain: "); Serial.println(servoCalc);
  int servoAngle = constrain((int)servoCalc, 0, 180);
  Serial.print("Servoangle after constrain: "); Serial.println(servoAngle);

  servo.write(servoAngle);   // Write the calculated angle to the servo

  String(normalizedLight, 2).toCharArray(LDR_Array, 6);
  mqttClient.publish("220473X_Light_Intensity", LDR_Array);

  String(servoAngle).toCharArray(Servo_Angle_Array, 6);
  mqttClient.publish("220473X_servo_angle", Servo_Angle_Array);
}





void update_time_with_check_alarm(void); // Function prototype for update_time_with_check_alarm


//Declared a function to print the time
void print_time_now() {
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
  delay(500);
}


//Declared a function to update time
void update_time(void) {
  struct tm timeinfo;

  // Try to get the current local time
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time"); // Print error if time retrieval fails
    return; // Exit function
  }

  char timeDay[3], timeHour[3], timeMinute[3], timeSecond[3];  // Declare char arrays for time components

  // Extract day, hour, minute, and second from timeinfo structure
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);

  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

}

// function to check the Temperature and Humidity
void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (data.temperature > 32) {
    display.clearDisplay();
    print_line("TEMP HIGH", 0, 40, 1);
  }
  else if (data.temperature < 24) {
    display.clearDisplay();
    print_line("TEMP LOW", 0, 40, 1);
  }
  delay(500); //Blink interval for warnings


  if (data.humidity > 80) {
    display.clearDisplay();
    print_line("HUMIDITY HIGH", 0, 50, 1);
  }
  else if (data.humidity < 65) {
    display.clearDisplay();
    print_line("HUMIDITY LOW", 0, 50, 1);
  }

  delay(500); //Blink interval for warnings
}

// Declared a function to wait for button press
int wait_for_button_press(void) {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {   // If button up is pressed, return PB_UP
      delay(200);
      return PB_UP;  

    } 
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;

    } 
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;

    } 
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time(); 
  }
}

void set_timezone() {
  int temp_offset = UTC_OFFSET / 3600;  // Convert seconds to hours for display
  bool setting = true;  // Flag to control the setting loop
  
  while (setting) {
    display.clearDisplay();  // Clear the display for fresh output
    print_line("Set UTC Offset (hrs):", 0, 0, 1);  // Display prompt
    print_line(String(temp_offset), 0, 20, 2);  // Show current offset
    print_line("Up/Down for adjust", 0, 40, 1);  // Instructions for adjusting
    print_line("OK to confirm", 0, 50, 1);  // Instructions to confirm

    int pressed = wait_for_button_press();  // Wait for button press

    // Adjust the offset upwards
    if (pressed == PB_UP) {
      temp_offset += 1;  // Increment offset
      if (temp_offset > 14) temp_offset = -12;  // Wrap around maximum
    }
    // Adjust the offset downwards
    else if (pressed == PB_DOWN) {
      temp_offset -= 1;  // Decrement offset
      if (temp_offset < -12) temp_offset = 14;  // Wrap around minimum
    }
    // Confirm the selected offset
    else if (pressed == PB_OK) {
      UTC_OFFSET = temp_offset * 3600;  // Convert hours back to seconds
      
      // Reconfigure time with new offset
      configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
      
      display.clearDisplay();  // Clear display for confirmation
      print_line("UTC Offset Set:", 0, 0, 1);  // Confirmation message
      print_line(String(temp_offset) + " hours", 0, 20, 2);  // Show new offset
      delay(2000);  // Wait for 2 seconds to show confirmation
      setting = false;  // Exit the setting loop
    }
    // Cancel the setting
    else if (pressed == PB_CANCEL) {
      setting = false;  // Exit the setting loop
    }
  }
}

//Declared a function to set the time manually
void set_time() {
  int temp_hour = hours;
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 2,0,0);
    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {   
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    } 

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    } 
    
    else if (pressed == PB_OK) {
      delay(200);
      hours = temp_hour;
      break;
    } 
    
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  int temp_minute = minutes;

  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    } 
    
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    } 
    
    else if (pressed == PB_OK) {
      delay(200);
      minutes = temp_minute;
      break;
    } 
    
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line("Time is set", 2, 0, 0);
  delay(1000);
}


//declared a function to set the alarm
void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;   //If button up is pressed, increment the hour
      //If the hour exceeds 24, reset it to 0
      temp_hour = temp_hour % 24;
    } 
    
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1; //If button down is pressed, decrement the hour
      //If the hour is less than 0, reset it to 23
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    } 
    
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour; //Set the alarm hour to the selected hour
      break;
    } 
    
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;   //If cancel button is pressed, exit the loop
    }

  }

  int temp_minute = alarm_minutes[alarm];  //Get the current alarm minute
  //Loop to set the alarm minute
  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) { //If button up is pressed, increment the minute
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60; //If the minute exceeds 60, reset it to 0
    }

    else if (pressed == PB_DOWN) {   //If button down is pressed, decrement the minute
      delay(200);
      temp_minute -= 1;  
      if (temp_minute < 0) {
        temp_minute = 59;    //If the minute is less than 0, reset it to 59
      }
    } 

    else if (pressed == PB_OK) {    //If OK button is pressed, set the alarm minute
      delay(200);
      alarm_minutes[alarm] = temp_minute;    //Set the alarm minute to the selected minute
      break;
    } 

    else if (pressed == PB_CANCEL) {  //If cancel button is pressed, exit the loop
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);   //Display confirmation message
  delay(1000);
}

void ring_alarm(int alarm_index); // Function prototype for ring_alarm

void snooze_alarm(int alarm_index) {
  int snoozing_time = 5; // Initiated a variable to store 5 minutes snoozing time
  display.clearDisplay();
  print_line("SNOOZED 5 min", 20, 20, 1);
  delay(1000);

  // Wait for 5 minutes (300,000 milliseconds)
  delay(snoozing_time * 60000);
  //delay(300,000);

  // Call the ring alarm function to ring the alarm again after 5 minutes
  ring_alarm(alarm_index);
}

//Declared a function to ring the alarm
void ring_alarm(int alarm_index) {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 1);  //Display alarm message
  digitalWrite(LED_1, HIGH);   //Turn on the LED



  bool break_happened = false;
  bool snoozed_happened = false;

   // Loop to play the alarm sound
  // The loop continues until the cancel button is pressed or snooze button is pressed
  while (!break_happened && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {   //If cancel button is pressed, exit the loop
        delay(200);
        break_happened = true;
        break;
      }

      if (digitalRead(PB_SNOOZE) == LOW) {   //If snooze button is pressed, exit the loop
        delay(200);
        snoozed_happened = true;
        break_happened = true;
        break;
      }

      tone(BUZZER, notes[i]);   //Play the note on the buzzer
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);   //Turn off the LED
  display.clearDisplay();

  if (snoozed_happened) {
    // Add 5 minutes to the alarm time
    snooze_alarm(alarm_index);
  }
}

//Declared a function to update time by checking the alarm
void update_time_with_check_alarm(void) {
  update_time();  //Call the update time function
  print_time_now();   //Print the current time

  if (alarm_enabled == true) {    //Check if alarms are enabled
    for (int i = 0; i < n_alarms; i++) {    //Loop through all alarms
      if (!alarm_triggered[i] && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm(i);
        alarm_triggered[i] = true;
      }
    }
  }
}

//Declared a function to view the alarms
void view_alarm() {
  display.clearDisplay();  // Clear the display for fresh output
  print_line("Active Alarms:", 0, 0, 1);  // Display header

  // Loop through the alarms and display their times
  for (int i = 0; i < n_alarms; i++) {
    print_line("Alarm " + String(i + 1) + ": " + String(alarm_hours[i]) + ":" + String(alarm_minutes[i]), 0, (i + 1) * 10, 1);
  }

  print_line("Cancel to exit", 0, (n_alarms + 1) * 10, 1);  // Instructions to exit
  while (digitalRead(PB_CANCEL) == HIGH) {
    // Wait for cancel button press to exit
  }
}

void delete_alarm() {
  int alarm_to_delete = 0;  // Start with the first alarm
  while (true) {
    display.clearDisplay();  // Clear the display for fresh output
    print_line("Select Alarm to Delete:", 0, 0, 1);  // Display prompt

    // Loop through the alarms and display their times
    for (int i = 0; i < n_alarms; i++) {
      // Display all alarms, highlight the selected one
      if (i == alarm_to_delete) {
        print_line("-> Alarm " + String(i + 1) + ": " + String(alarm_hours[i]) + ":" + String(alarm_minutes[i]), 0, (i + 1) * 10, 1); // Highlight selected alarm
      } else {
        print_line("   Alarm " + String(i + 1) + ": " + String(alarm_hours[i]) + ":" + String(alarm_minutes[i]), 0, (i + 1) * 10, 1);
      }
    }

    print_line("Select (1-2) or Cancel", 0, (n_alarms + 1) * 10, 1);  // Instructions

    int pressed = wait_for_button_press();  // Wait for button press

    // Check which alarm to delete
    if (pressed == PB_UP) {
      alarm_to_delete = (alarm_to_delete + 1) % n_alarms;  // Move to next alarm
    } else if (pressed == PB_DOWN) {
      alarm_to_delete = (alarm_to_delete - 1 + n_alarms) % n_alarms;  // Move to previous alarm
    } else if (pressed == PB_OK) {
      // Check if a valid alarm is selected
      if (alarm_hours[alarm_to_delete] != 0 || alarm_minutes[alarm_to_delete] != 0) {
        // Reset the alarm time to indicate it has been deleted
        alarm_hours[alarm_to_delete] = 0;  // Set hour to 0
        alarm_minutes[alarm_to_delete] = 0;  // Set minute to 0
        display.clearDisplay();
        print_line("Deleted Alarm " + String(alarm_to_delete + 1), 0, 0, 1);  // Confirmation message
        delay(2000);  // Wait for 2 seconds to show confirmation
        break;  // Exit the delete loop
      }
    } else if (pressed == PB_CANCEL) {
      break;  // Exit the delete loop
    }
  }
}


//Declared a function to run the selected mode
void run_mode(int mode) {
  if (mode == 0) {
    set_time();
  } 
  else if (mode == 1 || mode == 2) {
    set_alarm(mode - 1);
  } 
  else if (mode == 3) {
    alarm_enabled = false;
  }  
  else if (mode == 4) {
    set_timezone();
  }
  else if (mode == 5) {  
    view_alarm();
  }
  else if (mode == 6) {  
    delete_alarm();
  } 
}


//Declared a function to go to the menu
void go_to_menu(void) {
  while (digitalRead(PB_CANCEL) == HIGH) { 
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);  // Display current mode

    int pressed = wait_for_button_press();   //// Wait for button press
    if (pressed == PB_UP) {
      delay(200);
      current_mode = (current_mode + 1) % max_modes;  // Increment mode
    } 
    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode = (current_mode - 1 + max_modes) % max_modes;    //// Decrement mode
    } 
    else if (pressed == PB_OK) {
      delay(200);
      Serial.println(current_mode);
      run_mode(current_mode);     // Run the selected mode
    } 
    else if (pressed == PB_CANCEL) {
      delay(200);  // Exit the menu loop
      break;
    }
  }
}

void setup() {
  // Initialize the hardware pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);   
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(PB_SNOOZE, INPUT);
  pinMode(LDR_PIN, INPUT);
  servo.attach(SERVO_PIN, 500, 2400);  

  // Initialize the DHT sensor
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  Serial.println("Serial initialized");

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Show Adafruit splash screen
  display.display();
  delay(1000); // Delay for 1 second

  display.clearDisplay(); // Clear the buffer
  print_line("Initializing...", 0, 0, 2);
  delay(1000);

  // Initialize the WiFi connection with proper error handling
  setupWifi();

  // Initialize random seed for generating unique client IDs
  randomSeed(micros());
  
  // Setup MQTT after WiFi is connected
  setupMqtt();

  // Setup time client after network is established
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  
  // Initialize default parameter values if they haven't been set yet
  if (Sampling_interval == 0) Sampling_interval = 5000;    // Default: 5 seconds
  if (Sending_interval == 0) Sending_interval = 120000;   // Default: 2 minutes
  if (T_med == 0) T_med = 30;                  // Default: 30°C
  if (Theta_offset == 0) Theta_offset = 30;    // Default: 30 degrees
  if (Controlling_Factor == 0) Controlling_Factor = 0.75;  // Default: 0.75
  
  display.clearDisplay();
  print_line("Welcome to Medibox!", 10, 20, 2);  // Display welcome message
  delay(2000);
  
  display.clearDisplay();
}


void loop() {
  // Check if MQTT is connected and reconnect if necessary
  unsigned long currentMillis = millis();
  static unsigned long lastReconnectAttempt = 0;
  
  if (!mqttClient.connected()) {
    // Try to reconnect every 5 seconds
    if (currentMillis - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = currentMillis;
      Serial.println("MQTT disconnected, attempting reconnection...");
      
      // Attempt to reconnect
      if (connectToMQTTBroker()) {
        lastReconnectAttempt = 0; // Reset counter if connected
      }
    }
  } else {
    // Client is connected, process messages
    mqttClient.loop();
    
    // Publish data at the specified intervals
    static unsigned long lastPublishTime = 0;
    if (currentMillis - lastPublishTime > Sending_interval) {
      lastPublishTime = currentMillis;
      
      // Update and publish temperature data
      updateTemperature();
      Serial.print("Publishing temperature: ");
      Serial.println(Tmp_Array);
      mqttClient.publish("220473X_Temperature", Tmp_Array);
      
      // Update and publish light/servo data if parameters are valid
      if (Sending_interval > 0 && T_med > 0) {
        updateLight();
        Serial.print("Publishing light intensity: ");
        Serial.println(LDR_Array);
        Serial.print("Publishing servo angle: ");
        Serial.println(Servo_Angle_Array);
      }
    }
  }

  update_time_with_check_alarm();  // Update time and check for alarms
  
  if (digitalRead(PB_OK) == LOW) {  // If OK button is pressed, go to menu
    delay(200);
    go_to_menu();
  }
  
  check_temp();  // Check temperature and humidity
  delay(100);    // Small delay to prevent excess looping
}

