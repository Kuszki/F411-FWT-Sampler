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

#define ARM_MATH_CM4
#include "arm_math.h"

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

volatile int doReq = 0;
volatile int isDone = 1;

const float32_t* A = get_matrix_ptr();
const size_t N = 128;

float32_t X[N];
float32_t Y[N];
uint32_t V[N];

uint8_t dummy;

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

	arm_matrix_instance_f32 mat_A;
	arm_mat_init_f32(&mat_A, N, N, (float32_t*) A);

	arm_matrix_instance_f32 mat_X;
	arm_mat_init_f32(&mat_X, N, 1, X);

	arm_matrix_instance_f32 mat_Y;
	arm_mat_init_f32(&mat_Y, N, 1, Y);

	while (1) if (doReq)
	{
		isDone = 0;

		HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_RESET);
		HAL_TIM_Base_Start_IT(&htim2);

		while (!isDone);
		isDone = 0;

//		HAL_GPIO_WritePin(DEBUG_OUT_GPIO_Port, DEBUG_OUT_Pin, GPIO_PIN_SET);

		arm_q31_to_float((q31_t*) V, X, N);
		arm_mat_scale_f32(&mat_X, 2147483648.0f, &mat_X);
		arm_mat_mult_f32(&mat_A, &mat_X, &mat_Y);

//		HAL_GPIO_WritePin(DEBUG_OUT_GPIO_Port, DEBUG_OUT_Pin, GPIO_PIN_RESET);

		HAL_UART_Transmit_DMA(&huart1, (uint8_t*) V, N*sizeof(uint32_t));

		while (!isDone);
		doReq = 0;
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	HAL_GPIO_WritePin(LED_OUT_GPIO_Port, LED_OUT_Pin, GPIO_PIN_SET);
	HAL_TIM_Base_Stop_IT(&htim2);

	isDone = 1;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	HAL_GPIO_TogglePin(LED_OUT_GPIO_Port, LED_OUT_Pin);
	HAL_GPIO_TogglePin(DEBUG_OUT_GPIO_Port, DEBUG_OUT_Pin);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_UART_Receive_IT(huart, &dummy, 1);

	if (!doReq && isDone) doReq = true;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	isDone = 1;
}
