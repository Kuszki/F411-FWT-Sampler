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

#ifdef DEBUG
#define IF_DBG(x) x
#else
#define IF_DBG(x)
#endif

enum STATUS
{
	WAIT_FOR_USER,
	WAIT_FOR_TRIGGER,
	WAIT_FOR_ADC,
	WAIT_FOR_SEND,
	WAIT_FOR_DELAY,
	WAIT_FOR_UART
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

#if SAMPLES_NUM == 128
float32_t X[N];
float32_t Y[N];
#else
const unsigned toDo = (N*sizeof(uint32_t)) / 10240;
const unsigned toRs = (N*sizeof(uint32_t)) % 10240;
#endif

uint32_t V[N];
uint8_t dummy = 0;

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

	HAL_UART_Receive_IT(&huart1, &dummy, 1);
	HAL_ADC_Start_DMA(&hadc1, V, N);
#if SAMPLES_NUM == 128
	arm_matrix_instance_f32 mat_A;
	arm_mat_init_f32(&mat_A, N, N, (float32_t*) A);

	arm_matrix_instance_f32 mat_X;
	arm_mat_init_f32(&mat_X, N, 1, X);

	arm_matrix_instance_f32 mat_Y;
	arm_mat_init_f32(&mat_Y, N, 1, Y);
#endif
	while (1) if (status == WAIT_FOR_SEND)
	{
		HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_RESET);
#if SAMPLES_NUM == 128
		if (method == GET_DWT)
		{
			IF_DBG(HAL_GPIO_WritePin(DEBUG_3_OUT_GPIO_Port, DEBUG_3_OUT_Pin, GPIO_PIN_SET));

			arm_q31_to_float((q31_t*) V, X, N);
			arm_mat_scale_f32(&mat_X, 2147483648.0f, &mat_X);
			arm_mat_mult_f32(&mat_A, &mat_X, &mat_Y);
			arm_float_to_q31(X, (q31_t*) V, N);

			IF_DBG(HAL_GPIO_WritePin(DEBUG_3_OUT_GPIO_Port, DEBUG_3_OUT_Pin, GPIO_PIN_RESET));
		}

		HAL_UART_Transmit_DMA(&huart1, (uint8_t*) V, N*sizeof(uint32_t));

		status = WAIT_FOR_UART;
#else
		for (unsigned i = 0; i < toDo; ++i)
			HAL_UART_Transmit(&huart1, (uint8_t*) (V + i*2560), 10240, 10000);

		if (toRs)
			HAL_UART_Transmit(&huart1, (uint8_t*) (V + toDo*2560), toRs, 10000);

		status = WAIT_FOR_USER;
#endif
	}
	else if (status == WAIT_FOR_DELAY)
	{
		HAL_Delay(1000);
		HAL_GPIO_EXTI_Callback(666);
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
	if (status == WAIT_FOR_TRIGGER || pin == 666)
	{
		IF_DBG(HAL_GPIO_WritePin(DEBUG_0_OUT_GPIO_Port, DEBUG_0_OUT_Pin, GPIO_PIN_SET));

		htim2.Instance->DIER |= (0x1UL << (0U)); // Enable int
		htim2.Instance->CR1 |= (0x1UL << (0U)); // Enable timer
		htim2.Instance->EGR |= (0x1UL << (0U)); // Force update

		htim2.State = HAL_TIM_STATE_BUSY; // Mark HAL status

		status = WAIT_FOR_ADC;
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	IF_DBG(HAL_GPIO_WritePin(DEBUG_0_OUT_GPIO_Port, DEBUG_0_OUT_Pin, GPIO_PIN_RESET));

	HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_SET);

	HAL_TIM_Base_Stop_IT(&htim2);

	status = WAIT_FOR_SEND;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	IF_DBG(HAL_GPIO_TogglePin(DEBUG_1_OUT_GPIO_Port, DEBUG_1_OUT_Pin));
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_UART_Receive_IT(huart, &dummy, 1);

	if (status == WAIT_FOR_USER || status == WAIT_FOR_TRIGGER)
	{
		switch (dummy)
		{
			case 'a':
				status = WAIT_FOR_DELAY;
				method = GET_ADC;
			break;
			case 's':
				status = WAIT_FOR_TRIGGER;
				method = GET_ADC;
			break;
			case 'z':
				status = WAIT_FOR_DELAY;
				method = GET_DWT;
			break;
			case 'x':
				status = WAIT_FOR_TRIGGER;
				method = GET_DWT;
			break;
		}
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (status == WAIT_FOR_UART) status = WAIT_FOR_USER;
}
