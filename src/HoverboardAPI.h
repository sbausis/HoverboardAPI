#pragma once

#include "hbprotocol/protocol.h"
#include "Stream.h"

class HoverboardAPI
{
  public:

    enum Codes {
  protocolVersion          = 0x00,
  protocolSubscriptions    = 0x22,
  protocolCountSum         = 0x23,
  protocolCountACK         = 0x23,
  protocolCountnoACK       = 0x23,
  sensHall                 = 0x02,
  sensElectrical           = 0x08,
  enableMotors             = 0x09,
  disablePoweroff          = 0x0A,
  debugOut                 = 0x0B,
  setPointPWMData          = 0x0D,
  setPointPWM              = 0x0E,
  setBuzzer                = 0x21,
};

  //commonly used functions **************************************************************************
    HoverboardAPI(int (*send_serial_data)( unsigned char *data, int len ));          // * constructor.

    void protocolPush(unsigned char byte);
    void protocolTick();
    PARAMSTAT_FN updateParamHandler(Codes code, PARAMSTAT_FN callback);
    int updateParamVariable(Codes code, void *ptr, int len);

    void scheduleTransmission(Codes code, int count, unsigned int period, char som = PROTOCOL_SOM_NOACK);

    void requestRead(Codes code, char som = PROTOCOL_SOM_NOACK);
    void scheduleRead(Codes code, int count, unsigned int period, char som = PROTOCOL_SOM_NOACK);


    void sendPWM(int16_t pwm, int16_t steer = 0, char som = PROTOCOL_SOM_NOACK);
    void sendPWMData(int16_t pwm, int16_t steer = 0, int speed_max_power = 600, int speed_min_power = -600, int speed_minimum_pwm = 10, char som = PROTOCOL_SOM_ACK);
    void sendEnable(uint8_t newEnable, char som = PROTOCOL_SOM_ACK);
    void sendBuzzer(uint8_t buzzerFreq = 4, uint8_t buzzerPattern = 0, uint16_t buzzerLen = 100, char som = PROTOCOL_SOM_NOACK);
    void sendCounterReset(char som = PROTOCOL_SOM_ACK);

    int getTxBufferLevel();
	
    float getBatteryVoltage();
	int getBatteryVoltageRaw();
	
	float getBoardTemperature();
	float getBoardTemperatureFiltered();
	int getBoardTemperatureRaw();
	
	int isCharging();
	
	int getDCCurrentLimit();
	int getDCCurrentLimitAdc();
	
	float getMotorAmps(uint8_t motor);
    float getMotorAmpsAvg(uint8_t motor);
	float getMotorAmpsAvgAcc(uint8_t motor);
	
	int getMotorPwmLimiter(uint8_t motor);
	int getMotorPwmRequested(uint8_t motor);
	int getMotorPwmActual(uint8_t motor);
	
    double getSpeed_kmh();
    double getSteer_kmh();
    double getSpeed_mms();
    double getSteer_mms();
    double getSpeed0_kmh();
    double getSpeed1_kmh();
    double getSpeed0_mms();
    double getSpeed1_mms();

    void resetCounters();
    void printStats(Stream &Port);


    //available but not commonly used functions ********************************************************

  private:
    PROTOCOL_STAT s;


};

