#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "tim.h"
#include "adc.h"
#include "dma.h"
#include "crc.h"

#include "matrix.hpp"
#include "arm_math.h"

#define SAMPLES_NUM 29696

//#define DEBUG
#ifdef DEBUG
#define IF_DBG(x) x
#else
#define IF_DBG(x)
#endif
#undef DEBUG

enum STATUS
{
	WAIT_FOR_USER,
	WAIT_FOR_TRIGGER,
	WAIT_FOR_ADC,
	WAIT_FOR_SEND,
	WAIT_FOR_DELAY,
	WAIT_FOR_UART,
	WAIT_FOR_USB
};

enum METHOD
{
	GET_ADC,
	GET_DWT
};

extern "C"
{
	void SystemClock_Config(void);
}

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

extern TIM_HandleTypeDef htim2;

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

volatile STATUS status = WAIT_FOR_USER;
volatile METHOD method = GET_ADC;

const float32_t* A = get_matrix_ptr();
const size_t N = SAMPLES_NUM;

static GPIO_InitTypeDef trgConfig = { TRIGGER_IN_Pin, GPIO_MODE_IT_RISING, GPIO_NOPULL, 0, 0 };
static GPIO_InitTypeDef stbConfig = { TRIGGER_IN_Pin, GPIO_MODE_ANALOG, GPIO_NOPULL, 0, 0 };
static ADC_ChannelConfTypeDef adcConfig = { ADC_CHANNEL_4, 1, ADC_SAMPLETIME_144CYCLES, 0 };

static uint32_t newAdcCh = adcConfig.Channel;
static uint32_t newAdcSt = adcConfig.SamplingTime;

#if SAMPLES_NUM == 128
float32_t X[N];
float32_t Y[N];
#else
const unsigned toDo = (N*sizeof(uint32_t)) / 10240;
const unsigned toRs = (N*sizeof(uint32_t)) % 10240;
#endif

static uint32_t V[N];
static uint8_t dummy = 0;

int main(void)
{
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_CRC_Init();
	MX_DMA_Init();
	MX_TIM2_Init();
	MX_ADC1_Init();
	MX_USART1_UART_Init();

	HAL_GPIO_Init(TRIGGER_IN_GPIO_Port, &stbConfig);
	HAL_UART_Receive_IT(&huart1, &dummy, 1);
	HAL_ADC_Start_DMA(&hadc1, V, N);
	HAL_Delay(1000);

#if SAMPLES_NUM == 128
	arm_matrix_instance_f32 mat_A;
	arm_mat_init_f32(&mat_A, N, N, (float32_t*) A);

	arm_matrix_instance_f32 mat_X;
	arm_mat_init_f32(&mat_X, N, 1, X);

	arm_matrix_instance_f32 mat_Y;
	arm_mat_init_f32(&mat_Y, N, 1, Y);
#endif
	while (1)  if (status == WAIT_FOR_SEND)
	{
		HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_RESET);
#if SAMPLES_NUM == 128
		if (method == GET_DWT)
		{
			IF_DBG(HAL_GPIO_WritePin(DEBUG_1_OUT_GPIO_Port, DEBUG_1_OUT_Pin, GPIO_PIN_SET));

			arm_q31_to_float((q31_t*) V, X, N);
			arm_mat_scale_f32(&mat_X, 1.0f, &mat_X);
			arm_mat_mult_f32(&mat_A, &mat_X, &mat_Y);
			arm_float_to_q31(Y, (q31_t*) V, N);

			IF_DBG(HAL_GPIO_WritePin(DEBUG_1_OUT_GPIO_Port, DEBUG_1_OUT_Pin, GPIO_PIN_RESET));
		}

		HAL_UART_Transmit_DMA(&huart1, (uint8_t*) V, N*sizeof(uint32_t));

		status = WAIT_FOR_UART;
#else
		HAL_Delay(1000);

		for (unsigned i = 0; i < toDo; ++i)
			HAL_UART_Transmit(&huart1, (uint8_t*) (V + i*2560), 10240, 10000);

		if (toRs)
			HAL_UART_Transmit(&huart1, (uint8_t*) (V + toDo*2560), toRs, 10000);

		HAL_Delay(1000);

		status = WAIT_FOR_USER;
#endif
	}
	else if (status == WAIT_FOR_DELAY || status == WAIT_FOR_USB)
	{
		if (adcConfig.Channel != newAdcCh || adcConfig.SamplingTime != newAdcSt)
		{
			adcConfig.Channel = newAdcCh;
			adcConfig.SamplingTime = newAdcSt;
			HAL_ADC_ConfigChannel(&hadc1, &adcConfig);
		}

		HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_SET);
		HAL_Delay(1000);

		for (size_t i = 0; i < N; ++i) V[i] = 0;

		if (status == WAIT_FOR_DELAY) HAL_GPIO_EXTI_Callback(666);
		else
		{
			HAL_GPIO_Init(TRIGGER_IN_GPIO_Port, &trgConfig);
			HAL_Delay(1000);

			status = WAIT_FOR_TRIGGER;
		}

	}
	else if (status == WAIT_FOR_USB)
	{
		HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_SET);
		HAL_Delay(1000);

	}
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
	if (status != WAIT_FOR_TRIGGER && pin != 666) return;
	else htim2.Instance->CNT = TIMER2_CNT;

	IF_DBG(HAL_GPIO_WritePin(DEBUG_1_OUT_GPIO_Port, DEBUG_1_OUT_Pin, GPIO_PIN_SET));

	HAL_TIM_Base_Start_IT(&htim2);

	HAL_GPIO_Init(TRIGGER_IN_GPIO_Port, &stbConfig);

	status = WAIT_FOR_ADC;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	IF_DBG(HAL_GPIO_WritePin(DEBUG_1_OUT_GPIO_Port, DEBUG_1_OUT_Pin, GPIO_PIN_RESET));

	HAL_TIM_Base_Stop_IT(&htim2);

	status = WAIT_FOR_SEND;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_UART_Receive_IT(huart, &dummy, 1);

	if (status == WAIT_FOR_TRIGGER && dummy == 'q') status = WAIT_FOR_USER;
	else if (status == WAIT_FOR_USER)
	{
		switch (dummy)
		{
			case 'a':
				status = WAIT_FOR_DELAY;
				method = GET_ADC;
			break;
			case 's':
				status = WAIT_FOR_USB;
				method = GET_ADC;
			break;
			case 'z':
				status = WAIT_FOR_DELAY;
				method = GET_DWT;
			break;
			case 'x':
				status = WAIT_FOR_USB;
				method = GET_DWT;
			break;
			case '1':
				newAdcCh = ADC_CHANNEL_4;
			break;
			case '2':
				newAdcCh = ADC_CHANNEL_2;
			break;
			case '3':
				newAdcCh = ADC_CHANNEL_0;
			break;
			case '5':
				newAdcSt = ADC_SAMPLETIME_3CYCLES;
			break;
			case '6':
				newAdcSt = ADC_SAMPLETIME_15CYCLES;
			break;
			case '7':
				newAdcSt = ADC_SAMPLETIME_28CYCLES;
			break;
			case '8':
				newAdcSt = ADC_SAMPLETIME_56CYCLES;
			break;
			case '9':
				newAdcSt = ADC_SAMPLETIME_84CYCLES;
			break;
			case '0':
				newAdcSt = ADC_SAMPLETIME_112CYCLES;
			break;
			case '=':
				newAdcSt = ADC_SAMPLETIME_144CYCLES;
			break;
		}
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (status == WAIT_FOR_UART) status = WAIT_FOR_USER;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	IF_DBG(HAL_GPIO_TogglePin(DEBUG_0_OUT_GPIO_Port, DEBUG_0_OUT_Pin));
}
