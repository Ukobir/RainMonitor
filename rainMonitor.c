/*
 *  Por: Leonardo Romão
 *  Data: 17/05/2025
 *
 *  Monitor de Chuvas e cheias
 *
 *  Descrição: Utiliza o valor do joystick para simular sensores que medem o
 * nível d'água e o volume de chuva, entre valores de 0 a 100%. Depois mostra
 * o resultado no display oled e com alertas visuais com os leds e o sonoro
 * com os buzzers
 *
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "lib/ws2812.h"
#include "lib/buzina.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27
#define LED_BLUE 12
#define LED_GREEN 11
#define LED_RED 13
#define BUZZER_A 21
#define BUZZER_B 10

ssd1306_t ssd;

// Variáveis para tempo
static volatile uint32_t passado = 0;
uint32_t tempo_atual;

typedef struct
{
    uint8_t x_100;
    uint8_t y_100;
} final_value;

QueueHandle_t xQueueFinalValue;

void vJoystickTask(void *params)
{
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_init();

    uint16_t y;
    uint16_t x;

    final_value perCent;

    while (true)
    {
        adc_select_input(0); // GPIO 26 = ADC0
        y = adc_read();

        adc_select_input(1); // GPIO 27 = ADC1
        x = adc_read();

        // Calculo de conversão valor do joystick para %

        if (x > 2047)
        {
            x = 100 * (x) / (1 << 12); // Nivel d'agua
            perCent.x_100 = 2 * (x - 50);
        }
        else
        {
            x = 100 * (x) / (1 << 12); // Nivel d'agua
            perCent.x_100 = 2 * (50 - x);
        }

        if (y > 2047)
        {
            y = 100 * (y) / (1 << 12); // Volume de Chuva
            perCent.y_100 = 2 * (y - 50);
        }
        else
        {
            y = 100 * (y) / (1 << 12); // Volume de Chuva
            perCent.y_100 = 2 * (50 - y);
        }

        xQueueSend(xQueueFinalValue, &perCent, 0); // Envia o valor do joystick convertido para a fila

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz de leitura
    }
}

void vDisplayTask(void *params)
{
    initDisplay(&ssd);

    uint8_t x;
    uint8_t y;
    char x_value[5];
    char y_value[5];
    final_value perCent;
    while (true)
    {
        // Aqui os valores já convertidos são empregados para a construção da tela
        if (xQueueReceive(xQueueFinalValue, &perCent, portMAX_DELAY) == pdTRUE)
        {
            sprintf(x_value, "%u%%", perCent.x_100);
            sprintf(y_value, "%u%%", perCent.y_100);
            barras(&ssd);

            ssd1306_draw_string(&ssd, "Nivel d'Agua", 14, 3);
            ssd1306_draw_string(&ssd, "Volume de chuva", 5, 33);
            ssd1306_draw_string(&ssd, x_value, 55, 24);
            ssd1306_draw_string(&ssd, y_value, 55, 53);
            ssd1306_rect(&ssd, 15, 13, perCent.x_100, 4, true, true);
            ssd1306_rect(&ssd, 44, 13, perCent.y_100, 4, true, true);
            ssd1306_send_data(&ssd);
            ssd1306_fill(&ssd, false);
            if ((perCent.x_100 >= 70 || perCent.y_100 >= 80) && tempo_atual - passado > 1e3)
            {
                ssd1306_fill(&ssd, false);
                anima(&ssd);
                passado = tempo_atual;
                vTaskDelay(pdMS_TO_TICKS(500)); // Atualiza a cada 1500ms
            }
            tempo_atual = to_ms_since_boot(get_absolute_time());
        }
    }
}

void vPiscaRed(void *params)
{
    initPwm();
    final_value perCent;
    while (true)
    {
        if (xQueueReceive(xQueueFinalValue, &perCent, portMAX_DELAY) == pdTRUE)
        {
            if (perCent.y_100 >= 80 && perCent.x_100 >= 70) // Pisca o led vermelho a cada 0.5 segundo
            {
                beep(BUZZER_A, 65e3);
                beep(BUZZER_B, 65e3);
                gpio_put(LED_RED, 1);
                vTaskDelay(pdMS_TO_TICKS(250));
                semSom();
                gpio_put(LED_RED, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            else if (perCent.y_100 >= 80) // Pisca o led vermelho a cada 2 segundos
            {
                beep(BUZZER_A, 30e3);
                beep(BUZZER_B, 30e3);
                gpio_put(LED_RED, 1);
                vTaskDelay(pdMS_TO_TICKS(1e3));
                gpio_put(LED_RED, 0);
                semSom();
                vTaskDelay(pdMS_TO_TICKS(1e3));
            }
            else if (perCent.x_100 >= 70) // Pisca o led vermelho a cada 1 segundo
            {
                beep(BUZZER_A, 20e3);
                beep(BUZZER_B, 20e3);
                gpio_put(LED_RED, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                semSom();
                gpio_put(LED_RED, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }
}

void vMatrixAlert()
{

    npInit();  // Inicia o Neopixel LED
    npClear(); // Limpa a memória NP LED
    int i = 0;
    final_value perCent;
    while (true)
    {
        if (xQueueReceive(xQueueFinalValue, &perCent, portMAX_DELAY) == pdTRUE)
        {
            if (perCent.x_100 >= 70 || perCent.y_100 >= 80)
            {
                if (i == 12)
                {
                    i = 0;
                }

                convert(alerta[i % 12]);
                i++;
            }
            else
            {
                // Para desligar a matriz de leds
                npClear();
                npWrite();
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

// Modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Ativa BOOTSEL via botão
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_put(LED_RED, 0);

    gpio_init(BUZZER_A);
    gpio_set_dir(BUZZER_A, GPIO_OUT);

    stdio_init_all();

    // Cria a fila para compartilhamento de valor do joystick convertido
    xQueueFinalValue = xQueueCreate(6, sizeof(final_value));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display Task", 512, NULL, 1, NULL);
    xTaskCreate(vPiscaRed, "LED Red & Buzzer Alert Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrixAlert, "LED Matrix Alert", 256, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}
