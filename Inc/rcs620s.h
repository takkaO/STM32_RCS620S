#ifndef RCS620S_H
#define RCS620S_H

#include "string.h"
#include "usart.h"

/* --------------------------------
 * Constant
 * -------------------------------- */
// RCS620S
#define RCS620S_MAX_CARD_RESPONSE_LEN    254
#define RCS620S_MAX_RW_RESPONSE_LEN      265

// FeliCa Service/System code
#define FELICA_SYSTEM_CODE		0xFFFF
#define CYBERNE_SYSTEM_CODE     0x0003
#define PASSNET_SERVICE_CODE    0x090F

/* --------------------------------
 * Global variables
 * -------------------------------- */
typedef struct {
	uint32_t timeout;
	uint8_t idm[8];
	uint8_t pmm[8];
} RCS620S_Global_Variables;
extern RCS620S_Global_Variables rcs620s_gv;

/* --------------------------------
 * Public prototypes
 * -------------------------------- */
int RCS620S_initDevice(void);
int RCS620S_polling(uint16_t systemCode);
int RCS620S_cardCommand(const uint8_t *command, uint8_t commandLen,
		uint8_t response[RCS620S_MAX_CARD_RESPONSE_LEN], uint8_t *responseLen);
int RCS620S_rfOff(void);
int RCS620S_push(const uint8_t *data, uint8_t dataLen);
int RCS620S_requestService(uint16_t serviceCode);
int RCS620S_readEncryption(uint16_t serviceCode, uint8_t blockNumber, uint8_t *buf);
#endif
