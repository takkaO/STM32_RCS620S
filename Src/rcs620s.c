/*
 * rcs620s.c
 *
 *  Created on: 2019/09/24
 *      Author: csel-pc05
 */

#include "rcs620s.h"

/* --------------------------------
 * Constant
 * -------------------------------- */
#define RCS620S_DEFAULT_TIMEOUT  1000

/* --------------------------------
 * Global variables
 * -------------------------------- */
UART_HandleTypeDef rcs620s_huart;
RCS620S_Global_Variables rcs620s_gv;

/* --------------------------------
 * Private prototypes
 * -------------------------------- */
void RCS620S_constructor(UART_HandleTypeDef _huart);
int RCS620S_rwCommand(const uint8_t *command, uint16_t commandLen,
		uint8_t response[RCS620S_MAX_RW_RESPONSE_LEN], uint16_t *responseLen);
void RCS620S_cancel(void);
uint8_t RCS620S_calcDCS(const uint8_t *data, uint16_t len);
void writeSerial(const uint8_t *buf, uint16_t bufLen);
int readSerial(uint8_t *buf, uint16_t bufLen);

/* --------------------------------
 * Public functions
 * -------------------------------- */
int RCS620S_initDevice(UART_HandleTypeDef _huart) {
	int ret;
	uint8_t response[RCS620S_MAX_RW_RESPONSE_LEN];
	uint16_t responseLen;

	/* Initialize */
	RCS620S_constructor(_huart);

	/* RFConfiguration (various timings) */
	ret = RCS620S_rwCommand((const uint8_t*) "\xd4\x32\x02\x00\x00\x00", 6, response, &responseLen);
	if (!ret || (responseLen != 2) || (memcmp(response, "\xd5\x33", 2) != 0)) {
		return 0;
	}

	/* RFConfiguration (max retries) */
	ret = RCS620S_rwCommand((const uint8_t*) "\xd4\x32\x05\x00\x00\x00", 6, response, &responseLen);
	if (!ret || (responseLen != 2) || (memcmp(response, "\xd5\x33", 2) != 0)) {
		return 0;
	}

	/* RFConfiguration (additional wait time = 24ms) */
	ret = RCS620S_rwCommand((const uint8_t*) "\xd4\x32\x81\xb7", 4, response, &responseLen);
	if (!ret || (responseLen != 2) || (memcmp(response, "\xd5\x33", 2) != 0)) {
		return 0;
	}

	return 1;
}

int RCS620S_polling(uint16_t systemCode) {
	int ret;
	uint8_t buf[9];
	uint8_t response[RCS620S_MAX_RW_RESPONSE_LEN];
	uint16_t responseLen;

	/* InListPassiveTarget */
	memcpy(buf, "\xd4\x4a\x01\x01\x00\xff\xff\x00\x00", 9);
	buf[5] = (uint8_t) ((systemCode >> 8) & 0xFF);
	buf[6] = (uint8_t) ((systemCode >> 0) & 0xFF);

	ret = RCS620S_rwCommand(buf, 9, response, &responseLen);
	if (!ret || (responseLen != 22) || (memcmp(response, "\xd5\x4b\x01\x01\x12\x01", 6) != 0)) {
		return 0;
	}

	memcpy(rcs620s_gv.idm, response + 6, 8);
	memcpy(rcs620s_gv.pmm, response + 14, 8);
	return 1;
}

int RCS620S_cardCommand(const uint8_t *command, uint8_t commandLen,
		uint8_t response[RCS620S_MAX_CARD_RESPONSE_LEN], uint8_t *responseLen) {
	int ret;
	uint16_t commandTimeout;
	uint8_t buf[RCS620S_MAX_RW_RESPONSE_LEN];
	uint16_t len;

	if (rcs620s_gv.timeout >= (0x10000 / 2)) {
		commandTimeout = 0xFFFF;
	} else {
		commandTimeout = (uint16_t) (rcs620s_gv.timeout * 2);
	}

	/* CommunicateThruEX */
	buf[0] = 0xD4;
	buf[1] = 0xA0;
	buf[2] = (uint8_t) ((commandTimeout >> 0) & 0xFF);
	buf[3] = (uint8_t) ((commandTimeout >> 8) & 0xFF);
	buf[4] = (uint8_t) (commandLen + 1);
	memcpy(buf + 5, command, commandLen);

	ret = RCS620S_rwCommand(buf, 5 + commandLen, buf, &len);
	if (!ret || (len < 4) || (buf[0] != 0xD5) || (buf[1] != 0xA1) || (buf[2] != 0x00)
			|| (len != (3 + buf[3]))) {
		return 0;
	}

	*responseLen = (uint8_t) (buf[3] - 1);
	memcpy(response, buf + 4, *responseLen);

	return 1;
}

int RCS620S_rfOff(void) {
	int ret;
	uint8_t response[RCS620S_MAX_RW_RESPONSE_LEN];
	uint16_t responseLen;

	/* RFConfiguration (RF field) */
	ret = RCS620S_rwCommand((const uint8_t*) "\xd4\x32\x01\x00", 4, response, &responseLen);
	if (!ret || (responseLen != 2) || (memcmp(response, "\xd5\x33", 2) != 0)) {
		return 0;
	}

	return 1;
}

int RCS620S_push(const uint8_t *data, uint8_t dataLen) {
	int ret;
	uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
	uint8_t responseLen;

	if (dataLen > 224) {
		return 0;
	}

	/* Push */
	buf[0] = 0xB0;
	memcpy(buf + 1, rcs620s_gv.idm, 8);
	buf[9] = dataLen;
	memcpy(buf + 10, data, dataLen);

	ret = RCS620S_cardCommand(buf, 10 + dataLen, buf, &responseLen);
	if (!ret || (responseLen != 10) || (buf[0] != 0xb1) ||
	        (memcmp(buf + 1, rcs620s_gv.idm, 8) != 0) || (buf[9] != dataLen)) {
		return 0;
	}

	buf[0] = 0xA4;
	memcpy(buf+1, rcs620s_gv.idm, 8);
	buf[9] = 0x00;

	ret = RCS620S_cardCommand(buf, 10, buf, &responseLen);
	if (!ret || (responseLen != 10) || (buf[0] != 0xa5) ||
	        (memcmp(buf + 1, rcs620s_gv.idm, 8) != 0) || (buf[9] != 0x00)) {
		return 0;
	}

	HAL_Delay(100);
	return 1;
}

int RCS620S_requestService(uint16_t serviceCode){
	int ret;
	uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
	uint8_t responseLen = 0;

	buf[0] = 0x02;
	memcpy(buf + 1, rcs620s_gv.idm, 8);
	buf[9] = 0x01;
	buf[10] = (uint8_t)((serviceCode >> 0) & 0xFF);
	buf[11] = (uint8_t)((serviceCode >> 8) & 0xFF);

	ret = RCS620S_cardCommand(buf, 12, buf, &responseLen);

	if(!ret || (responseLen != 12) || (buf[0] != 0x03) ||
	  (memcmp(buf + 1, rcs620s_gv.idm, 8) != 0) || ((buf[10] == 0xFF) && (buf[11] == 0xFF))) {
		return 0;
	}

	return 1;
}

int RCS620S_readEncryption(uint16_t serviceCode, uint8_t blockNumber, uint8_t *buf){
	int ret;
	uint8_t responseLen = 0;

	buf[0] = 0x06;
	memcpy(buf + 1, rcs620s_gv.idm, 8);
	buf[9] = 0x01; // Number of services
	buf[10] = (uint8_t)((serviceCode >> 0) & 0xFF);
	buf[11] = (uint8_t)((serviceCode >> 8) & 0xFF);
	buf[12] = 0x01; // Number of blocks
	buf[13] = 0x80;
	buf[14] = blockNumber;

	ret = RCS620S_cardCommand(buf, 15, buf, &responseLen);

	if (!ret || (responseLen != 28) || (buf[0] != 0x07) ||
	  (memcmp(buf + 1, rcs620s_gv.idm, 8) != 0)) {
		return 0;
	}

	return 1;
}

/* --------------------------------
 * Private functions
 * -------------------------------- */

void RCS620S_constructor(UART_HandleTypeDef _huart) {
	rcs620s_gv.timeout = RCS620S_DEFAULT_TIMEOUT;
	rcs620s_huart = _huart;
}

int RCS620S_rwCommand(const uint8_t *command, uint16_t commandLen,
		uint8_t response[RCS620S_MAX_RW_RESPONSE_LEN], uint16_t *responseLen) {
	int ret;
	uint8_t buf[9];

	uint8_t dcs = RCS620S_calcDCS(command, commandLen);

	/* Transmit the command */
	buf[0] = 0x00;
	buf[1] = 0x00;
	buf[2] = 0xFF;
	if (commandLen <= 255) {
		buf[3] = commandLen;
		buf[4] = (uint8_t) -buf[3];
		writeSerial(buf, 5);
	} else {
		buf[3] = 0xFF;
		buf[4] = 0xFF;
		buf[5] = (uint8_t) ((commandLen >> 8) & 0xFF);
		buf[6] = (uint8_t) ((commandLen >> 0) & 0xFF);
		buf[7] = (uint8_t) -(buf[5] + buf[6]);
		writeSerial(buf, 8);
	}

	writeSerial(command, commandLen);
	buf[0] = dcs;
	buf[1] = 0x00;
	writeSerial(buf, 2);

	/* Receive an ACK */
	ret = readSerial(buf, 6);
	if (!ret || (memcmp(buf, "\x00\x00\xff\x00\xff\x00", 6) != 0)) {
		RCS620S_cancel();
		return 0;
	}

	/* receive a response */
	ret = readSerial(buf, 5);
	if (!ret) {
		RCS620S_cancel();
		return 0;
	} else if (memcmp(buf, "\x00\x00\xff", 3) != 0) {
		return 0;
	}
	if ((buf[3] == 0xFF) && (buf[4] == 0xFF)) {
		ret = readSerial(buf + 5, 3);
		if (!ret || (((buf[5] + buf[6] + buf[7]) & 0xFF) != 0)) {
			return 0;
		}
		*responseLen = (((uint16_t) buf[5] << 8) | ((uint16_t) buf[6] << 0));
	} else {
		if (((buf[3] + buf[4]) & 0xFF) != 0) {
			return 0;
		}
		*responseLen = buf[3];
	}
	if (*responseLen > RCS620S_MAX_RW_RESPONSE_LEN) {
		return 0;
	}

	ret = readSerial(response, *responseLen);
	if (!ret) {
		RCS620S_cancel();
		return 0;
	}

	dcs = RCS620S_calcDCS(response, *responseLen);

	ret = readSerial(buf, 2);
	if (!ret || (buf[0] != dcs) || (buf[1] != 0x00)) {
		RCS620S_cancel();
		return 0;
	}

	return 1;
}

void RCS620S_cancel(void) {
	/* transmit an ACK */
	writeSerial((const uint8_t*) "\x00\x00\xff\x00\xff\x00", 6);
	HAL_Delay(1);
}

uint8_t RCS620S_calcDCS(const uint8_t *data, uint16_t len) {
	uint8_t sum = 0;
	uint16_t i;

	for (i = 0; i < len; i++) {
		sum += data[i];
	}

	return (uint8_t) -(sum & 0xFF);
}

void writeSerial(const uint8_t *buf, uint16_t bufLen) {
	uint8_t _buf[RCS620S_MAX_RW_RESPONSE_LEN];
	memcpy(_buf, buf, bufLen);

	HAL_UART_Transmit(&rcs620s_huart, _buf, bufLen, rcs620s_gv.timeout);
}

int readSerial(uint8_t *buf, uint16_t bufLen) {
	HAL_StatusTypeDef ret;
	ret = HAL_UART_Receive(&rcs620s_huart, buf, bufLen, rcs620s_gv.timeout);
	if (ret == HAL_OK) {
		return 1;
	}
	return 0;
}
