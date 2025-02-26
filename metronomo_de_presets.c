#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "ws2818b.pio.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

// Definições dos pinos GPIO e de valores contantes úteis ao programa
#define LED_RED 13
#define LED_BLUE 12
#define LED_MATRIX 7
#define LED_COUNT 25
#define BUTTON_A 5
#define BUTTON_B 6
#define JOYSTICK_BUTTON 22
#define JOYSTICK_Y 26
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C
#define BUZZER_A 21
#define BUZZER_B 10

// Definições de variáveis e estruturas úteis ao programa
ssd1306_t ssd;
int bpm_values[20];
int beats_values[20];
int bpm_count = 0;
volatile int current_bpm_index = 0; 
volatile bool paused = false; 
bool metronome_running = false;
struct pixel_t { uint32_t G, R, B; }; // Estrutura para armazenar a cor de um pixel (LED da matriz WS2812)
typedef struct pixel_t pixel_t;
pixel_t leds[LED_COUNT];
PIO np_pio;
uint sm;

// Inicializa o controlador PIO para os LEDs WS2812
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
}

// Define a cor de um LED específico na matriz
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Apaga todos os LEDs da matriz
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) npSetLED(i, 0, 0, 0);
}

// Atualiza a matriz de LEDs enviando os dados ao hardware
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G << 24);
        pio_sm_put_blocking(np_pio, sm, leds[i].R << 24);
        pio_sm_put_blocking(np_pio, sm, leds[i].B << 24);
    }
}

// Função que mapeia os LEDs em zigue-zague, de acordo com a BitDogLab
int zigzag_map(int row, int col) {
    if (row % 2 == 0) {
        return (row * 5) + col;  // Linhas pares (esquerda para direita)
    } else {
        return (row * 5) + (4 - col);  // Linhas ímpares (direita para esquerda)
    }
}

// Exibe um número de 0 a 9 na matriz de LEDs
void exibir_numero(int num) {
    npClear();

    const uint8_t numeros[10][5][5] = {  
        { // Número 0
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        },
        { // Número 1
            {0,0,1,0,0},
            {0,1,1,0,0},
            {1,0,1,0,0},
            {0,0,1,0,0},
            {1,1,1,1,1}
        },
        { // Número 2
            {1,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,1,1,1,1}
        },
        { // Número 3
            {1,1,1,1,1},
            {0,0,0,0,1},
            {0,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 4
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {0,0,0,0,1},
            {0,0,0,0,1}
        },
        { // Número 5
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 6
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 7
            {1,1,1,1,1},
            {0,0,0,0,1},
            {0,0,1,1,0},
            {0,0,1,0,0},
            {0,0,1,0,0}
        },
        { // Número 8
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 9
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1}
        }
    };

    for (int row = 4; row >= 0; row--) {  
        for (int col = 4; col >= 0; col--) {  
            int led_index = zigzag_map(4 - row, 4 - col);  
            if (numeros[num][row][col]) {
                npSetLED(led_index, 255, 0, 0);  
            }
        }
    }

    npWrite();
}

// Função utilizada para enviar dados ao display e atualizá-lo
void atualizar_display(const char *mensagem) {
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, mensagem, 0, 0);
    ssd1306_send_data(&ssd);
}

// Função para configuração dos Presets
void config_preset() {
    int contador = 1;

    // Loop para definir o total de Presets
    while (true) {
        adc_select_input(0);
        uint16_t eixo_y = adc_read();
        if (eixo_y > 3000 && contador < 20) contador++;
        if (eixo_y < 1000 && contador > 1) contador--;
        if (gpio_get(BUTTON_B) == 0) break;
        char buffer[20];
        sprintf(buffer, "Total de\nPresets:\n  >%d", contador);
        atualizar_display(buffer);
        sleep_ms(80);
    }
    bpm_count = contador;
    
    // Loop para definir os BPM e Beats de cada Preset
    for (int i = 1; i <= bpm_count; i++) {        
        int bpm = 60;
        int beats = 4;
        while (true) {
            char buffer[30];
            if (i <= 9){
                sprintf(buffer, " BPM do\n\nPreset %d\n  >%d", i, bpm);
            }
            else{
                sprintf(buffer, " BPM do\n\nPreset%d\n  >%d", i, bpm);
            }
            atualizar_display(buffer);
            sleep_ms(80);
        
            adc_select_input(0);
            uint16_t eixo_y = adc_read();
            if (eixo_y > 3000 && bpm < 300) bpm++;
            else if (eixo_y < 1000 && bpm > 1) bpm--;
            
            if (gpio_get(BUTTON_B) == 0) {
                while (true) {
                    char buffer[30];
                    if (i <= 9){
                        sprintf(buffer, "Beats do\nPreset %d\n   >%d", i, beats);
                    }
                    else{
                        sprintf(buffer, "Beats do\nPreset%d\n   >%d", i, beats);
                    }
                    atualizar_display(buffer);
                    sleep_ms(80);
                
                    adc_select_input(0);
                    uint16_t eixo_y = adc_read();
                    if (eixo_y > 3000 && beats < 9) beats++;
                    else if (eixo_y < 1000 && beats > 2) beats--;

                    if (gpio_get(BUTTON_B) == 0) break;
                }
                break;
            }
        }
        bpm_values[i-1] = bpm;
        beats_values[i-1] = beats;
    }
}

// Função de debounce para os botões
bool debounce(uint gpio) {
    static uint32_t ultimo_tempo_A = 0; 
    static uint32_t ultimo_tempo_B = 0; 
    static uint32_t ultimo_tempo_JOYSTICK_BUTTON = 0;
    
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time()); 

    if (gpio == BUTTON_A) {
        if (tempo_atual - ultimo_tempo_A > 200) { 
            ultimo_tempo_A = tempo_atual; 
            return true;
        }
    } else if (gpio == JOYSTICK_BUTTON) {
        if (tempo_atual - ultimo_tempo_JOYSTICK_BUTTON > 200) {
            ultimo_tempo_JOYSTICK_BUTTON = tempo_atual;
            return true;
        }
    } else if (gpio == BUTTON_B) {
        if (tempo_atual - ultimo_tempo_B > 200) {
            ultimo_tempo_B = tempo_atual;
            return true;
        }
    }
    return false;
}

// Função de interrupção chamada ao apertar os botões
void isr_botoes(uint gpio, uint32_t events) {
    if (debounce(gpio)) {
        if (gpio == BUTTON_A && metronome_running == true) {
            paused = !paused;
        } else if (gpio == JOYSTICK_BUTTON && metronome_running == true) {
            current_bpm_index = (current_bpm_index + 1) % bpm_count;
        }
    }
}

// Função que faz o buzzer parar de emitir som
int64_t stop_beep_callback(alarm_id_t id, void *user_data) {
    uint gpio = (uintptr_t)user_data;
    pwm_set_gpio_level(gpio, 0); // Desliga o som
    gpio_set_function(gpio, GPIO_FUNC_SIO); // Retorna o GPIO ao estado normal
    gpio_put(gpio, 0);
    return 0; // Não reagendar o alarme
}

// Função que faz o buzzer emitir som em uma determinada frequência por uma determinada duração
void beep_async(uint gpio, int frequency, int duration_ms) {
    // Configurar PWM no GPIO do buzzer
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    
    // Configurar frequência do PWM
    uint32_t clock = 125000000; // Clock padrão do RP2040
    uint32_t divider16 = clock / frequency / 4096 + (clock % (frequency * 4096) != 0);
    if (divider16 / 16 == 0) divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / frequency - 1;
    
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&config, PWM_DIV_FREE_RUNNING);
    pwm_config_set_clkdiv(&config, divider16 / 16.0f);
    pwm_config_set_wrap(&config, wrap);
    
    pwm_init(slice_num, &config, true);
    
    // 50% duty cycle para gerar o som
    pwm_set_gpio_level(gpio, wrap / 2);
    
    // Configurar o timer para desligar o beep após a duração
    add_alarm_in_ms(duration_ms, stop_beep_callback, (void*)(uintptr_t)gpio, false);
}

// Configuração inicial do sistema
void setup() {
    stdio_init_all();
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, 128, 64, false, OLED_ADDR, I2C_PORT);
    ssd1306_config(&ssd); 

    adc_init();
    adc_gpio_init(JOYSTICK_Y);
    
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    
    gpio_init(JOYSTICK_BUTTON);
    gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON);
    
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &isr_botoes);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &isr_botoes);
    
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);

    npInit(LED_MATRIX);
    npClear();
}

int main() {
    setup();
    config_preset();
    metronome_running = true;

    // Loop principal de execução do metrônomo depois que os Presets foram configurados
    while (metronome_running) {
        int bpm = bpm_values[current_bpm_index];
        int led_flag = current_bpm_index;
        int intervalo = 60000 / bpm; 
        int duracao = intervalo / 2;
        int contagem = beats_values[current_bpm_index];
        gpio_put(LED_RED, 0);
    
        absolute_time_t proximo_batimento = get_absolute_time();
    
        for (int i = 1; i <= contagem; i++) {
            if (paused) continue;
        
            char buffer[20];
            if ((current_bpm_index + 1) <= 9){
                if (bpm_values[current_bpm_index] <= 99){
                    sprintf(buffer, "Preset %d\nBPM: %d\n\nBeats:%d", current_bpm_index + 1, bpm_values[current_bpm_index], beats_values[current_bpm_index]);
                }
                else {
                    sprintf(buffer, "Preset %d\nBPM:%d\n\nBeats:%d", current_bpm_index + 1, bpm_values[current_bpm_index], beats_values[current_bpm_index]);
                }
            } 
            else{
                if (bpm_values[current_bpm_index] <= 99){
                    sprintf(buffer, "Preset%d\nBPM: %d\n\nBeats:%d", current_bpm_index + 1, bpm_values[current_bpm_index], beats_values[current_bpm_index]);
                }
                else{
                    sprintf(buffer, "Preset%d\nBPM:%d\n\nBeats:%d", current_bpm_index + 1, bpm_values[current_bpm_index], beats_values[current_bpm_index]);
                }
            }

            atualizar_display(buffer);
        
            exibir_numero(i);
        
            if (i == 1) {
                beep_async(BUZZER_A, 800, duracao);
            } else {
                beep_async(BUZZER_B, 600, duracao);
            }
        
            // Pisca o LED azul no contratempo (metade do intervalo)
            absolute_time_t meio_tempo = delayed_by_ms(get_absolute_time(), intervalo / 2);
            
            // Aguarda o contratempo para piscar o LED azul
            if (led_flag == current_bpm_index){
                sleep_until(meio_tempo);
                gpio_put(LED_BLUE, 1);
                sleep_ms(duracao / 2);  
                gpio_put(LED_BLUE, 0);
            }
            else{ // Liga LED vermelho para indicar que o preset vai mudar 
                gpio_put(LED_RED, 1);
            }
        
            proximo_batimento = delayed_by_ms(proximo_batimento, intervalo);
            sleep_until(proximo_batimento);
        }
    }
    return 0;
}