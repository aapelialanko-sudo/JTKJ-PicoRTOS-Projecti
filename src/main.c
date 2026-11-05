
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"



#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1


enum state { WAITING=1, data_ready = 2};
enum state programState = WAITING;


uint32_t ambientLight;
//muuta globaaliksi struktiksi
float ax, ay, az, gx, gy, gz;

static void btn_fxn(uint gpio, uint32_t eventMask) {
    uint8_t pinValue = gpio_get(LED1);
    pinValue = !pinValue;
    gpio_put(LED1, pinValue);
    // Tehtävä 1: Vaihda LEDin tila käyttämällä TKJHAT SDK:n sopivaa funktiota.
    // Exercise 1: Toggle the LED using the appropriate function from the TKJHAT SDK.
}

static void sensor_task(void *arg){
    (void)arg;
    // Tehtävä 2: Alusta valoisuusanturi. Etsi SDK-dokumentaatiosta sopiva funktio.
    // Exercise 2: Init the light sensor. Find in the SDK documentation the adequate function.
   init_veml6030();
    for(;;){
  
        // luetaan data ja vaihdetaan tilaa
        if (programState == WAITING){
            ambientLight = veml6030_read_light();
            programState = data_ready;

        }

        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



static void print_task(void *arg){
    (void)arg;
    
    while(1){
        
        // tight_loop_contents();
        
                if(programState == data_ready){
            printf("Kiihtyvyys: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f\n", ax, ay, az, gx, gy, gz);
            programState = WAITING;
            }
             vTaskDelay(pdMS_TO_TICKS(1000));
        

    }
}

static void imu_task(void *pvParameters) {
    (void)pvParameters;
    
    
    // Setting up the sensor. 
    if (init_ICM42670() == 0) {
        printf("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
            printf("ICM-42670P could not initialize gyroscope");
        }
    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }
    while (1)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, NULL) == 0) {
            
            
            if(programState == WAITING){
                programState = data_ready;
            }
             vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}




int main(){

    stdio_init_all();

    // Uncomment this lines if you want to wait till the serial monitor is connected
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.
    
    gpio_init(BUTTON1);
    gpio_set_dir(BUTTON1, GPIO_IN);

    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);

    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    TaskHandle_t hSensorTask, hPrintTask, hUSB = NULL;
    /*
    xTaskCreate(usbTask, "usb", 2048, NULL, 3, &hUSB);
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif
    */


    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(imu_task, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hSensorTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,  // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hPrintTask);         // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

