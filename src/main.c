#include "stm32f10x.h"
#include "i2c.h"
#include "tcs34725.h"
#include "usart.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    int r;
    int g;
    int b;
    uint16_t c;
} ColorData;

typedef struct {
    ColorData raw;
    int16_t rn;
    int16_t gn;
    int16_t bn;
    uint32_t dist;
    int8_t idx;
    uint8_t match;
    uint8_t valid;
} SensorResult;

static const int16_t allowed_norm[4][3] = {
    {744, 137, 118},
    {278, 343, 377},
    {292, 446, 261},
    {333, 333, 333}
};

#define MATCH_THRESHOLD         15000UL
#define SENSOR_LOOP_DELAY_MS    10U
#define DEBUG_PRINT_MS          150U
#define DEBUG_UART              1

#define TX_PORT                 GPIOA
#define TX_PIN_BIT0             GPIO_Pin_0
#define TX_PIN_BIT1             GPIO_Pin_1
#define TX_PIN_VALID            GPIO_Pin_2

static volatile uint32_t g_ms_ticks = 0;
static uint32_t g_last_print_ms = 0;
static SensorResult g_sensor;
static int8_t g_last_sent_idx = -2;
static uint8_t g_last_sent_valid = 2;

void SysTick_Handler(void)
{
    g_ms_ticks++;
}

static uint32_t millis(void)
{
    return g_ms_ticks;
}

static void delay_ms_tick(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms);
}

static void uart_send_len(const char *s, uint16_t len)
{
    USART_Send_bytes(s, len);
}

static void uart_printf(const char *fmt, ...)
{
#if DEBUG_UART
    char buf[96];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n > (int)sizeof(buf)) n = (int)sizeof(buf);
        uart_send_len(buf, (uint16_t)n);
    }
#else
    (void)fmt;
#endif
}

static void GPIO_Output_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = TX_PIN_BIT0 | TX_PIN_BIT1 | TX_PIN_VALID;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(TX_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(TX_PORT, TX_PIN_BIT0 | TX_PIN_BIT1 | TX_PIN_VALID);
}

static void Sender_Output_Invalid(void)
{
    GPIO_ResetBits(TX_PORT, TX_PIN_BIT0 | TX_PIN_BIT1 | TX_PIN_VALID);
}

static void Sender_Output_Color(int8_t idx)
{
    GPIO_ResetBits(TX_PORT, TX_PIN_BIT0 | TX_PIN_BIT1 | TX_PIN_VALID);

    if (idx & 0x01) GPIO_SetBits(TX_PORT, TX_PIN_BIT0);
    if (idx & 0x02) GPIO_SetBits(TX_PORT, TX_PIN_BIT1);

    GPIO_SetBits(TX_PORT, TX_PIN_VALID);
}

static void Sender_Update_Output(int8_t idx, uint8_t valid)
{
    if ((idx == g_last_sent_idx) && (valid == g_last_sent_valid)) {
        return;
    }

    g_last_sent_idx = idx;
    g_last_sent_valid = valid;

    if (valid) Sender_Output_Color(idx);
    else Sender_Output_Invalid();
}

static void Sensor_Init(void)
{
    I2C_Peripheral_Init(I2C1);
    delay_ms_tick(10);
    tcs3272_init(I2C1);
    delay_ms_tick(10);
}

static void read_sensor_raw(I2C_TypeDef *I2Cx, ColorData *d)
{
    getRGB(I2Cx, &d->r, &d->g, &d->b, &d->c);
}

static void calc_norm_rgb(const ColorData *src, int16_t *rn, int16_t *gn, int16_t *bn)
{
    int32_t sum = src->r + src->g + src->b;
    if (sum <= 0) {
        *rn = 0; *gn = 0; *bn = 0;
        return;
    }

    *rn = (int16_t)((src->r * 1000L) / sum);
    *gn = (int16_t)((src->g * 1000L) / sum);
    *bn = (int16_t)((src->b * 1000L) / sum);
}

static int8_t classify_from_norm(int16_t rn, int16_t gn, int16_t bn, uint32_t *best_dist)
{
    uint8_t i;
    int8_t best_idx = -1;
    uint32_t min_dist = 0xFFFFFFFFUL;

    for (i = 0; i < 4U; i++) {
        int32_t dr = rn - allowed_norm[i][0];
        int32_t dg = gn - allowed_norm[i][1];
        int32_t db = bn - allowed_norm[i][2];
        uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);
        if (d < min_dist) {
            min_dist = d;
            best_idx = (int8_t)i;
        }
    }

    *best_dist = min_dist;
    return best_idx;
}

static void sensor_read_and_classify(I2C_TypeDef *I2Cx, SensorResult *res)
{
    read_sensor_raw(I2Cx, &res->raw);

    if ((res->raw.r == 0) && (res->raw.g == 0) && (res->raw.b == 0) && (res->raw.c == 0)) {
        res->rn = 0;
        res->gn = 0;
        res->bn = 0;
        res->dist = 0xFFFFFFFFUL;
        res->idx = -1;
        res->match = 0;
        res->valid = 0;
        return;
    }

    calc_norm_rgb(&res->raw, &res->rn, &res->gn, &res->bn);
    res->idx = classify_from_norm(res->rn, res->gn, res->bn, &res->dist);
    res->match = ((res->idx >= 0) && (res->dist <= MATCH_THRESHOLD)) ? 1U : 0U;
    res->valid = 1;
}

static char color_char(const SensorResult *s)
{
    if (!s->valid) return 'E';
    if (!s->match) return 'N';
    switch (s->idx) {
        case 0: return 'R';
        case 1: return 'B';
        case 2: return 'G';
        case 3: return 'W';
        default: return '?';
    }
}

static void process_single_sensor_sender(void)
{
    uint32_t now = millis();

    sensor_read_and_classify(I2C1, &g_sensor);

    if (g_sensor.valid && g_sensor.match) {
        Sender_Update_Output(g_sensor.idx, 1U);
    } else {
        Sender_Update_Output(0, 0U);
    }

#if DEBUG_UART
    if ((now - g_last_print_ms) >= DEBUG_PRINT_MS) {
        g_last_print_ms = now;
        uart_printf("S:%c %d %d %d %lu\r\n",
                    color_char(&g_sensor),
                    (int)g_sensor.rn,
                    (int)g_sensor.gn,
                    (int)g_sensor.bn,
                    (unsigned long)g_sensor.dist);
    }
#endif

    delay_ms_tick(SENSOR_LOOP_DELAY_MS);
}

int main(void)
{
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U);

    Usart_Int(115200);
    uart_send_len("TX RUN\r\n", 8);

    GPIO_Output_Init();
    Sender_Output_Invalid();
    Sensor_Init();

    while (1)
    {
        process_single_sensor_sender();
    }
}
