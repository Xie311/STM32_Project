/*
 * @Author: szf
 * @Date: 2023-02-22 12:04:21
 * @LastEditTime: 2024-04-21 14:50:58
 * @LastEditors: x311 
 * @brief 运动学逆解算及PID计算函数
 * @FilePath: \WTR_Omni_Auto_Chassis\UserCode\Lib\Calculate\wtr_calculate.c
 */

#include "wtr_calculate.h"
#include <math.h>
#include "Chassis_UserConfig.h"

double moter_speed[4];

// 麦轮底盘参数
#define rotate_ratio    0.3615   // (Width + Length)/2
#define wheel_rpm_ratio 2387.324 // 换算线速度到rpm

/**
 * @brief麦克纳姆轮底盘逆解算
 * @author: szf
 * @date:
 * @return {void}
 */
void CalculateFourMecanumWheels(double *moter_speed, double vx, double vy, double vw)
{
    moter_speed[0] = (vx - vy - vw * rotate_ratio) * wheel_rpm_ratio;  // 前左
    moter_speed[1] = (vx + vy - vw * rotate_ratio) * wheel_rpm_ratio;  // 前右
    moter_speed[2] = (-vx + vy - vw * rotate_ratio) * wheel_rpm_ratio; // 后右
    moter_speed[3] = (-vx - vy - vw * rotate_ratio) * wheel_rpm_ratio; // 后左
}

/**
 * @brief: 码盘串级PID
 * @author: X311 2024/4/21
 * @param： ChassisControl 底盘运动状态
 * @param： OPS_Data  码盘反馈数据
 * @note：  通过码盘反馈的当前位置与上位机传来的目标位置PID计算得到vx、vy
 * @note：  将vx、vy存储到ChassisControl中的velocity结构体内
 */
void OPS_Servo(CHASSIS_MOVING_STATE *ChassisControl, OPS_t *OPS_Data)
{
    OPS_Data->opsPID_x.ref = ChassisControl->position.x;
    OPS_Data->opsPID_y.ref = ChassisControl->position.y;

    OPS_Data->opsPID_x.fdb = OPS_Data->pos_x;
    OPS_Data->opsPID_y.fdb = OPS_Data->pos_y;

    PID_Calc(&(OPS_Data->opsPID_x));
    PID_Calc(&(OPS_Data->opsPID_y));

    ChassisControl->velocity.x = OPS_Data->opsPID_x.output;
    ChassisControl->velocity.y = OPS_Data->opsPID_y.output;
    ChassisControl->velocity.w = OPS_Data->w_z;
}


// /**
//  * @brief: 串级PID控制
//  * @author：X311 2024/4/13
//  * @param TargetPosition：期望位置--来自大疆遥控/上位机
//  * @param motor : 电机结构体--当前速度
//  * @param OPS_data:码盘反馈值--当前位置
//  * @return {*}
//  */
// void CasServo(Tar_t *TargetPosition, DJI_t *motor, OPS_t *OPS_Data)
// {
//     //理论上应该修改第二层PID的参数（？），但是牵一发而动全身，所以尝试通过比例系数K得到修改Kp的效果
//     float k  = 1;
//     OPS_Data->opsPID.ref = sqrt((TargetPosition->pos_x) * (TargetPosition->pos_x) + (TargetPosition->pos_y) * (TargetPosition->pos_y));
//     OPS_Data->opsPID.fdb = sqrt((OPS_Data->pos_x) * (OPS_Data->pos_x) +(OPS_Data->pos_y) * (OPS_Data->pos_y));
//     PID_Calc(&(OPS_Data->opsPID));

//     motor->speedPID.ref = k*OPS_Data->opsPID.output;
//     motor->speedPID.fdb = motor->FdbData.rpm;
//     PID_Calc(&(motor->speedPID));

//     //OPS_Data->opsPID.ref = TargetPosition->pos_y;
//     //OPS_Data->opsPID.fdb = OPS_Data->pos_y;
// }


/**
 * @brief: PID控制-增量式PID
 * @auther: Chen YiTong 3083697520
 * @param {__IO PID_t} *pid
 * @return {*}
 */

void PID_Calc(__IO PID_t *pid)
{
    pid->cur_error = pid->ref - pid->fdb;
    pid->output += pid->KP * (pid->cur_error - pid->error[1]) + pid->KI * pid->cur_error + pid->KD * (pid->cur_error - 2 * pid->error[1] + pid->error[0]);
    pid->error[0] = pid->error[1];
    pid->error[1] = pid->ref - pid->fdb;
    /*设定输出上限*/
    if (pid->output > pid->outputMax) pid->output = pid->outputMax;
    if (pid->output < -pid->outputMax) pid->output = -pid->outputMax;
}


/**
 * @brief: PID算法-P控制
 * @auther: Chen YiTong 3083697520
 * @param {__IO PID_t} *pid
 * @return {*}
 */
void P_Calc(__IO PID_t *pid)
{
    pid->cur_error = pid->ref - pid->fdb;
    pid->output    = pid->KP * pid->cur_error;
    /*设定输出上限*/
    if (pid->output > pid->outputMax) pid->output = pid->outputMax;
    if (pid->output < -pid->outputMax) pid->output = -pid->outputMax;

    if (fabs(pid->output) < pid->outputMin)
        pid->output = 0;
}


/**
 * @brief: 位置伺服
 * @auther: Chen YiTong 3083697520
 * @param {float} ref 目标位置
 * @param {DJI_t} *motor 电机结构体
 * @return {*}
 * @note 位置-速度串级
 */
void positionServo(float ref, DJI_t *motor)
{
    motor->posPID.ref = ref;
    motor->posPID.fdb = motor->AxisData.AxisAngle_inDegree;
    PID_Calc(&(motor->posPID));

    motor->speedPID.ref = motor->posPID.output;
    motor->speedPID.fdb = motor->FdbData.rpm;
    PID_Calc(&(motor->speedPID));
}

/**
 * @brief: 速度伺服函数
 * @auther: Chen YiTong 3083697520
 * @param {float} ref 目标值
 * @param {DJI_t} *motor 电机结构体
 * @return {*}
 */
void speedServo(float ref, DJI_t *motor)
{
    motor->speedPID.ref = ref;
    motor->speedPID.fdb = motor->FdbData.rpm;
    PID_Calc(&(motor->speedPID));
}


/**
 * @brief: 圆周死区控制
 * @auther: szf
 * @param {double} x
 * @param {double} y
 * @param {double} *new_x
 * @param {double} *new_y
 * @param {double} threshould
 * @return {*}
 */
void DeadBand(double x, double y, double *new_x, double *new_y, double threshould)
{
    double length     = sqrt(x * x + y * y);
    double new_length = length - threshould;

    if (new_length <= 0) {
        *new_x = 0;
        *new_y = 0;
        return;
    }

    double k = new_length / length;

    *new_x = x * k;
    *new_y = y * k;
}

/**
 * @brief: 单轴死区控制
 * @auther: szf
 * @param {double} x
 * @param {double} *new_x
 * @param {double} threshould
 * @return {*}
 * @note   对输入值进行死区处理，防止过小的输入值对系统产生影响
 */
void DeadBandOneDimensional(double x, double *new_x, double threshould)
{
    // 计算输入值的绝对值与死区阈值的差值
    double difference_x = fabs(x) - threshould;
    
    // 如果差值小于0，即输入值在死区范围内
    if (difference_x < 0) {
        *new_x = 0;
        return;
    }

    // 计算缩放因子k，用于对输入值进行缩放
    double k = difference_x / fabs(x);
    *new_x   = k * x;
}
