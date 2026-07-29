
/*
 * Сделано:
 * 1. set_map() – отправка 0x98 + 2 байта адреса.
 * 2. write_byte_to_map() – команда 0x84. Используется для записи байта по текущему MAP.
 * 3. read_byte_from_map() – команда 0x80. Возвращает байт и инкрементирует MAP.
 * 4. write_reg() / read_reg() – косвенный доступ к любому регистру через set_map + write/read.
 * 5. TRANSFER_Init() – сохраняет SPI и пины, инициализирует GPIO для CS, MRST, RUN. Всё правильно.
 * 6. TRANSFER_ResetAndWait() – формирует импульс на MRST (HIGH → LOW), затем читает MSR до установки бита READY (бит 7). Соответствует даташиту.
 * 7. TRANSFER_ConfigMaster() – запись в MCR с использованием битовых масок.
 * 8. TRANSFER_ConfigRxChannel() – сборка байта по структуре и запись в ARXCn.
 * 9. TRANSFER_ConfigTxChannel() – аналогично для ATXCn.
 * 10. TRANSFER_SetFifoThreshold() – запись в FTV (0x8050).
 * 11. TRANSFER_Start() / Stop() – управление пином RUN.
 * 12. TRANSFER_ReadReceivedWord() – вычисление адреса по каналу/метке, установка MAP, посылка 0x80 и чтение 4 байт. После 0x80 MAP инкрементируется, и чтение подряд даёт статус+3 байта.
 * 13. TRANSFER_SendImmediate() – команда 0x94 + channel и 4 байта данных. Соответствует 100101TT.
 * 14. TRANSFER_ReadLastReceived() – команда 0xC0 | (channel<<2) и затем чтение 4 байт.
 * Согласно таблице 1, для этой команды количество данных = 4, auto-increment = No.
 * Это значит, что после отправки опкода нужно просто прочитать 4 байта (без дополнительной 0x80).
 * 15. TRANSFER_SetLoopback() - Управление loopback (для тестов и отладки)
 */

#include "transfer_holt.h"

/*
 * brief
 * Хранят указатели на SPI и пины, которые передаются при инициализации.
 */
/* hspi_inst – указатель на структуру HAL SPI (например, &hspi3). Позволяет вызывать HAL_SPI_Transmit/Receive.*/
/*cs_port, cs_pin – порт и номер пина для Chip Select (CS) HI-3220. CS активен низким уровнем.*/
/*mrst_port, mrst_pin – пин для сброса (Master Reset) HI-3220. Активный высокий импульс.*/
/*run_port, run_pin – пин для входа RUN HI-3210. Когда на нём высокий уровень – чип работает, когда низкий – стоит в Idle.*/
static SPI_HandleTypeDef *hspi_inst;
static GPIO_TypeDef *cs_port, *mrst_port, *run_port;
static uint16_t cs_pin, mrst_pin, run_pin;

/*
 * Команда	Описание
 * 0x98		Write MAP (запись адреса в Memory Address Pointer)
 * 0x90		Read MAP (чтение текущего значения MAP)
 * 0x80		Read memory at address MAP (чтение байта по текущему MAP)
 * 0x88		Write memory at address MAP (запись байта по текущему MAP)
 */

// ---------- Низкоуровневый доступ через MAP ----------
/*
 * brief
 * Загрузить 16-битный адрес в MAP.
 * Использует команду 0x8C + два байта, т.к. это единственный способ установить указатель для косвенного доступа
 *
 * Команда 0x98. Это SPI-команда «Write MAP». После неё идут два байта данных – адрес.

 * Зачем нужна? HI-3220 не имеет обычной адресной шины.
 * Чтобы указать, с какой ячейкой памяти работать, нужно загрузить 16-битный адрес во внутренний регистр MAP (Memory Address Pointer).
 *
 * (addr >> 8) & 0xFF и addr & 0xFF? Адрес 16-битный, SPI передаёт байты первым старший (big-endian порядок, как требует HI-3220).
 */
static void set_map(uint16_t addr)
{
    /*
     * Адрес в HI-3220 — 16-битный. Это значит, что он может принимать значения от 0x0000 до 0xFFFF.
     * По SPI мы можем передавать только байты (по 8 бит). Поэтому 16 бит нужно разбить на два байта:
     * Старший байт — содержит биты 15..8 адреса.
     * Младший байт — содержит биты 7..0 адреса.
     */

    /*
     * Это единственный способ сообщить HI-3220, по какому адресу вы хотите читать или писать.
     * Без этого шага чип не знал бы, к какой ячейке обращаться, потому что у него нет отдельной адресной шины — всё идёт через один SPI.
     */

	/* 0x98 — это специальная SPI-команда, которая говорит HI-3220: «Следующие два байта — это адрес, загрузи его во мой внутренний указатель MAP».
	Без этой команды HI-3220 не поймёт, что передаётся адрес. Можно было бы просто отправить два байта адреса, но чип не знал бы, что с ними делать. Поэтому сначала идёт команда-префикс.*/

    uint8_t cmd = 0x98; // Write MAP (HI-3220)
    uint8_t msb = (addr >> 8) & 0xFF;
    uint8_t lsb = addr & 0xFF;

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_inst, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi_inst, &msb, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi_inst, &lsb, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

/*
 * brief
 * Записать байт по текущему MAP.
 *
 * После того как MAP установлен, эта функция пишет один байт в ту память, на которую указывает MAP.
 * Команда 0x88 – «write location addressed by pointer value» .
 *
 * Автоинкремент: после записи байта MAP сам увеличивается на 1,
 * поэтому можно записывать последовательно несколько байтов без повторной отправки команды.
 *
 * Почему отдельно передаётся команда, а потом данные?
 * Потому что SPI — дуплекс, но в данном случае мы просто последовательно отправляем два байта: опкод и данные.
 */
static void write_byte_to_map(uint8_t data)
{
    uint8_t cmd = 0x88; // Write memory at current MAP (HI-3220)
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_inst, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi_inst, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

/*
 * brief
 * Прочитать байт по текущему MAP.
 *
 * Команда 0x80 («Read memory at address MAP») – после отправки команды, на следующем такте SPI,
 * HI-3220 выставляет на вывод SO значение байта из текущей ячейки MAP, а затем увеличивает MAP.
 *
 * Важно: команда чтения не требует дополнительных байтов адреса – адрес уже сидит в MAP.
 */
static uint8_t read_byte_from_map(void)
{
	/*
	 * 0x80 – это не регистр, а SPI команда «Read memory at address MAP».
	 * Она не имеет отношения к регистру MCR (0x800F). При её отправке HI-3220 выставляет на вывод SO содержимое ячейки памяти,
	 * на которую указывает MAP, а затем увеличивает MAP на 1.
	 */
    uint8_t cmd = 0x80;           // Read memory at current MAP
    uint8_t data;
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_inst, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(hspi_inst, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
    return data;
}


/*
 * brief
 * Запись регистра (адрес 16 бит)
 *
 * Это обёртки для доступа к любому регистру (0x8000…0x807F) или к любой ячейке памяти (0x0000…0x7FFF).
 * Они скрывают двухшаговость: сначала установить MAP, потом прочитать/записать байт.
 */
static void write_reg(uint16_t addr, uint8_t data)
{
    set_map(addr);
    write_byte_to_map(data);
}

/*
 * brief
 * Чтение регистра
 *
 * Это обёртки для доступа к любому регистру (0x8000…0x807F) или к любой ячейке памяти (0x0000…0x7FFF).
 * Они скрывают двухшаговость: сначала установить MAP, потом прочитать/записать байт.
 */
static uint8_t read_reg(uint16_t addr)
{
    set_map(addr);
    return read_byte_from_map();
}

// ---------- Публичные функции ----------
/*
 * brief
 * Description:
 * Сохранить SPI и пины, настроить GPIO для CS, MRST, RUN
 * - Сохраняет переданные параметры в статические переменные.
 * - Настраивает пины CS, MRST, RUN как push-pull outputs.
 * - CS инициализируется в HIGH (неактивный уровень).
 * - RUN инициализируется в LOW (HI-3210 в Idle после старта).
 * - MRST не инициализируется значением по умолчанию, его состояние будет установлено позже в TRANSFER_ResetAndWait.
 *
 * run_port - это пин, который подаётся на вход RUN HI-3210. Когда он HIGH – чип начинает приём/передачу. Когда LOW – останавливается (Idle).
 *
 * Args:
 * - hspi – указатель на SPI, например &hspi3.
 * - cs_port_, cs_pin_ – порт и пин для CS.
 * - mrst_port_, mrst_pin_ – для сброса.
 * - run_port_, run_pin_ – для RUN.
 *
 * ----- Простыми словами -----
 * TRANSFER_Init выполняет низкоуровневую привязку драйвера к конкретному аппаратному обеспечению микроконтроллера (STM32).
 * - Запоминает (сохраняет в статических переменных), через какой SPI, какие пины и порты нужно управлять HI-3220.
 * - Настраивает эти пины как выходы (чтобы мы могли дёргать CS, MRST, RUN).
 * - Устанавливает начальные состояния пинов (CS = высокий, RUN = низкий), чтобы HI-3220 находился в известном состоянии до вызова TRANSFER_ResetAndWait.
 *
 * Без этого драйвер не знал бы, куда подключать CS, какую ножку SPI использовать и т.д.
 * Это как сказать: «Вот SPI3, вот пины PB0, PB1, PC13 – работай с ними».
 */
void TRANSFER_Init(SPI_HandleTypeDef *hspi,
                   GPIO_TypeDef *cs_port_, uint16_t cs_pin_,
                   GPIO_TypeDef *mrst_port_, uint16_t mrst_pin_,
                   GPIO_TypeDef *run_port_, uint16_t run_pin_)
{
    hspi_inst = hspi;
    cs_port = cs_port_;
    cs_pin = cs_pin_;
    mrst_port = mrst_port_;
    mrst_pin = mrst_pin_;
    run_port = run_port_;
    run_pin = run_pin_;

    /*
     * Создаём структуру инициализации GPIO:
     * - OUTPUT_PP – выход push-pull (может и высокий, и низкий уровень).
     * - NOPULL – без внутренних подтяжек.
     * - LOW – невысокая скорость переключения (вполне достаточно для CS, MRST, RUN).
     */

    // Инициализация пинов как выходов
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    gpio_init.Pin = cs_pin;
    HAL_GPIO_Init(cs_port, &gpio_init);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    /*
     * Если передан ненулевой порт для MRST (т.е. мы хотим управлять сбросом), настраиваем этот пин как выход.
     * Значение не задаём – оно будет установлено позже в TRANSFER_ResetAndWait (там сначала HIGH, потом LOW).
     */
    if (mrst_port) {
        gpio_init.Pin = mrst_pin;
        HAL_GPIO_Init(mrst_port, &gpio_init);
    }

    /*
     * Если задан пин RUN, настраиваем его как выход и сразу пишем LOW.
     * Это важно, потому что после сброса HI-3210 переходит в состояние Idle, и мы сами должны перевести RUN в HIGH, когда захотим запустить приём-передачу (вызовом TRANSFER_Start).
     * LOW гарантирует, что чип не начнёт работу раньше времени.
     */
    if (run_port) {
        gpio_init.Pin = run_pin;
        HAL_GPIO_Init(run_port, &gpio_init);
        HAL_GPIO_WritePin(run_port, run_pin, GPIO_PIN_RESET);
    }

    /*
     * Без вызова TRANSFER_Init драйвер не будет знать, как управлять чипом, и последующие вызовы (например, TRANSFER_ConfigMaster) либо не скомпилируются, либо будут использовать неинициализированные указатели (что приведёт к зависанию).
     * Поэтому TRANSFER_Init – это обязательный первый шаг перед любой работой с HI-3220.
     */
}

/*
 * brief
 * Назначение:
 * Выполняет аппаратный сброс чипа (импульс на MRST) и ожидает перехода в состояние READY (бит в регистре MSR).
 * Зачем нужна:
 * После включения питания или в случае сбоя чип должен быть инициализирован. Без этой функции регистры и память могут находиться в неопределённом состоянии.
 * Обязательна для вызова сразу после TRANSFER_Init().
 */
void TRANSFER_ResetAndWait(void)
{
    if (mrst_port) {
        HAL_GPIO_WritePin(mrst_port, mrst_pin, GPIO_PIN_SET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(mrst_port, mrst_pin, GPIO_PIN_RESET);
        HAL_Delay(10);
    }
    while (!(read_reg(HI3220_MSR) & HI3220_MSR_READY)) {
        HAL_Delay(1);
    }
}

/*
 * brief
 * Установить глобальные биты (RX, TX, переворот меток).
 * Прямая запись в MCR.
 *
 * Записывает в Master Control Register три важнейших глобальных разрешения:
 * - A429RX – включить приёмники.
 * - A429TX – включить передатчики.
 * - AFLIP – автоматически переворачивать метки (битовый порядок). Удобно, если работаем с метками как числами 0..255, а не с битовым потоком.
 *
 * AFLIP – если установлен, то HI-3210 автоматически переворачивает порядок битов в метке (label) при приёме и перед передачей.
 * Это удобно, потому что на шине ARINC 429 биты идут в порядке от старшего к младшему, а в памяти микроконтроллера обычно хранят метку как обычный байт.
 * Включив AFLIP, мы избавляемся от ручного битового реверса.
 */
void TRANSFER_ConfigMaster(uint8_t enable_rx, uint8_t flip_labels, uint8_t enable_tx)
{
    uint8_t val = 0;
    if (enable_rx) val |= HI3220_MCR_A429RX;
    if (flip_labels) val |= HI3220_MCR_FLIP;
    if (enable_tx) val |= HI3220_MCR_A429TX;
    write_reg(HI3220_MCR_WR, val);
}

/*
 * brief
 * Настроить один приёмный канал.
 * Регистр ARXC имеет чёткую битовую структуру. Функция собирает байт из полей структуры.
 *
 * Каждый канал приёма имеет свой регистр ARXC0..ARXC7 (0x8010+).
 * Биты (12 стр. даташита):
 * - бит 7 = Enable
 * - бит 6 = скорость: 0=100k, 1=12.5k
 * - бит 5 = Parity enable
 * - бит 4 = Decoder enable (проверка SDI9/10)
 * - биты 3,2 = SDI10, SDI9
 * - биты 1-0 = условие флага FIFO (00=никогда, 01=не пусто, 10=выше порога, 11=полное)
 *
 * Функция формирует байт и записывает его.
 */
void TRANSFER_ConfigRxChannel(uint8_t channel, const HI3220_RxConfig *cfg)
{
    if (channel > 15) return;
    uint16_t addr = HI3220_RXC0 + channel;
    uint8_t reg = 0;
    if (cfg->enable)      reg |= (1 << 7);
    if (cfg->speed)       reg |= (1 << 6);
    if (cfg->parity_en)   reg |= (1 << 5);
    if (cfg->decoder_en)  reg |= (1 << 4);
    if (cfg->sdi10)       reg |= (1 << 3);
    if (cfg->sdi9)        reg |= (1 << 2);
    reg |= (cfg->fifo_flag_cond & 0x03);
    write_reg(addr, reg);
}

/*
 * brief
 * Настроить один передающий канал.
 * Аналогично ATXC
 *
 * Регистры ATXC0..3 (0x8018+).
 * Биты:
 * - бит 7 = run/stop планировщика (0 – остановлен, 1 – запущен циклический вывод).
 * - бит 6 = скорость передачи.
 * - бит 5 = parity enable (если 1, то 32-й бит заменяется на паритет).
 * - бит 4 = even/odd parity.
 * - бит 3 = skip (как вести себя при окончании цикла).
 *
 * Важно: для immediate передачи (команда 0x94) планировщик можно не запускать; достаточно run_stop = 0. Передатчик всё равно сможет отправить слово по команде.
 */
void TRANSFER_ConfigTxChannel(uint8_t channel, const HI3220_TxConfig *cfg)
{
    if (channel > 7) return;
    uint16_t addr = HI3220_TXC0 + channel;
    uint8_t reg = 0;
    if (cfg->run_stop)    reg |= (1 << 7);
    if (cfg->prescale)    reg |= (1 << 6);
    if (cfg->parity_en)   reg |= (1 << 5);
    if (cfg->even_odd)    reg |= (1 << 4);
    if (cfg->skip)        reg |= (1 << 3);
    if (cfg->tristate)    reg |= (1 << 2);
    if (cfg->speed)       reg |= (1 << 1);   // RATE
    if (cfg->opt50k)      reg |= (1 << 0);   // 50KOPT
    write_reg(addr, reg);
}

/*
 * brief
 * Установить порог FIFO для флага.
 * FTV принимает значение 0-31, пишется напрямую.
 *
 * FTV (0x8050) – значение порога для FIFO (0..31). Когда количество сообщений в FIFO превысит этот порог, устанавливается флаг THRESHOLD.
 *
 * Назначение:
 * Устанавливает пороговое значение для FIFO каждого приёмного канала. Когда количество сообщений в FIFO превышает этот порог, устанавливается флаг (и может быть сгенерировано прерывание).
 * Зачем нужна:
 * Если используется FIFO для накопления сообщений, эта функция позволяет настроить момент оповещения (например, когда накопилось 10 слов).
 */
void TRANSFER_SetFifoThreshold(uint8_t channel, uint8_t threshold)
{
    if (channel > 15) return;
    if (threshold > 63) threshold = 63;   // в HI-3220 порог 0..63 (6 бит)
    uint16_t addr = HI3220_FTV(channel);
    write_reg(addr, threshold);
}

/*
 * brief
 *
 * Start&Stop
 * Назначение:
 * Управляют входом RUN. При HIGH чип переходит в активный режим (приём/передача), при LOW – в состояние Idle (остановка всех шин).
 * Зачем нужны:
 * Позволяют программно запускать и останавливать обмен данными. Без вызова Start() чип не будет обрабатывать шины, даже если MCR настроен. Это ключевое управление рабочим циклом.
 */
void TRANSFER_Start(void)
{
    if (run_port) {
        HAL_GPIO_WritePin(run_port, run_pin, GPIO_PIN_SET);
    }
}

void TRANSFER_Stop(void)
{
    if (run_port) {
        HAL_GPIO_WritePin(run_port, run_pin, GPIO_PIN_RESET);
    }
}

/*
 * brief
 *
 * Каждый приёмный канал имеет:
 * Receive FIFO Threshold Flag - регистры FTFL (0x800D) и FTFH (0x800E). Каждый бит = 1,
 * если в соответствующем FIFO накопилось сообщений больше, чем задано в пороге.
 * Receive FIFO Count - регистры RFC0…RFC15 (0x8068…0x8077).
 * Чтение даёт текущее количество слов в FIFO. Запись 0xA5 в этот регистр очищает FIFO (стр. 21).
*/
void TRANSFER_PollRxFifos(void (*callback)(uint8_t, const uint8_t*), uint8_t max_read)
{
	/*Прочитать флаги порога (какие FIFO не пусты)*/
	uint8_t ftfl = read_reg(HI3220_FTFL);
	uint8_t ftfh = read_reg(HI3220_FTFH);

	/*Для каждого канала, у которого установлен бит*/
	for (int ch = 0; ch < 16; ch++)
	{
		uint8_t bit = (ch < 8) ? (ftfl >> ch) & 1 : (ftfh >> (ch-8)) & 1;
		if (!bit) continue;

		/*Узнать сколько слов в FIFO*/
		uint16_t addr_rfc = HI3220_RFC(ch);
		uint8_t count = read_reg(addr_rfc);

		uint8_t size = 0;
		if (count < max_read)
		{
			size = count;
		} else
		{
			size = max_read;
		}

		/*Прочитать все слова из FIFO (каждое - 4 байта)*/
		for (int i = 0; i < size; i++)
		{
			uint8_t word[4];
			TRANSFER_ReadFifoWord(ch, word);
			if (callback) // TODO: callback function
			{
				callback(ch, word); /*callback – функция, которая обрабатывает принятое слово.*/
			}
		}
		if (count <= max_read)
		{
			write_reg(addr_rfc, 0xA5); // Очистить FIFO, если прочли все слова из канала! (записать 0xA5 в регистр счётчика)
		}
	}
}

/*
 * brief
 *
 * Для чтения из FIFO используется специальная SPI-команда 0xC0 | (channel << 2) (стр. 43–44).
 * Она автоматически читает 4 байта (одно ARINC-слово) из FIFO выбранного канала.
 */
void TRANSFER_ReadFifoWord(uint8_t channel, uint8_t* data_out)
{
	if (channel > 15 || !data_out) return;
	uint8_t cmd = 0xC0 | (channel << 2); /* 1100 0000 + CCCC00 */
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi_inst, &cmd, 1, HAL_MAX_DELAY);
	HAL_SPI_Receive(hspi_inst, data_out, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

/*
 * brief
 * Немедленно отправить 4-байтное слово на передатчик.
 * Команда 100101TT – 4 байта данных следуют за опкодом. Планировщик не нужен.
 *
 * - Команда 0x94 + номер канала в двух младших битах → 0x94 (канал 0), 0x95 (канал 1), 0x96 (канал 2), 0x97 (канал 3).
 * - Следом передаются 4 байта – это полное ARINC-слово (метка/статус + 3 байта данных).
 * Какой именно порядок – зависит от настройки AFLIP. Если AFLIP=1, то первый байт должен быть перевёрнутой меткой (биты в обратном порядке).
 *
 * Этот механизм позволяет CPU в любой момент отправить сообщение, даже если планировщик работает на том же канале (слово встанет в очередь).
 */
void TRANSFER_SendImmediate(uint8_t channel, const uint8_t *data)
{
    if (channel > 7 || !data) return;
    uint8_t cmd = 0xA4 | (channel & 0x07);   // 0xA4..0xA7
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_inst, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi_inst, (uint8_t*)data, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}


void TRANSFER_WriteWord(uint16_t addr, uint8_t *data)
{
    set_map(addr);
    uint8_t cmd = 0x88;   // Write memory at current MAP (HI-3220)
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_inst, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi_inst, (uint8_t*)data, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}


void TRANSFER_SetLoopback(uint8_t mask)
{
    // Записать маску в LOOPBACK
    write_reg(HI3220_LOOPBACK, mask);
    // Включить глобальный loopback в MCR
    uint8_t mcr = read_reg(HI3220_MCR_RD);
    mcr |= HI3220_MCR_LOOP;
    write_reg(HI3220_MCR_WR, mcr);
}

void Init_Holt(SPI_HandleTypeDef *hspi,
               GPIO_TypeDef *cs_port_, uint16_t cs_pin_,
               GPIO_TypeDef *mrst_port_, uint16_t mrst_pin_,
               GPIO_TypeDef *run_port_, uint16_t run_pin_)
{
	// 1. Низкоуровневая инициализация (SPI, GPIO)
	TRANSFER_Init(&hspi, cs_port_, cs_pin_, mrst_port_, mrst_pin_, run_port_, run_pin_);

	// 2. Сброс и ожидание READY
	TRANSFER_ResetAndWait();

	// 3. Глобальное включение приёма (передачу включим позже)
	TRANSFER_ConfigMaster(1, 1, 0);   // RX on, FLIP on, TX off

	// 4. Настройка всех приёмных каналов (0..15)
	HI3220_RxConfig rx_cfg = {
		.enable = 1,
		.speed = 0,          // 100 кбит/с
		.parity_en = 0,
		.decoder_en = 0,
		.fifo_flag_cond = 2  // флаг при превышении порога
	};
	for (int ch = 0; ch < 16; ch++)
	{
		TRANSFER_ConfigRxChannel(ch, &rx_cfg);
	}

	// 5. Настройка FIFO Enable Map – разрешить все метки (0..255) для всех каналов
	//    (если нужны только некоторые метки – изменить цикл)
	for (int ch = 0; ch < 16; ch++)
	{
		for (int label = 0; label < 256; label++)
		{
			uint16_t addr = HI3220_FIFO_ENABLE_BASE + (ch * 32) + (label / 8);
			uint8_t bit = 1 << (label % 8);
			uint8_t val = read_reg(addr) | bit;
			write_reg(addr, val);
		}
	}

	// 6. Установить порог FIFO = 1 (сигнал при появлении хотя бы одного слова)
	for (int ch = 0; ch < 16; ch++)
	{
		TRANSFER_SetFifoThreshold(ch, 1);
	}

	// 7. Настройка передатчиков (для immediate передачи)
	HI3220_TxConfig tx_cfg = {
		.run_stop = 0,   // планировщик не используем
		.speed = 0,
		.parity_en = 0,
		.even_odd = 0,
		.skip = 0,
		.prescale = 0,
		.tristate = 0,
		.opt50k = 0
	};
	for (int ch = 0; ch < 8; ch++)
	{
		TRANSFER_ConfigTxChannel(ch, &tx_cfg);
	}

	// 8. Включить глобальную передачу (бит A429TX в MCR)
	uint8_t mcr = read_reg(HI3220_MCR_RD);
	mcr |= HI3220_MCR_A429TX;
	write_reg(HI3220_MCR_WR, mcr);

	// 9. (Опционально) включить loopback для теста
	// TRANSFER_SetLoopback(0x01);

	// 10. Запустить чип (RUN = HIGH)
	TRANSFER_Start();
}


/*
 * Loopback – это тестовый режим, при котором выход передатчика (TXn) замыкается на вход соответствующих приёмников (RX2n и RX2n+1) внутри чипа,
 * без необходимости внешнего физического соединения. Это позволяет проверить:
 * - Корректность работы передатчика и приёмника.
 * - Правильность конфигурации каналов.
 * - Целостность данных (отправил слово – оно же и принял).
 *
 * Когда его использовать:
 * - При отладке – убедиться, что драйвер и HI-3210 работают, даже если к чипу не подключены внешние ARINC-приёмники HI-8448 или драйверы HI-8592.
 * - При самодиагностике устройства – можно периодически включать loopback, отправлять тестовое слово и проверять, что оно вернулось без искажений.
 * - В учебных/тестовых проектах – когда нет физической ARINC-сети.
 *
 * В боевом режиме (при реальной работе с ARINC-шинами) loopback должен быть выключен (TRANSFER_SetLoopback(0)), иначе все переданные данные будут замыкаться на приёмники, и реальные сообщения с шины не будут приниматься.
 */


/* Альтернатива - работа через прерывания STM32: подключить вывод AINT (или MINT)
 к GPIO STM32 с прерыванием по фронту. В обработчике читать APIR и вызывать Process ARINCMessages().
 Это эффективнее, чем постоянный опрос.
 */
