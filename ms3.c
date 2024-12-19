#include <hardware/gpio.h>
#include "pico/stdlib.h"
#include "stdio.h"

// Pin definitions - Fixed pin conflicts
#define IN1 4        // H-bridge input 1
#define IN2 5        // H-bridge input 2
#define IN3 21       // H-bridge input 3
#define IN4 7        // H-bridge input 4
#define BUZZER 26    // Changed buzzer pin to avoid conflict
#define IR_ECHO 27   // IR sensor echo for distance
#define IR_TRIG 28   // IR sensor trigger for distance
#define POT_PIN 20   // Speed control pin
#define IR_SWITCH 6// New IR sensor pin for on/off functionality
#define LEFT_ENABLE 17
#define RIGHT_ENABLE 18
// Constants
#define SAFE_DISTANCE 15 // Distance in centimeters

// Global variables
uint64_t duration;
float distance;
bool isMoving = true;
bool isEnabled = true;  // New variable to track system state

// Function declarations
void measureDistance(void);
void moveForward(void);
void stopMotors(void);
void emergencyStop(void);
void resumeMovement(void);
void checkIRSwitch(void);  // New function to check IR switch status

int main() {
    // Initialize stdio
    stdio_init_all();
    sleep_ms(1000);  // Give system time to stabilize
    
    printf("Initializing system...\n");
    
    // Initialize GPIO pins
    gpio_init(IN1);
    gpio_init(IN2);
    gpio_init(IN3);
    gpio_init(IN4);
    gpio_init(BUZZER);
    gpio_init(IR_TRIG);
    gpio_init(IR_ECHO);
    gpio_init(POT_PIN);
    gpio_init(IR_SWITCH);  // Initialize new IR switch pin
    gpio_init(LEFT_ENABLE);
    gpio_init(RIGHT_ENABLE);
    
    // Set GPIO directions
    gpio_set_dir(IN1, GPIO_OUT);
    gpio_set_dir(IN2, GPIO_OUT);
    gpio_set_dir(IN3, GPIO_OUT);
    gpio_set_dir(IN4, GPIO_OUT);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_set_dir(IR_TRIG, GPIO_OUT);
    gpio_set_dir(IR_ECHO, GPIO_IN);
    gpio_set_dir(POT_PIN, GPIO_IN);
    gpio_set_dir(IR_SWITCH, GPIO_IN);  // Set IR switch as input
     gpio_set_dir(LEFT_ENABLE, GPIO_OUT); 
      gpio_set_dir(RIGHT_ENABLE, GPIO_OUT);  
    
    // Set initial states
    gpio_put(IN1, 0);
    gpio_put(IN2, 0);
    gpio_put(IN3, 0);
    gpio_put(IN4, 0);
    gpio_put(BUZZER, 0);
    gpio_put(IR_TRIG, 0);
    gpio_put(LEFT_ENABLE, 16);
    gpio_put(RIGHT_ENABLE, 16);
    

    
    printf("System initialized. Starting main loop...\n");
    
    while (1) {
        // Check IR switch status first
        checkIRSwitch();
        
        // Only proceed with normal operation if system is enabled
        if (isEnabled) {
            // Print debug start
            printf("Starting distance measurement...\n");
            
            // Read distance from IR sensor
            measureDistance();
            
            printf("Distance measured: %.2f cm\n", distance);
            
            // Check if obstacle is too close
            if (distance < SAFE_DISTANCE && distance > 0) {
                printf("Obstacle detected! Emergency stop!\n");
                emergencyStop();
            } else {
                if (!isMoving) {
                    printf("Path clear, resuming movement\n");
                    resumeMovement();
                }
                moveForward();
            }
        } else {
            // System is disabled - ensure motors are stopped
            stopMotors();
            gpio_put(BUZZER, 0);  // Ensure buzzer is off
        }
        
        sleep_ms(100);  // Small delay
    }
    
    return 0;
}

void checkIRSwitch(void) {
    static bool lastState = true;  // Remember last state for edge detection
    static uint32_t lastChangeTime = 0;  // For debouncing
    
    // Read current state of IR switch
    bool currentState = gpio_get(IR_SWITCH);
    uint32_t currentTime = to_ms_since_boot(get_absolute_time());
    
    // Simple debouncing - ignore changes within 500ms
    if (currentTime - lastChangeTime > 500) {
        if (currentState != lastState) {
            // State has changed
            if (currentState) {  // IR beam broken
                isEnabled = !isEnabled;  // Toggle system state
                printf("System state changed to: %s\n", isEnabled ? "ENABLED" : "DISABLED");
                lastChangeTime = currentTime;
            }
            lastState = currentState;
        }
    }
}

// Rest of the functions remain the same
void measureDistance(void) {
    // Clear the trigger pin
    gpio_put(IR_TRIG, 0);
    sleep_us(5);
    
    // Send 10μs pulse to trigger pin
    gpio_put(IR_TRIG, 1);
    sleep_us(10);
    gpio_put(IR_TRIG, 0);
    
    // Wait a moment for sensor to settle
    sleep_us(5);
    
    // Measure the response
    uint64_t start_time = time_us_64();
    uint64_t end_time;
    
    // Wait for echo to go high
    while (!gpio_get(IR_ECHO)) {
        if (time_us_64() - start_time > 35000) {
            printf("Timeout waiting for echo start\n");
            distance = 999;
            return;
        }
    }
    
    start_time = time_us_64();
    
    // Wait for echo to go low
    while (gpio_get(IR_ECHO)) {
        if (time_us_64() - start_time > 35000) {
            printf("Timeout waiting for echo end\n");
            distance = 999;
            return;
        }
    }
    
    end_time = time_us_64();
    duration = end_time - start_time;
    
    // Calculate distance in centimeters
    distance = (duration * 0.0343) / 2;
    
    printf("Duration: %llu us, Distance: %.2f cm\n", duration, distance);
}

void moveForward(void) {
    gpio_put(IN1, 1);  // Motor 1 forward
    gpio_put(IN2, 0);
    gpio_put(IN3, 1);  // Motor 2 forward
    gpio_put(IN4, 0);
    isMoving = true;
    printf("Motors moving forward\n");
}

void stopMotors(void) {
    gpio_put(IN1, 0);
    gpio_put(IN2, 0);
    gpio_put(IN3, 0);
    gpio_put(IN4, 0);
    isMoving = false;
    printf("Motors stopped\n");
}

void emergencyStop(void) {
    stopMotors();
    gpio_put(BUZZER, 1);  // Turn on buzzer
    printf("Emergency stop activated - Buzzer ON\n");
}

void resumeMovement(void) {
    gpio_put(BUZZER, 0);  // Turn off buzzer
    isMoving = true;
    printf("Resuming movement - Buzzer OFF\n");
}