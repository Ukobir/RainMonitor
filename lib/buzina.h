#include "pico/stdlib.h"  //subconjunto central de bibliotecas do SDK Pico
#include "hardware/pwm.h" //biblioteca para controlar o hardware de PWM
#include "hardware/clocks.h"

#ifndef BUZINA_H
#define BUZINA_H

#define BUZZER_A 21
#define BUZZER_B 10



// Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 5000

// Definição de uma função para  iniciar 2 PWMs para os Buzzers
void initPwm()
{
    // Configurar o pino como saída de PWM
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    // Obter o slice do PWM associado ao pino
    uint slice_num_A = pwm_gpio_to_slice_num(BUZZER_A);
    uint slice_num_B = pwm_gpio_to_slice_num(BUZZER_B);
    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(BUZZER_A, 2048);
    pwm_set_gpio_level(BUZZER_B, 2048);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock

    pwm_init(slice_num_A, &config, true);
    pwm_init(slice_num_B, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 0);
}

void beep(uint pin, uint16_t wrap)
{
    int slice = pwm_gpio_to_slice_num(pin); // Obtém o slice PWM correspondente ao pino
    pwm_set_wrap(slice, wrap);              // Define o valor de wrap para o PWM
    pwm_set_gpio_level(pin, wrap / 2);      // Ajusta o nível PWM com base no wrap
    pwm_set_enabled(slice, true);           // Habilita o PWM no slice correspondente
}

void semSom()
{
    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 0);
}

#endif