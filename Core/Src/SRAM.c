
#include "SRAM.h"

// Буферы для данных
uint8_t data_to_write[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
uint8_t read_buffer[8] = 	{0x01, 0x00, 0x00, 0x00, 0x05};

// Адрес в памяти FM24, куда будем писать (0...8191)
uint16_t memory_address = 0;

I2C_HandleTypeDef* sramhI2c2_;

uint16_t countReadMemory_ = 0;
uint8_t read_buffer_origin[8];
uint8_t sramWork_ = SRAM_WORK_OK;
uint8_t SRAM_FM24CL64B_ADDR = FM24CL64B_WRITE_ADDR;

// Проверка и установка статуса
void SRAMVerifyWork()
{
	sramWork_ = SRAM_WORK_FAILURE;
	if (HAL_I2C_IsDeviceReady(backlightHi2c2, SRAM_FM24CL64B_ADDR, 2, 10) == HAL_OK)
	{
		sramWork_ = SRAM_WORK_OK;
	}
}

// Инициализация модуля при старте
void SRAMInit(I2C_HandleTypeDef* hi2c2)
{
	sramhI2c2_ = hi2c2;
	SRAMVerifyWork();
	if(sramWork_ == SRAM_WORK_FAILURE)
	{
		return;
	}
}

void SRAMWriteBlock(uint16_t address, uint8_t* cData)
{
	// 1. Запись данных
	HAL_StatusTypeDef status = HAL_ERROR;
	status = HAL_I2C_Mem_Write(sramhI2c2_,
	                           FM24CL64B_WRITE_ADDR,
	                           address,
							   I2C_MEMADD_SIZE_16BIT,
	                           cData,
	                           sizeof(cData),
	                           HAL_MAX_DELAY);

	if (status == HAL_OK)
	{
	    // Запись выполнена без задержек (NoDelay)
	    // Можно сразу приступать к чтению
	}
}

uint8_t* SRAMReadBlock(uint16_t address, uint8_t size)
{
	HAL_StatusTypeDef status = HAL_ERROR;
	uint8_t read_buffer[size];
	for(int i=0; i<size; i++)
	{
		read_buffer[i] = 0;
	}
	status = HAL_I2C_Mem_Read(sramhI2c2_,
							  FM24CL64B_READ_ADDR /* FM24CL64B_WRITE_ADDR */,
							  address,
							  I2C_MEMADD_SIZE_16BIT,
							  read_buffer,
							  size,
							  HAL_MAX_DELAY);

	if (status == HAL_OK)
	{
		//
	}
	return &read_buffer;
}

uint8_t* SRAMGetBuffer(void)
{
	return &read_buffer;
}

void SRAMSetBuffer(uint8_t data[])
{
	for(int i=0; i<8; i++)
	{
		data_to_write[i] = data[i];
	}
}

// Получить время работы устройства
uint16_t SRAMGetWorkTimeSRAM()
{
	uint8_t* buffer = SRAMReadBlock(SRAM_TIME_ADDRESS, 2);
	// Вариант "Little-Endian" (младший байт первым)
	uint16_t wtime = buffer[0] | (buffer[1] << 8);
	return wtime;
}

// Записать время работы устройства
void SRAMSetWorkTimeSRAM(uint16_t wtime)
{
	uint8_t buffer[2];

	// Вариант "Little-Endian" (младший байт первым) — стандарт для STM32
	buffer[0] = wtime & 0xFF;        // 0x34
	buffer[1] = (wtime >> 8) & 0xFF; // 0x12

	SRAMWriteBlock(SRAM_TIME_ADDRESS, &buffer);
}

// Получить версию прошивки
uint32_t SRAMGetSWVersion()
{
	uint8_t* buffer = SRAMReadBlock(SRAM_VERSION_ADDRESS, 4);
	// Вариант "Little-Endian" (младший байт первым)
	uint32_t version = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
	return version;
}

// Записать версию прошивки
void SRAMSetSWVersion(uint32_t version)
{
	uint8_t buffer[4];

	// Вариант "Little-Endian" (младший байт первым) — стандарт для STM32
	buffer[0] = version & 0xFF;        	// 0x78
	buffer[1] = (version >> 8) & 0xFF; 	// 0x56
	buffer[2] = (version >> 16) & 0xFF; 	// 0x34
	buffer[3] = (version >> 24) & 0xFF; 	// 0x12

	SRAMWriteBlock(SRAM_VERSION_ADDRESS, &buffer);
}

// Получить CRC16 прошивки
uint16_t SRAMGetSWCheckSum(void)
{
	uint8_t* buffer = SRAMReadBlock(SRAM_CHECKSUM_ADDRESS, 2);
	// Вариант "Little-Endian" (младший байт первым)
	uint16_t crc16 = buffer[0] | (buffer[1] << 8);
	return crc16;
}

// Записать CRC16 прошивки
void SRAMSetSWCheckSum(uint16_t crc16)
{
	uint8_t buffer[2];

	// Вариант "Little-Endian" (младший байт первым) — стандарт для STM32
	buffer[0] = crc16 & 0xFF;        // 0x34
	buffer[1] = (crc16 >> 8) & 0xFF; // 0x12

	SRAMWriteBlock(SRAM_CHECKSUM_ADDRESS, &buffer);
}
