/**********************************************************************************************
 * HoverboardAPI Library - Version 0.0.2
 **********************************************************************************************/

#include "HoverboardAPI.h"
#include "hbprotocol/protocol.h"
#include "hbprotocol/protocol_private.h"
#include "protocolFunctions.h"
#include <stdio.h>



/*Constructor (...)*********************************************************
 * Hand over function pointer where to send data.
 *
 * Arduino example:
 *
 *    int serialWrapper(unsigned char *data, int len) {
 *      return (int) Serial.write(data,len);
 *    }
 *    HoverboardAPI hoverboard = HoverboardAPI(serialWrapper);
 *
 ***************************************************************************/

extern "C" {
  extern void delay(uint32_t ms);
  extern unsigned long millis(void);
}

uint32_t tickWrapper(void) { return (uint32_t) millis(); }

HoverboardAPI::HoverboardAPI(int (*send_serial_data)( unsigned char *data, int len )) {
  if(protocol_init(&s) != 0) while(1) {};
  setup_protocol();
  s.send_serial_data = send_serial_data;
  s.send_serial_data_wait = send_serial_data;
  s.allow_ascii = 0;       // do not allow ASCII parsing.
//  s.timeout1 = 50; //timeout for ACK
//  s.timeout2 = 10; // timeout between characters
  protocol_GetTick = tickWrapper;
  protocol_Delay = delay;
  setParamHandler(Codes::sensHall, NULL); // Disable callbacks for Hall
}


/***************************************************************************
 * Input function. Feed with Serial.read().
 ***************************************************************************/
void HoverboardAPI::protocolPush(unsigned char byte) {
  protocol_byte(&s, byte);
}

/***************************************************************************
 * Triggers message sending from Buffer and scheduled messages.
 ***************************************************************************/
void HoverboardAPI::protocolTick() {
  protocol_tick(&s);
}

/***************************************************************************
 * Returns local TX Buffer level
 ***************************************************************************/
int HoverboardAPI::getTxBufferLevel() {
  return (mpTxQueued(&s.ack.TxBuffer) + mpTxQueued(&s.noack.TxBuffer));
}

/***************************************************************************
 * Sets a callback Function to handle Protocol Read or Write events
 ***************************************************************************/

PARAMSTAT_FN HoverboardAPI::updateParamHandler(Codes code, PARAMSTAT_FN callback) {
  PARAMSTAT_FN old = getParamHandler(code);
  setParamHandler(code, callback);
  return old;
}

/***************************************************************************
 * Sets a Variable into which data is stored on receive and taken from for
 * scheduled sending.
 ***************************************************************************/

int HoverboardAPI::updateParamVariable(Codes code, void *ptr, int len) {
  if(params[code] != NULL) {
    return setParamVariable(code, params[code]->ui_type, ptr, len, params[code]->rw );
  }

  return 1;
}


/***************************************************************************
 * Print Protocol Statistics. Remote Data has to be requested first.
 ***************************************************************************/
void HoverboardAPI::printStats(Stream &port) {
  char buffer [100];
  extern PROTOCOLCOUNT ProtocolcountData;

/*
  snprintf ( buffer, 100, "ACK RX: %4li TX: %4li RXmissing: %4li TXretries: %4i    ", s.ack.counters.rx, s.ack.counters.tx, s.ack.counters.rxMissing, s.ack.counters.txRetries);
  port.print(buffer);
  snprintf ( buffer, 100, "NOACK RX: %4li TX: %4li RXmissing: %4li TXretries: %4i    ", s.noack.counters.rx, s.noack.counters.tx, s.noack.counters.rxMissing, s.noack.counters.txRetries);
  port.print(buffer);
  snprintf ( buffer, 100, "Received RX: %4li TX: %4li RXmissing: %4li TXretries: %4i    ",  ProtocolcountData.rx, ProtocolcountData.tx, ProtocolcountData.rxMissing, ProtocolcountData.txRetries);
  port.print(buffer);
*/

  snprintf ( buffer, 100, "Local  RX: %4li TX: %4li RXmissing: %4li    ", s.ack.counters.rx + s.noack.counters.rx, s.ack.counters.tx + s.noack.counters.tx, s.ack.counters.rxMissing + s.noack.counters.rxMissing);
  port.print(buffer);
  snprintf ( buffer, 100, "Remote RX: %4li TX: %4li RXmissing: %4li    ",  ProtocolcountData.rx, ProtocolcountData.tx, ProtocolcountData.rxMissing);
  port.print(buffer);
  snprintf ( buffer, 100, "Missed Local->Remote %4li (%4li) Remote->Local %4li (%4li)", s.ack.counters.tx + s.noack.counters.tx - ProtocolcountData.rx, ProtocolcountData.rxMissing, ProtocolcountData.tx - s.ack.counters.rx - s.noack.counters.rx, s.ack.counters.rxMissing + s.noack.counters.rxMissing);
  port.print(buffer);
  port.println();
}


/***************************************************************************
 *    Triggers a readout of data from the hoverboard
 *    It is necessary to set a callback if something should happen when the
 *    data arrives. Otherwise the data can just be read from the variable.
 ***************************************************************************/
void HoverboardAPI::requestRead(Codes code, char som) {

    // Compose new Message, no ACK needed.
    PROTOCOL_MSG2 msg = {
      .SOM = som,
    };

    // Message structure is for reading values.
    PROTOCOL_BYTES_READVALS *readvals = (PROTOCOL_BYTES_READVALS *) &(msg.bytes);
    readvals->cmd  = PROTOCOL_CMD_READVAL;  // Read value

    // Code indicating which variable should be read. See params[] in protocol.c
    readvals->code = code;

    msg.len = sizeof(readvals->cmd) + sizeof(readvals->code);

    protocol_post(&s, &msg);
}

/***************************************************************************
 * Schedules periodic transmission of value from control to hoverboard
 * count -1 for indefinetely
 ***************************************************************************/

void HoverboardAPI::scheduleTransmission(Codes code, int count, unsigned int period, char som) {
  PROTOCOL_SUBSCRIBEDATA SubscribeData;
  SubscribeData.code   = code;
  SubscribeData.count  = count;
  SubscribeData.period = period;
  SubscribeData.next_send_time = 0;
  SubscribeData.som = som;

  // Use native Subscription function to fill in array.
  if(params[Codes::protocolSubscriptions] && params[Codes::protocolSubscriptions]->fn) {
    params[Codes::protocolSubscriptions]->fn( &s, params[Codes::protocolSubscriptions], FN_TYPE_POST_WRITE, (unsigned char*) &SubscribeData, sizeof(SubscribeData) );
  }
}


/***************************************************************************
 *    Triggers a periodic readout of data from the hoverboard
 *    It is necessary to set a callback if something should happen when the
 *    data arrives. Otherwise the data can just be read from the variable.
 ***************************************************************************/
void HoverboardAPI::scheduleRead(Codes code, int count, unsigned int period, char som) {

  // Compose new Message, with ACK.
  PROTOCOL_MSG2 msg = {
    .SOM = PROTOCOL_SOM_ACK,
  };

  // Prepare Message structure to write subscription values.
  PROTOCOL_BYTES_WRITEVALS *writevals = (PROTOCOL_BYTES_WRITEVALS *) &(msg.bytes);
  PROTOCOL_SUBSCRIBEDATA *writesubscribe = (PROTOCOL_SUBSCRIBEDATA *) writevals->content;

  writevals->cmd  = PROTOCOL_CMD_WRITEVAL;  // Read value

  // Write into Subscriptions
  writevals->code = Codes::protocolSubscriptions;


  // Code indicating which variable should be read. See params[] in protocol.c
  writesubscribe->code = code;
  writesubscribe->count = count;
  writesubscribe->period = period;
  writesubscribe->som = som;

  msg.len = sizeof(writevals->cmd) + sizeof(writevals->code) + sizeof(*writesubscribe);

  protocol_post(&s, &msg);
}


/***************************************************************************
 * Sends PWM values to hoverboard
 ***************************************************************************/
void HoverboardAPI::sendPWM(int16_t pwm, int16_t steer, char som) {

  // Compose new Message
  PROTOCOL_MSG2 msg = {
    .SOM = som,
  };

  // Prepare Message structure to write PWM values.
  PROTOCOL_BYTES_WRITEVALS *writevals = (PROTOCOL_BYTES_WRITEVALS *) &(msg.bytes);
  PROTOCOL_PWM_DATA *writespeed = (PROTOCOL_PWM_DATA *) writevals->content;


  writevals->cmd  = PROTOCOL_CMD_WRITEVAL;  // Write value

  writevals->code = Codes::setPointPWM;

  writespeed->pwm[0] = pwm + steer;
  writespeed->pwm[1] = pwm - steer;

  msg.len = sizeof(writevals->cmd) + sizeof(writevals->code) + sizeof(writespeed->pwm);
  protocol_post(&s, &msg);
}

/***************************************************************************
 * Sends PWM values and Limits to hoverboard
 ***************************************************************************/
void HoverboardAPI::sendPWMData(int16_t pwm, int16_t steer, int speed_max_power, int speed_min_power, int speed_minimum_pwm, char som) {

  // Compose new Message
  PROTOCOL_MSG2 msg = {
    .SOM = som,
  };

  // Prepare Message structure to write PWM values.
  PROTOCOL_BYTES_WRITEVALS *writevals = (PROTOCOL_BYTES_WRITEVALS *) &(msg.bytes);
  PROTOCOL_PWM_DATA *writespeed = (PROTOCOL_PWM_DATA *) writevals->content;


  writevals->cmd  = PROTOCOL_CMD_WRITEVAL;  // Write value

  writevals->code = Codes::setPointPWMData;

  writespeed->pwm[0] = pwm + steer;
  writespeed->pwm[1] = pwm - steer;
  writespeed->speed_max_power = speed_max_power;
  writespeed->speed_min_power = speed_min_power;
  writespeed->speed_minimum_pwm = speed_minimum_pwm;


  msg.len = sizeof(writevals->cmd) + sizeof(writevals->code) + sizeof(*writespeed);
  protocol_post(&s, &msg);
}

/***************************************************************************
 * Sends Buzzer data to hoverboard
 ***************************************************************************/
void HoverboardAPI::sendBuzzer(uint8_t buzzerFreq, uint8_t buzzerPattern, uint16_t buzzerLen, char som) {

  // Compose new Message
  PROTOCOL_MSG2 msg = {
    .SOM = som,
  };

  // Prepare Message structure to write buzzer values.
  PROTOCOL_BYTES_WRITEVALS *writevals = (PROTOCOL_BYTES_WRITEVALS *) &(msg.bytes);
  PROTOCOL_BUZZER_DATA *writebuzzer = (PROTOCOL_BUZZER_DATA *) writevals->content;


  writevals->cmd  = PROTOCOL_CMD_WRITEVAL;  // Write value

  writevals->code = Codes::setBuzzer;

  writebuzzer->buzzerFreq = buzzerFreq;
  writebuzzer->buzzerPattern = buzzerPattern;
  writebuzzer->buzzerLen = buzzerLen;


  msg.len = sizeof(writevals->cmd) + sizeof(writevals->code) + sizeof(*writebuzzer);
  protocol_post(&s, &msg);
}

/***************************************************************************
 * Sends enable to hoverboard
 ***************************************************************************/
void HoverboardAPI::sendEnable(uint8_t newEnable, char som) {

  // Compose new Message
  PROTOCOL_MSG2 msg = {
    .SOM = som,
  };

  // Prepare Message structure to write buzzer values.
  PROTOCOL_BYTES_WRITEVALS *writevals = (PROTOCOL_BYTES_WRITEVALS *) &(msg.bytes);
  uint8_t *writeenable = (uint8_t *) writevals->content;


  writevals->cmd  = PROTOCOL_CMD_WRITEVAL;  // Write value

  writevals->code = Codes::enableMotors;

  *writeenable = newEnable;


  msg.len = sizeof(writevals->cmd) + sizeof(writevals->code) + sizeof(*writeenable);
  protocol_post(&s, &msg);
}

/***************************************************************************
 * Reset statistic counters
 ***************************************************************************/
void HoverboardAPI::sendCounterReset(char som) {

  // Compose new Message
  PROTOCOL_MSG2 msg = {
    .SOM = som,
  };

  // Prepare Message structure to write buzzer values.
  PROTOCOL_BYTES_WRITEVALS *writevals = (PROTOCOL_BYTES_WRITEVALS *) &(msg.bytes);
  PROTOCOLCOUNT *writeprotocolcount = (PROTOCOLCOUNT *) writevals->content;


  writevals->cmd  = PROTOCOL_CMD_WRITEVAL;  // Write value

  writevals->code = Codes::protocolCountSum;

  writeprotocolcount->rx = 0;
  writeprotocolcount->rxMissing = 0;
  writeprotocolcount->tx = 0;
  writeprotocolcount->txRetries = 0;
  writeprotocolcount->txFailed = 0;
  writeprotocolcount->unwantedacks = 0;
  writeprotocolcount->unwantednacks = 0;
  writeprotocolcount->unknowncommands = 0;
  writeprotocolcount->unplausibleresponse = 0;

  msg.len = sizeof(writevals->cmd) + sizeof(writevals->code) + sizeof(*writeprotocolcount);
  protocol_post(&s, &msg);
}

/***************************************************************************
 * Reset statistic counters
 ***************************************************************************/
void HoverboardAPI::resetCounters() {

  s.ack.counters.rx = 0;
  s.ack.counters.rxMissing = 0;
  s.ack.counters.tx = 0;
  s.ack.counters.txRetries = 0;
  s.ack.counters.txFailed = 0;
  s.ack.counters.unwantedacks = 0;
  s.ack.counters.unwantednacks = 0;
  s.ack.counters.unknowncommands = 0;
  s.ack.counters.unplausibleresponse = 0;

  s.noack.counters.rx = 0;
  s.noack.counters.rxMissing = 0;
  s.noack.counters.tx = 0;
  s.noack.counters.txRetries = 0;
  s.noack.counters.txFailed = 0;
  s.noack.counters.unwantedacks = 0;
  s.noack.counters.unwantednacks = 0;
  s.noack.counters.unknowncommands = 0;
  s.noack.counters.unplausibleresponse = 0;
}

/***************************************************************************
 * returns electrical Measurements. Readout has to be requested before with
 * requestRead or scheduling.
 ***************************************************************************/
float HoverboardAPI::getBatteryVoltage() {
  return electrical_measurements.batteryVoltage;
}

int HoverboardAPI::getBatteryVoltageRaw() {
  return electrical_measurements.bat_raw;
}

float HoverboardAPI::getBoardTemperature() {
  return electrical_measurements.board_temp_deg_c;
}

float HoverboardAPI::getBoardTemperatureFiltered() {
  return electrical_measurements.board_temp_filtered;
}

int HoverboardAPI::getBoardTemperatureRaw() {
  return electrical_measurements.board_temp_raw;
}

int HoverboardAPI::isCharging() {
  return electrical_measurements.charging;
}

int HoverboardAPI::getDCCurrentLimit() {
  return (electrical_measurements.dcCurLim) / 100.0;
}
int HoverboardAPI::getDCCurrentLimitAdc() {
  return electrical_measurements.dc_adc_limit;
}

float HoverboardAPI::getMotorAmps(uint8_t motor) {
  if(motor > sizeof(electrical_measurements.motors)/sizeof(electrical_measurements.motors[0])) return -1.0;
  return electrical_measurements.motors[motor].dcAmps;
}

float HoverboardAPI::getMotorAmpsAvg(uint8_t motor) {
  if(motor > sizeof(electrical_measurements.motors)/sizeof(electrical_measurements.motors[0])) return -1.0;
  return electrical_measurements.motors[motor].dcAmpsAvg;
}

float HoverboardAPI::getMotorAmpsAvgAcc(uint8_t motor) {
  if(motor > sizeof(electrical_measurements.motors)/sizeof(electrical_measurements.motors[0])) return -1.0;
  return electrical_measurements.motors[motor].dcAmpsAvgAcc;
}

int HoverboardAPI::getMotorPwmLimiter(uint8_t motor) {
  if(motor > sizeof(electrical_measurements.motors)/sizeof(electrical_measurements.motors[0])) return -1.0;
  return electrical_measurements.motors[motor].pwm_limiter;
}

int HoverboardAPI::getMotorPwmRequested(uint8_t motor) {
  if(motor > sizeof(electrical_measurements.motors)/sizeof(electrical_measurements.motors[0])) return -1.0;
  return electrical_measurements.motors[motor].pwm_requested;
}

int HoverboardAPI::getMotorPwmActual(uint8_t motor) {
  if(motor > sizeof(electrical_measurements.motors)/sizeof(electrical_measurements.motors[0])) return -1.0;
  return electrical_measurements.motors[motor].pwm_actual;
}

/***************************************************************************
 * returns hall data. Readout has to be requested before with
 * requestRead or scheduling.
 ***************************************************************************/
double HoverboardAPI::getSpeed_kmh() {
  return   (HallData[0].HallSpeed_mm_per_s + HallData[1].HallSpeed_mm_per_s) / 2.0 * 3600.0 / 1000000.0;
}

double HoverboardAPI::getSteer_kmh() {
  return   (HallData[0].HallSpeed_mm_per_s * 3600.0 / 1000000.0 )- getSpeed_kmh();
}

double HoverboardAPI::getSpeed_mms() {
  return   (HallData[0].HallSpeed_mm_per_s + HallData[1].HallSpeed_mm_per_s) / 2.0;
}

double HoverboardAPI::getSteer_mms() {
  return   HallData[0].HallSpeed_mm_per_s - getSpeed_mms();
}

double HoverboardAPI::getSpeed0_mms() {
  return   HallData[0].HallSpeed_mm_per_s;
}

double HoverboardAPI::getSpeed1_mms() {
  return   HallData[1].HallSpeed_mm_per_s;
}

double HoverboardAPI::getSpeed0_kmh() {
  return   HallData[0].HallSpeed_mm_per_s * 3600.0 / 1000000.0;
}

double HoverboardAPI::getSpeed1_kmh() {
  return   HallData[1].HallSpeed_mm_per_s * 3600.0 / 1000000.0;
}
