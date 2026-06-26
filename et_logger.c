#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <string.h>

/* ============================== */
#define BAUD_RATE CBR_115200
#define SYNC0 0xAA
#define SYNC1 0x55
#define PACKET_SIZE 15
#define INSTRUCTION_COUNT 2655
#define STATIC_WCET 3996

static volatile bool running = true;
static HANDLE hSerial = INVALID_HANDLE_VALUE;
static FILE *fp = NULL;
static bool file_opened = false;
static uint32_t highest_et_ns = 0;

void handle_sigint(int sig)
{
	(void)sig;
	running = false;
	printf("\nStopping logger...\n");
}

int main(int argc, char *argv[])
{
	/* Get test name */
	char test_name[64] = {0};
	printf("Enter test name (or press Enter for no prefix): ");
	fgets(test_name, sizeof(test_name), stdin);

	/* Remove newline */
	size_t len = strlen(test_name);
	if (len > 0 && test_name[len - 1] == '\n')
		test_name[len - 1] = '\0';

	/* Parse COM port from command line */
	char com_port_input[32];
	const char *com_port = "COM6"; // Default
	if (argc > 1)
	{
		com_port = argv[1];
	}

	signal(SIGINT, handle_sigint);

	/* Open serial port with retry */
	char port_name[32];
	while (1)
	{
		snprintf(port_name, sizeof(port_name), "\\\\.\\%s", com_port);
		hSerial = CreateFileA(port_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

		if (hSerial != INVALID_HANDLE_VALUE)
			break;

		printf("ERROR: Cannot open %s\n", com_port);
		printf("Enter COM port to use (e.g., COM6): ");
		if (fgets(com_port_input, sizeof(com_port_input), stdin) == NULL)
			return 1;

		/* Remove newline */
		len = strlen(com_port_input);
		if (len > 0 && com_port_input[len - 1] == '\n')
			com_port_input[len - 1] = '\0';

		if (strlen(com_port_input) == 0)
			return 1;

		com_port = com_port_input;
	}

	/* Configure serial */
	DCB dcb = {0};
	dcb.DCBlength = sizeof(dcb);
	GetCommState(hSerial, &dcb);
	dcb.BaudRate = BAUD_RATE;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	SetCommState(hSerial, &dcb);

	COMMTIMEOUTS timeouts = {0};
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	SetCommTimeouts(hSerial, &timeouts);

	/* Prepare filename */
	char filename[128];
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char timestamp[64];
	strftime(timestamp, sizeof(timestamp), "et_log_%Y-%m-%d_%H-%M-%S.csv", t);

	if (strlen(test_name) > 0)
		snprintf(filename, sizeof(filename), "%s-%s", test_name, timestamp);
	else
		snprintf(filename, sizeof(filename), "%s", timestamp);

	printf("Listening on %s @115200 baud\n", com_port);
	printf("Waiting for first valid packet (15 bytes)...\n");
	printf("Press Ctrl+C to stop\n\n");

	uint8_t buffer[PACKET_SIZE];
	DWORD bytes_read = 0;
	size_t offset = 0;

	while (running)
	{
		ReadFile(hSerial, buffer + offset, PACKET_SIZE - offset, &bytes_read, NULL);

		if (bytes_read == 0)
			continue;

		offset += bytes_read;

		if (offset < PACKET_SIZE)
			continue;

		/* Validate sync */
		if (buffer[0] != SYNC0 || buffer[1] != SYNC1)
		{
			memmove(buffer, buffer + 1, PACKET_SIZE - 1);
			offset = PACKET_SIZE - 1;
			continue;
		}

		/* Decode packet */
		uint8_t state = buffer[2];
		uint32_t et_ns = 0;
		uint32_t cycles = 0;
		uint32_t instructions = 0;

		/* Decode et_ns (4 bytes) */
		for (int i = 0; i < 4; i++)
			et_ns |= ((uint32_t)buffer[3 + i]) << (8 * i);

		/* Decode cycles (4 bytes) */
		for (int i = 0; i < 4; i++)
			cycles |= ((uint32_t)buffer[7 + i]) << (8 * i);

		/* Decode instructions (4 bytes) */
		for (int i = 0; i < 4; i++)
			instructions |= ((uint32_t)buffer[11 + i]) << (8 * i);

		double swcet_ratio = (double)cycles / (double)STATIC_WCET;

		/* Create CSV on first packet */
		if (!file_opened)
		{
			fp = fopen(filename, "w");
			if (!fp)
			{
				printf("ERROR: Cannot create CSV file\n");
				break;
			}
			fprintf(fp, "state,et_ns,cycles,instructions,swcet_ratio\n");
			fflush(fp);
			file_opened = true;
			printf("Logging started: %s\n", filename);
		}

		/* Print only when new highest WCET found */
		if (et_ns > highest_et_ns)
		{
			highest_et_ns = et_ns;
			printf("State: %u | et_ns: %u | Cycles: %u | Instr: %u | SWCET Ratio: %.3f\n",
						 state, et_ns, cycles, instructions, swcet_ratio);
			fflush(stdout);
		}

		fprintf(fp, "%u,%u,%u,%u,%.3f\n", state, et_ns, cycles, instructions, swcet_ratio);
		fflush(fp);

		offset = 0;
	}

	if (file_opened)
		fclose(fp);
	CloseHandle(hSerial);

	if (file_opened)
		printf("\nSaved to %s\n", filename);
	else
		printf("\nNo data received. No file created.\n");

	return 0;
}
