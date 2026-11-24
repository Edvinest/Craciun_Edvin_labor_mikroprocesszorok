/*
 * motor.h
 *
 *  Created on: 2019. nov. 22.
 *      Author: girgaszpeter
 *
 *  [panel] A Kristalytiszta panelre (STM32F405, BD6211F) atirva, 2026.11.
 *          A [panel] jelu sorok ujak vagy masok, mint a Nucleos valtozatban.
 */

#ifndef MOTOR_H_
#define MOTOR_H_

#include "main.h"

#define MOTOR_SUPPLY_VOLTAGE		5.0F
#define MOTOR_KV					3000
/*
 * Constants for motor encoder resolution. Means the number
 * of encoder pulses per a complete motor rotation.
 */
#define MOTOR_ENCODER_RESOLUTION	16U

/*
 * Constants for motor encoder evaluation. Means the number
 * of encoder pulses per an encoder step.
 */
#define MOTOR_ENCODER_STEPS			2U

/*
 * Constants for motor gear reduction ratio. Means the number
 * of motor rotations per output shaft rotation.
 */
#define MOTOR_GEAR_RATIO			120U

/*
 * [panel] A BD6211F bemenetei. A PA5-on a TIM2 CH1 PWM-je jon ki
 * (a tankonyv 12. fejezete szerint ez a motor PWM-vezerlojele).
 * A masik bemenet (RIN) laba meg nem ismert, ezert a program nem
 * vezerli; feltetelezzuk, hogy L szinten marad (ezt a panelen meg
 * kell nezni). Ezert csak az "elore" uzemmodok mennek: a, b, e, i.
 */
#define MOTOR_PORT					MOTOR_PWM_GPIO_Port
#define MOTOR_FIN_PIN				MOTOR_PWM_Pin

/*
 * [panel] A kapcsolo (SW1-SW4) BE allasanak szintje. Ha a panelen
 * forditva van (BE allasban 0-t olvasunk), ide GPIO_PIN_RESET kell.
 */
#define HMI_SW_BE					GPIO_PIN_SET

/*
 * [panel] Uzemmodok (a tankonyv 3. es 4. tablazata).
 * SW1 = b, SW3 = e, SW4 = i, minden mas kapcsoloallas = a.
 */
typedef enum
{
	MOTOR_MODE_A = 'a',	/* keszenlet: FIN = L, RIN = L             */
	MOTOR_MODE_B = 'b',	/* maximalis sebesseggel elore: FIN = H     */
	MOTOR_MODE_E = 'e',	/* PWM: FIN = PWM (TIM2), VREF = VCC        */
	MOTOR_MODE_I = 'i'	/* analog: FIN = H, VREF = DAC (PA4)        */
} Motor_Mode_t;

/*
 * The required percent of maximum speed.
 */
extern int32_t set_speed_percent;

/*
 * [panel] Az aktualis uzemmod es a kapcsolok allapota
 * (0. bit = SW1, 1. bit = SW2, 2. bit = SW3, 3. bit = SW4).
 */
extern Motor_Mode_t motor_mode;
extern uint8_t switch_state;

/*
 * The PWM duty of the motor. Value is between 0x0000 and 0xFFFF.
 */
extern uint16_t Motor_PWMDuty;

/*
 * The latest state of the encoders inputs. It's needed for the
 * evaluation of the new input states in the interrupt service
 * routine. Values is one of 0b0000, 0b0001, 0b0010, 0b0011.
 * The LSB stores Phase A value, the second bit stores the
 * Phase B value.
 */
extern uint8_t encoder_state;

/*
 * The actual position of the encoder.
 */
extern int32_t encoder_position_act;

/*
 * The previous position of the encoder.
 */
extern int32_t encoder_position_prev;

/*
 * Encoder error counter
 */
extern int32_t enc_error;

/*
 * Speed of the encoder in RPM.
 */
extern float encoder_speed_rpm;

/*
 * Speed of the output shaft in RPM.
 */
extern float output_speed_rpm;

/*
 * @brief Initialize global variables of motor driver,
 *        start the PWM (TIM2), the DAC and the speed timer (TIM10).
 */
void Motor_init(void);

/*
 * @brief [panel] A labat sima GPIO-kimenetre allitja (a tankonyv fuggvenye).
 */
void Motor_setPinGPIO(uint16_t pin);

/*
 * @brief [panel] A labat a TIM2 PWM-kimenetere allitja (a tankonyv fuggvenye).
 */
void Motor_setPinPWM(uint16_t pin);

/*
 * @brief Set the duty cycle of the PWM output.
 *        The CCR of the used timer (TIM2) will be set according
 *        to the value parameter. Can be between 0x0000 and 0xFFFF.
 */
void Motor_setPWMDuty(uint16_t value);

/*
 * @brief Set the duty cycle in percent of the PWM output (0 ... 100).
 */
void Motor_setPWMDutyPercent(float value);

/*
 * @brief [panel] Beolvassa az SW1-SW4 kapcsolokat, es ha az uzemmod
 *        valtozott, beallitja a BD6211F bemeneteit (SetMotorOpMode).
 */
void Motor_setOpModeFromSwitches(void);

/*
 * @brief [panel] A sebesseg szazalekban (0 ... 100): e modban
 *        a PWM kitoltese, i modban a VREF (DAC) feszultsege.
 */
void Motor_setSpeedPercent(int32_t percent);

void updateEncoderPosition(uint8_t phaseA, uint8_t phaseB);

/*
 * @brief This interrupt service routine is called by the HAL library
 *        when there's a change of any of the encoder pins.
 *        [panel] Az enkoder a bal es a jobb gomb (kezi enkoder).
 *
 * @param The GPIO pin wich caused the interrupt
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/*
 * @brief Callback for the TIM10s overflow update
 *
 * @param The instance of the timer that made the interrupt
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

#endif /* MOTOR_H_ */
