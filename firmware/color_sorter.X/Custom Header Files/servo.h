#ifndef SERVO_H
#define SERVO_H

// PWM frequency in Hz
#define PWM_FREQ       50

// Timer counts for corresponding servo pulses (in timer ticks)
#define SERVO_RIGHT    1400    // 1.0 ms pulse (≈ 0°)
#define SERVO_LEFT     4800    // 2.4 ms pulse (≈ 180°)
#define SERVO_CENTER   3000    // 1.5 ms pulse (≈ 90°)


/**
 * @brief Initialize Timer2 and OC1 for PWM control of the servo.
 * 
 * Sets up 20 ms period (50 Hz) channel on RB5 / OC1.
 * Must be called before any move or toggle.
 */
void servo_init(void);

/**
 * @brief Cycle through CENTER → RIGHT → CENTER → LEFT → … 
 * 
 * Each call advances the servo to the next of four positions.
 */
void toggle_servo(void);

/**
 * @brief Move the servo fully to the right (0°).
 */
void moveRight(void);

/**
 * @brief Move the servo fully to the left (180°).
 */
void moveLeft(void);

/**
 * @brief Center the servo (≈ 90°).
 */
void moveCenter(void);

/**
 * @brief Set servo to an arbitrary angle between 0° and 180°.
 * @param angle  Desired angle in degrees; clipped to [0,180].
 * 
 * Uses linear mapping into the timer‑count range.
 */
void servo_set_angle(int angle);

#endif // SERVO_H