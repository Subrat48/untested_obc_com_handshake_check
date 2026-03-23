#include "obc_msn_logic.h"
#include <string.h>
#include <stdarg.h>

/* Private Variables */

static OBC_State_t current_state = STATE_INIT;
static Mission_Type_t current_mission = MSN_NONE;

// Internal Prototypes
uint8_t calc_crc8(uint8_t *data, uint8_t len);
void send_packet(UART_HandleTypeDef *huart, uint8_t cmd, uint8_t *data,
		uint8_t len);
int receive_packet(UART_HandleTypeDef *huart, OBC_Packet_t *pkt,
		uint32_t timeout);

void Turn_ON_COM_MCU(void);
void Restart_COM_MCU(void);
void Store_Status_In_Flash(uint32_t address, uint8_t status);
void Execute_Mission(Mission_Type_t mission);

/* ---------------- Debug Print Function ---------------- */
void OBC_Debug_Print(const char *format, ...) {
	char buffer[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	// Transmit string via FTDI UART
	HAL_UART_Transmit(DEBUG_UART, (uint8_t*) buffer, strlen(buffer), 100);
}

/* ---------------- Main Logic Processing ---------------- */

void OBC_Logic_Init(void) {
	OBC_Debug_Print("\r\n========================================\r\n");
	OBC_Debug_Print("[OBC] Booting Up... Initialization Started\r\n");

	// Start with MUX routing SPI to OBC so we can read the Flash ID safely
	MUX_Set_OBC_Mode();
	uint32_t flash_id = Read_Flash_ID();
	OBC_Debug_Print("[OBC] Shared Flash ID Read: 0x%06X\r\n",
			(unsigned int) flash_id);

	current_state = STATE_INIT;
}

void OBC_Logic_Process(void) {
	OBC_Packet_t rx_pkt;

	switch (current_state) {
	case STATE_INIT:
		OBC_Debug_Print("[OBC] Turning ON COM MCU Power...\r\n");
		Turn_ON_COM_MCU();
		current_state = STATE_HANDSHAKE_SEND;
		break;

	case STATE_HANDSHAKE_SEND:
		OBC_Debug_Print("[OBC] Sending Handshake (0x10) to COM...\r\n");
		// Send handshake command (No payload, so pass NULL and 0 length)
		send_packet(COM_UART, CMD_HANDSHAKE, NULL, 0);
		current_state = STATE_HANDSHAKE_WAIT;
		break;

	case STATE_HANDSHAKE_WAIT:
		// Wait up to (2 seconds) for a valid response packet from COM
		if (receive_packet(COM_UART, &rx_pkt, 2000)) {
			if (rx_pkt.cmd == CMD_HANDSHAKE) {
				OBC_Debug_Print("[OBC] Handshake OK! COM is alive.\r\n");
				current_state = STATE_WAIT_CMD;
			} else {
				OBC_Debug_Print(
						"[OBC] Unexpected packet during Handshake: 0x%02X\r\n",
						rx_pkt.cmd);
			}
		} else {
			// If 2 seconds pass with no valid packet, COM failed. Restart it.
			OBC_Debug_Print("[OBC] Handshake Timeout! Restarting COM...\r\n");
			Restart_COM_MCU();
			current_state = STATE_HANDSHAKE_SEND;
		}
		break;

	case STATE_WAIT_CMD:
		// Wait silently for 5 seconds. If nothing, loop and wait again.
		if (receive_packet(COM_UART, &rx_pkt, 5000)) {
			OBC_Debug_Print("[OBC] Packet RCV | CMD: 0x%02X | LEN: %d\r\n",
					rx_pkt.cmd, rx_pkt.len);

			// Check if command is CMD_MSN_RUN and has a payload containing the Mission ID
			if (rx_pkt.cmd == CMD_MSN_RUN && rx_pkt.len > 0) {
				OBC_Debug_Print(
						"[OBC] Mission RUN Command Received. Payload: %d\r\n",
						rx_pkt.data[0]);

				// Ground station sets data[0] to 1 for CAM, 2 for ADCS
				if (rx_pkt.data[0] == 1)
					current_mission = MSN_CAM;
				else if (rx_pkt.data[0] == 2)
					current_mission = MSN_ADCS;
				else
					current_mission = MSN_NONE;

				if (current_mission != MSN_NONE) {
					OBC_Debug_Print("[OBC] Mission Valid! Sending ACK...\r\n");
					send_packet(COM_UART, CMD_ACK, NULL, 0);

					// Choose Flash memory sector based on the mission
					uint32_t flash_addr =
							(current_mission == MSN_CAM) ?
									FLASH_ADDR_CAM_STATUS :
									FLASH_ADDR_ADCS_STATUS;
					// Write 0x01 to Shared Flash to indicate Mission Started
					Store_Status_In_Flash(flash_addr, 0x01);
					OBC_Debug_Print(
							"[OBC] Status 0x01 saved to Flash Address 0x%06X\r\n",
							(unsigned int) flash_addr);

					current_state = STATE_EXECUTE_MSN;
				} else {
					OBC_Debug_Print(
							"[OBC] Invalid Mission ID. Sending NACK.\r\n");
					send_packet(COM_UART, CMD_NACK, NULL, 0);
				}
			} else {
				OBC_Debug_Print("[OBC] Ignored Command. Sending NACK.\r\n");
				send_packet(COM_UART, CMD_NACK, NULL, 0);
			}
		}
		break;

	case STATE_EXECUTE_MSN:
		OBC_Debug_Print("[OBC] >>> STARTING MISSION EXECUTION <<<\r\n");
		Execute_Mission(current_mission);
		OBC_Debug_Print("[OBC] >>> MISSION EXECUTION FINISHED <<<\r\n");

		// Mission complete, write 0xFF to Shared Flash to indicate Finished
		uint32_t flash_addr =
				(current_mission == MSN_CAM) ?
						FLASH_ADDR_CAM_STATUS : FLASH_ADDR_ADCS_STATUS;
		Store_Status_In_Flash(flash_addr, 0xFF);
		OBC_Debug_Print("[OBC] Status 0xFF saved to Flash.\r\n");

		// Notify COM MCU so it can relay completion to Earth
		OBC_Debug_Print("[OBC] Sending Mission Done (0x32) to COM...\r\n");
		send_packet(COM_UART, CMD_MSN_DONE, NULL, 0);

		current_mission = MSN_NONE;
		current_state = STATE_WAIT_CMD;
		OBC_Debug_Print("[OBC] Returning to Wait State...\r\n");
		break;

	default:
		current_state = STATE_INIT;
		break;
	}
}

/* ---------------- Hardware Interactions ---------------- */

void Execute_Mission(Mission_Type_t mission) {
	// 1. ENABLE THE MAIN MISSION POWER (DC-DC)
	// Turning on the DC-DC converter turns on the Over-Current Protection (OCP)
	OBC_Debug_Print("      -> 3V3 DC-DC (OCP enabled)...\r\n");
	HAL_GPIO_WritePin(GPIOH, EN_3V3_2_DCDC_Pin, GPIO_PIN_SET);
	HAL_Delay(10); // Wait for DC-DC voltage to stabilize before applying load

	// 2. HANDOVER SPI FLASH TO THE MISSION PAYLOAD
	// MUX switches to B2: OBC disconnects, Payload connects to Flash
	OBC_Debug_Print(
			"      -> Switching MUX to B2: Payload to Flash Memory...\r\n");
	MUX_Set_MSN_Mode();
	HAL_Delay(1);

	// 3. ENABLE SPECIFIC MISSION LOAD SWITCHES
	// Power up the specific hardware (Camera or ADCS)
	switch (mission) {
	case MSN_CAM:
		OBC_Debug_Print("      -> Turning ON CAM Load Switch (GPIO1)...\r\n");
		HAL_GPIO_WritePin(GPIOE, GPIO1_Pin, GPIO_PIN_SET);
		break;

	case MSN_ADCS:
		OBC_Debug_Print("      -> Turning ON ADCS Load Switch (GPIO3)...\r\n");
		HAL_GPIO_WritePin(GPIO3_GPIO_Port, GPIO3_Pin, GPIO_PIN_SET);
		break;

	default:
		OBC_Debug_Print("      -> ERROR: Unknown Mission! Aborting.\r\n"); //close dc-dc
		MUX_Set_OBC_Mode(); // To MUX before aborting
		HAL_GPIO_WritePin(GPIOH, EN_3V3_2_DCDC_Pin, GPIO_PIN_RESET);
		return;
	}

	// 4. WAIT FOR MISSION TO COMPLETE
	// During this 30s block, the payload dumps large data directly to Flash via MUX
	OBC_Debug_Print("      -> Running for 30 seconds...\r\n");
	HAL_Delay(30000);

	// 5. DISABLE SPECIFIC MISSION LOAD SWITCHES
	OBC_Debug_Print("      -> Mission Time Up. Disabling Load Switch...\r\n");
	switch (mission) {
	case MSN_CAM:
		HAL_GPIO_WritePin(GPIOE, GPIO1_Pin, GPIO_PIN_RESET);
		break;

	case MSN_ADCS:
		HAL_GPIO_WritePin(GPIO3_GPIO_Port, GPIO3_Pin, GPIO_PIN_RESET);
		break;

	default:
		break;
	}

	// 6. TAKE FLASH MEMORY BACK TO THE OBC
	// MUX switches to B1: OBC regains control of SPI lines to write the completion status
	OBC_Debug_Print(
			"      -> Switching MUX to B1: OBC reclaiming Flash Memory...\r\n");
	MUX_Set_OBC_Mode();
	HAL_Delay(1);

	// 7. DISABLE MAIN MISSION POWER
	OBC_Debug_Print("      -> Disabling 3V3 DC-DC Converter...\r\n");
	HAL_GPIO_WritePin(GPIOH, EN_3V3_2_DCDC_Pin, GPIO_PIN_RESET);
}

void Turn_ON_COM_MCU(void) {
	// Drives the DTC144EKA NPN Transistor to power the COM MCU
	HAL_GPIO_WritePin(GPIOH, GPIO_PIN_11, GPIO_PIN_SET);
	HAL_Delay(500); // Wait for COM MCU to fully boot
}

void Restart_COM_MCU(void) {
	// Hard reset line to COM MCU
	HAL_GPIO_WritePin(EN_GLOBAL_RESET_GPIO_Port, EN_GLOBAL_RESET_Pin,
			GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(EN_GLOBAL_RESET_GPIO_Port, EN_GLOBAL_RESET_Pin,
			GPIO_PIN_SET);
	HAL_Delay(500); // Time to boot before sending next handshake
}

/* ---------------- MUX & Flash Control ---------------- */

void MUX_Set_OBC_Mode(void) {
	// S=0 (LOW) connects A pins to B1 pins (OBC SPI)
	HAL_GPIO_WritePin(MUX_EN_GPIO_Port, MUX_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(MSN_FM_MODE_GPIO_Port, MSN_FM_MODE_Pin, GPIO_PIN_RESET);
}

void MUX_Set_MSN_Mode(void) {
	// S=1 (HIGH) connects A pins to B2 pins (Mission Payload SPI)
	HAL_GPIO_WritePin(MSN_FM_MODE_GPIO_Port, MSN_FM_MODE_Pin, GPIO_PIN_SET);
}

uint32_t Read_Flash_ID(void) {
	uint8_t cmd = FLASH_CMD_JEDEC; // 0x9F command
	uint8_t id_data[3] = { 0 };
	uint32_t flash_id = 0;

	MUX_Set_OBC_Mode();
	HAL_Delay(1);

	HAL_GPIO_WritePin(SMSN_FM_CS_OBC_GPIO_Port, SMSN_FM_CS_OBC_Pin,
			GPIO_PIN_RESET);
	HAL_SPI_Transmit(FLASH_SPI, &cmd, 1, 100);
	HAL_SPI_Receive(FLASH_SPI, id_data, 3, 100);
	HAL_GPIO_WritePin(SMSN_FM_CS_OBC_GPIO_Port, SMSN_FM_CS_OBC_Pin,
			GPIO_PIN_SET);

	flash_id = (id_data[0] << 16) | (id_data[1] << 8) | id_data[2];
	return flash_id;
}

void Store_Status_In_Flash(uint32_t address, uint8_t status) {
	MUX_Set_OBC_Mode();
	HAL_Delay(1);

	// Send Write Enable Command (0x06) - Required to unlock flash latches
	uint8_t wren_cmd = FLASH_CMD_WREN;
	HAL_GPIO_WritePin(SMSN_FM_CS_OBC_GPIO_Port, SMSN_FM_CS_OBC_Pin,
			GPIO_PIN_RESET);
	HAL_SPI_Transmit(FLASH_SPI, &wren_cmd, 1, 100);
	HAL_GPIO_WritePin(SMSN_FM_CS_OBC_GPIO_Port, SMSN_FM_CS_OBC_Pin,
			GPIO_PIN_SET);
	HAL_Delay(1);

	// Send Page Program Command (0x02) followed by 24-bit address and 8-bit data
	uint8_t tx_buf[5] = { FLASH_CMD_WRITE, (address >> 16) & 0xFF,
			(address >> 8) & 0xFF, address & 0xFF, status };
	HAL_GPIO_WritePin(SMSN_FM_CS_OBC_GPIO_Port, SMSN_FM_CS_OBC_Pin,
			GPIO_PIN_RESET);
	HAL_SPI_Transmit(FLASH_SPI, tx_buf, 5, 1000);
	HAL_GPIO_WritePin(SMSN_FM_CS_OBC_GPIO_Port, SMSN_FM_CS_OBC_Pin,
			GPIO_PIN_SET);
	HAL_Delay(5);
}

/* ---------------- Protocol / Packet Functions ---------------- */

uint8_t calc_crc8(uint8_t *data, uint8_t len) {
	// Calculates a unique 1-byte "signature" based on the packet data
	uint8_t crc = 0;
	for (uint8_t i = 0; i < len; i++) {
		crc ^= data[i]; // XOR operation over every byte
	}
	return crc;
}

void send_packet(UART_HandleTypeDef *huart, uint8_t cmd, uint8_t *data,
		uint8_t len) {
	// Create a temporary 64-byte array in memory (the "box" to pack bytes into)
	uint8_t buffer[64];
	// Counter to keep track of current position in the buffer
	uint8_t index = 0;

	// Add Header 1 (0xAA) and Header 2 (0x55) - acts as a "Wake Up!" signal for COM
	buffer[index++] = HEADER1;
	buffer[index++] = HEADER2;
	// Insert the command byte
	buffer[index++] = cmd;
	// Specify exactly how many payload bytes will follow
	buffer[index++] = len;

	// Copy the actual mission payload into the buffer byte by byte
	for (uint8_t i = 0; i < len; i++)
		buffer[index++] = data[i];

	// Calculate the CRC8 signature over the Headers, Command, Length, and Payload
	uint8_t crc = calc_crc8(buffer, index);
	// Append the signature byte to the very end of the packet
	buffer[index++] = crc;

	// Fire the entire assembled buffer out via standard UART hardware transmission
	HAL_UART_Transmit(huart, buffer, index, HAL_MAX_DELAY);
}

int receive_packet(UART_HandleTypeDef *huart, OBC_Packet_t *pkt,
		uint32_t timeout) {
	uint8_t header[4];

	// Attempt to receive 4 bytes. If it fails or times out, return 0 (fail)
	if (HAL_UART_Receive(huart, header, 4, timeout) != HAL_OK)
		return 0;
	// Verify the "Wake Up" headers match 0xAA and 0x55
	if (header[0] != HEADER1 || header[1] != HEADER2)
		return 0;

	// Extract command and length
	pkt->cmd = header[2];
	pkt->len = header[3];

	// Prevent buffer overflows (security best practice)
	if (pkt->len > MAX_DATA)
		return 0;

	// Fetch the payload if length > 0
	if (pkt->len > 0) {
		if (HAL_UART_Receive(huart, pkt->data, pkt->len, timeout) != HAL_OK)
			return 0;
	}

	// Fetch the single trailing CRC byte
	uint8_t crc;
	if (HAL_UART_Receive(huart, &crc, 1, timeout) != HAL_OK)
		return 0;

	// Reconstruct the raw array locally to verify the CRC signature
	uint8_t check_buffer[64];
	memcpy(check_buffer, header, 4);
	if (pkt->len > 0)
		memcpy(&check_buffer[4], pkt->data, pkt->len);

	// Calculate CRC of received data. If it doesn't match the received CRC, packet is corrupted.
	uint8_t expected_crc = calc_crc8(check_buffer, 4 + pkt->len);
	if (expected_crc != crc)
		return 0;

	return 1; // Packet valid!
}
