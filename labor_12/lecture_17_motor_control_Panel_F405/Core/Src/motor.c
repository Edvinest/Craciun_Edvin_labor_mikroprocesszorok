/*
 * motor.c
 *
 *  Created on: 2019. nov. 22.
 *      Author: girgaszpeter
 *
 *  [panel] A Kristalytiszta panelre (STM32F405, BD6211F) atirva, 2026.11.
 */

#include "motor.h"
#include "tim.h"
#include "dac.h"


int32_t set_speed_percent;
uint16_t Motor_PWMDuty;
uint8_t encoder_state;
int32_t encoder_position_act;
int32_t encoder_position_prev;
int32_t enc_error;
float encoder_speed_rpm;
float output_speed_rpm;

Motor_Mode_t motor_mode;		/* [panel] */
uint8_t switch_state;			/* [panel] */
static uint8_t mode_set = 0;	/* [panel] be van-e mar allitva valamilyen uzemmod */

/*
 * Initial values for the global variables
 * of the motor driver library.
 */
void Motor_init(void)
{
	enc_error = 0;
	Motor_PWMDuty = 0;
	encoder_state = 0;
	encoder_position_act = 0;
	encoder_position_prev = 0;
	set_speed_percent = 0;

	/* [panel] A PWM egyszer indul el, utana csak a kitoltest (CCR) irjuk at */
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

	/* [panel] DAC (PA4) -> muveleti erosito (x1,5) -> VREF_M -> jumper -> BD6211F VREF */
	HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
	HAL_DAC_Start(&hdac, DAC_CHANNEL_1);

	/* [panel] A Nucleos valtozat csak engedelyezte a TIM10 megszakitasat, de a
	 * szamlalot nem inditotta el, ezert ott az RPM mindig 0 maradt. Itt elindul. */
	HAL_TIM_Base_Start_IT(&htim10);

	Motor_setOpModeFromSwitches();
}

void Motor_setPinGPIO(uint16_t pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	HAL_GPIO_WritePin(MOTOR_PORT, pin, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(MOTOR_PORT, &GPIO_InitStruct);
}

void Motor_setPinPWM(uint16_t pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;		/* PA5 = TIM2_CH1 */
	HAL_GPIO_Init(MOTOR_PORT, &GPIO_InitStruct);
}

void Motor_setPWMDuty(uint16_t value)
{
	/* [panel] A PWM tartomanya 0 ... htim2.Init.Period (a TIM2 periodusa, nem fix 65535),
	 * ezert a Period-ot a tim.c-ben barmikor at lehet irni (pl. 4199 = 20 kHz). */
	if (value > htim2.Init.Period)
	{
		value = (uint16_t)htim2.Init.Period;
	}
	Motor_PWMDuty = value;
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, value);
}

void Motor_setPWMDutyPercent(float value)
{
	/*
	 * Calculating the new value for the Capture-compare
	 * register: 0 for 0%, htim2.Init.Period for 100% duty cycle.
	 * [panel] Csak elore: negativ ertekre 0 % (a RIN-t nem vezereljuk).
	 */
	if (value < 0)
	{
		value = 0;
	}
	if (value > 100)
	{
		value = 100;
	}
	Motor_setPWMDuty((uint16_t)(htim2.Init.Period * (value / 100)));
}

/* [panel] A kapcsolok beolvasasa: SW1 = 0. bit ... SW4 = 3. bit */
static uint8_t Motor_readSwitches(void)
{
	uint8_t sw = 0;
	if (HAL_GPIO_ReadPin(HMI_SW_1_GPIO_Port, HMI_SW_1_Pin) == HMI_SW_BE) sw |= 1;
	if (HAL_GPIO_ReadPin(HMI_SW_2_GPIO_Port, HMI_SW_2_Pin) == HMI_SW_BE) sw |= 2;
	if (HAL_GPIO_ReadPin(HMI_SW_3_GPIO_Port, HMI_SW_3_Pin) == HMI_SW_BE) sw |= 4;
	if (HAL_GPIO_ReadPin(HMI_SW_4_GPIO_Port, HMI_SW_4_Pin) == HMI_SW_BE) sw |= 8;
	return sw;
}

void Motor_setOpModeFromSwitches(void)
{
	Motor_Mode_t new_mode;

	switch_state = Motor_readSwitches();

	/* A tankonyv 4. tablazata. Az SW2 (c mod, hatra) a RIN-t kellene, ezert itt a. */
	switch (switch_state)
	{
		case 1:  new_mode = MOTOR_MODE_B; break;	/* csak SW1 */
		case 4:  new_mode = MOTOR_MODE_E; break;	/* csak SW3 */
		case 8:  new_mode = MOTOR_MODE_I; break;	/* csak SW4 */
		default: new_mode = MOTOR_MODE_A; break;
	}

	if (mode_set && new_mode == motor_mode)
	{
		return;		/* nem valtozott, nincs teendo */
	}

	switch (new_mode)
	{
		case MOTOR_MODE_B:		/* FIN = H, VREF = VCC */
			Motor_setPinGPIO(MOTOR_FIN_PIN);
			HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_FIN_PIN, GPIO_PIN_SET);
			HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 4095);
			break;
		case MOTOR_MODE_E:		/* FIN = PWM, VREF = VCC */
			Motor_setPWMDuty(0);
			Motor_setPinPWM(MOTOR_FIN_PIN);
			HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 4095);
			break;
		case MOTOR_MODE_I:		/* FIN = H, VREF = DAC (a sebesseg allitja) */
			Motor_setPinGPIO(MOTOR_FIN_PIN);
			HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_FIN_PIN, GPIO_PIN_SET);
			HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
			break;
		default:				/* a mod: FIN = L, VREF = 0 */
			Motor_setPinGPIO(MOTOR_FIN_PIN);
			HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_FIN_PIN, GPIO_PIN_RESET);
			HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0);
			break;
	}
	motor_mode = new_mode;
	mode_set = 1;
}

void Motor_setSpeedPercent(int32_t percent)
{
	if (percent < 0)   percent = 0;
	if (percent > 100) percent = 100;

	if (motor_mode == MOTOR_MODE_E)
	{
		Motor_setPWMDutyPercent((float)percent);
	}
	else if (motor_mode == MOTOR_MODE_I)
	{
		HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (uint32_t)(4095 * percent / 100));
	}
	/* a es b modban a sebesseg nem allithato */
}

void updateEncoderPosition(uint8_t phaseA, uint8_t phaseB) {
	/*
	 * Copy the latest encoder state to a temporary
	 * variable, keep only the two last bits.
	 */
	uint8_t s = encoder_state & 3;

	/*
	 * Put the new values of the encoder phases to
	 * the temporary variable
	 */
	if (phaseA) s |= 4;
	if (phaseB) s |= 8;

	/*
	 * The s variable holds the latest and the new
	 * state of the encoder channels state.
	 *
	 * Case New B  New A Last B Last A
	 * 0:       0      0      0      0
	 * 1:       0      0      0      1
	 * 2:       0      0      1      0
	 * 3:       0      0      1      1
	 * 4:       0      1      0      0
	 * 5:       0      1      0      1
	 * 6:       0      1      1      0
	 * 7:       0      1      1      1
	 * 8:       1      0      0      0
	 * 9:       1      0      0      1
	 * 10:      1      0      1      0
	 * 11:      1      0      1      1
	 * 12:      1      1      0      0
	 * 13:      1      1      0      1
	 * 14:      1      1      1      0
	 * 15:      1      1      1      1
	 *
	 */
	switch (s) {
		/*
		 * If the new values equal to the last values,
		 * then nothing changed.
		 */
		case 0: case 5: case 10: case 15:
			break;
		/*
		 * If A channel has a falling edge while B is low,
		 * or B channel has a falling edge while A is high,
		 * or A channel has a rising edge while B is high,
		 * or B channel has a rising edge while A is low,
		 * then the encoder is turning in the positive
		 * direction, so increase the encoder_position variable
		 */
		case 1: case 7: case 8: case 14:
			encoder_position_act++; break;
		/*
		 * If A channel has a rising edge while B is low,
		 * or B channel has a rising edge while A is high,
		 * or A channel has a falling edge while B is high,
		 * or B channel has a falling edge while A is low,
		 * then the encoder is turning in the negative
		 * direction, so decrease the encoder_position variable
		 */
		case 2: case 4: case 11: case 13:
			encoder_position_act--; break;
		/*
		 * If both A and B channels value change in one step,
		 * then minimum one edge transition was missed. Error
		 * counter should be increased
		 */
		default:
			enc_error++; break;
	}
	/*
	 * Put the encoder channels state in the encoder_state
	 * global variable for the next evaluation of this interrupt
	 * service routine.
	 */
	encoder_state = (s >> 2);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	/*
	 * Check if the interrupt request was made by either one
	 * of the encoder pins
	 * [panel] ENC_PHASE_A = jobb gomb (PB13), ENC_PHASE_B = bal gomb (PB15)
	 */
    if ( GPIO_Pin == ENC_PHASE_A_Pin || GPIO_Pin == ENC_PHASE_B_Pin)
    {
    	// If so, then evaluate the encoder_state updater function.
    	updateEncoderPosition(
    		HAL_GPIO_ReadPin(ENC_PHASE_A_GPIO_Port, ENC_PHASE_A_Pin),
    		HAL_GPIO_ReadPin(ENC_PHASE_B_GPIO_Port, ENC_PHASE_B_Pin)
    	);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim == &htim10)
	{
		/* Calculate change of encoder position */
		int32_t position_difference = encoder_position_act - encoder_position_prev;
		encoder_position_prev = encoder_position_act;

		/* TIM10: 168 MHz / 1680 / 1000 = 100 Hz */
		float frequency = (float) SystemCoreClock / ((float) (htim->Init.Period + 1) * (float) (htim->Init.Prescaler + 1));
		float speed = (float) position_difference * frequency;
		speed /= MOTOR_ENCODER_RESOLUTION * MOTOR_ENCODER_STEPS;

		encoder_speed_rpm = speed * 60;
		output_speed_rpm = encoder_speed_rpm / MOTOR_GEAR_RATIO;
	}
}
