
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"



#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1

uint32_t ambientLight;
//tehdään globaali struct, johon imu data tallennetaan. Näin saadaan imu dataa luettua eri tehtävissä
typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
} IMUData;

volatile IMUData g_imu_data; // pakottaa lukemaan arvot aina uudestaan, eikä käytä vanhoja arvoja
volatile bool nappi_painettu = false; // 

enum state { WAITING=1, data_ready = 2};
volatile enum state programState = WAITING; // pakottaa lukemaan arvot aina uudestaan

typedef enum {                  //lisätty illalla. Määritellään livedatan arvoista mitä merkkejä asennot antavat
    Tuntematon_asento = 0,
    ASENTO_PYSTY,
    ASENTO_VAAKA,
} Laiteasento;

#define PYSTY_KYNNYS 0.70f   //hatusta ja netistä heitetyt valmiit arvot. Näisät kattoo että millä livedata tiedolla saadaan oikea merkki printattua
#define VAAKA_KYNNYS 0.70f  //pitää katsoa vielä screenshoteista tarkennetut arvot

void tulostetaan_asento(void){
    if (g_imu_data.az > VAAKA_KYNNYS || g_imu_data.az < -VAAKA_KYNNYS) {
    // Laite on vaakatasossa
    printf(".\n");          //tässä laitetaan printtiarvot eri asennoilla
    fflush(stdout); // lähettää printin heti, eikä odota että puskuri täyttyy
} else if ( g_imu_data.ay > PYSTY_KYNNYS || g_imu_data.ay < -PYSTY_KYNNYS) {
    // Laite on pystysuorassa
    printf("-\n");
    fflush(stdout); 
} else {
    // Laite on tuntemattomassa asennossa
    printf(".\n");
}
}


static void btn_fxn(uint gpio, uint32_t eventMask) {
    uint8_t pinValue = gpio_get(LED1);
    pinValue = !pinValue;
    gpio_put(LED1, pinValue);

   nappi_painettu = true;
}

static void sensor_task(void *arg){
    (void)arg;

   init_veml6030();
    for(;;){
  
        // luetaan data ja vaihdetaan tilaa
        if (programState == WAITING){
            ambientLight = veml6030_read_light();
            programState = data_ready;

        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



static void print_task(void *arg){
    (void)arg;
    
    while(1){
        
        // tight_loop_contents();
        
                if(programState == data_ready){
            //printf("Kiihtyvyys: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f\n",g_imu_data.ax , g_imu_data.ay, g_imu_data.az,g_imu_data.gx, g_imu_data.gy, g_imu_data.gz);
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
        if (ICM42670_read_sensor_data((float *)&g_imu_data.ax, (float *)&g_imu_data.ay, (float *)&g_imu_data.az,
                                     (float *)&g_imu_data.gx, (float *)&g_imu_data.gy, (float *)&g_imu_data.gz, NULL) == 0) {
            
            if (nappi_painettu){
                nappi_painettu = false;          //tulostukset napin painelun mukaan. Jos nappi on ylhäällä, niin ei tulosteta asentoja
                tulostetaan_asento();
            }
            
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

