#ifndef OBC_MSN_LOGIC_H
#define OBC_MSN_LOGIC_H

#include "main.h"
#include <stdio.h>

/* ---------------- Peripheral Config ---------------- */
extern UART_HandleTypeDef huart1; // OBC UART connected to COM MCU
extern SPI_HandleTypeDef hspi4;   // Shared Flash Memory
extern UART_HandleTypeDef huart7; // FTDI Debug Console

#define COM_UART &huart1
#define FLASH_SPI &hspi4
#define DEBUG_UART &huart7

/* ---------------- Flash Memory Config (MT25QL01G) ---------------- */
#define FLASH_ADDR_CAM_STATUS  0x000000
#define FLASH_ADDR_ADCS_STATUS 0x010000

#define FLASH_CMD_WREN  0x06 // Write Enable
#define FLASH_CMD_WRITE 0x02 // Page Program
#define FLASH_CMD_JEDEC 0x9F // Read ID

/* ---------------- Protocol Defines (From COM) ---------------- */
#define HEADER1 0xAA
#define HEADER2 0x55
#define MAX_DATA 32

#define CMD_HANDSHAKE   0x10
#define CMD_MSN_RUN     0x20
#define CMD_ACK         0x30
#define CMD_NACK        0x31
#define CMD_MSN_DONE    0x32

/* ---------------- Enums ---------------- */
typedef enum {
	STATE_INIT,
	STATE_HANDSHAKE_SEND,
	STATE_HANDSHAKE_WAIT,
	STATE_WAIT_CMD,
	STATE_EXECUTE_MSN,
	STATE_ERROR
} OBC_State_t;

typedef enum {
	MSN_NONE = 0, MSN_CAM = 1, MSN_ADCS = 2
} Mission_Type_t;

// Packet Structure
typedef struct {
	uint8_t cmd;
	uint8_t len;
	uint8_t data[MAX_DATA];
} OBC_Packet_t;

/* ---------------- Function Prototypes ---------------- */
void OBC_Logic_Init(void);
void OBC_Logic_Process(void);
void OBC_Debug_Print(const char *format, ...);

// MUX & Flash hardware functions
void MUX_Set_OBC_Mode(void);
void MUX_Set_MSN_Mode(void);
uint32_t Read_Flash_ID(void);

#endif /* OBC_MSN_LOGIC_H */
