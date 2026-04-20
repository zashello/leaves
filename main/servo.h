#pragma once

/**
 * 将舵机移动到90度
 */
void servo_move_to_90_degrees(void);

/**
 * 将舵机移动到指定角度
 * @param angle 0-180度的角度
 */
void servo_move_to_angle(uint8_t angle);
