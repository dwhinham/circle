//
// kernel.cpp
//

#include <circle/alloc.h>

#include "kernel.h"

#define LONESHA256_STATIC
#include "lonesha256.h"

#define DRIVE		"SD:"
#define FILENAME_READ	"/testfile.bin"
#define FILENAME_WRITE	"/testfile2.bin"

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED)
{
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}
	
	if (bOK)
	{
		bOK = m_EMMC.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// Mount file system
	if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot mount drive: %s", DRIVE);
	}
	
	FIL File;
	FRESULT Result = f_open (&File, DRIVE FILENAME_READ, FA_READ);
	if (Result != FR_OK)
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot open %s for reading", FILENAME_READ);
	}
	
	UINT nSize = f_size (&File);
	u8* pBuffer = static_cast<u8*>(malloc(nSize));
	if (!pBuffer)
	{
		m_Logger.Write (FromKernel, LogPanic, "Failed to allocate %ld bytes", nSize);
	}
	
	m_Logger.Write (FromKernel, LogNotice, "Reading %dMB from SD card...", nSize / 1024 / 1024);

	unsigned int nStartTicks = CTimer::Get()->GetClockTicks();

	UINT nBytesRead = 0;
	Result = f_read (&File, pBuffer, nSize, &nBytesRead);
	if (Result != FR_OK)
	{
		m_Logger.Write (FromKernel, LogPanic, "Failed to read %s into memory", FILENAME_READ);
	}
	
	f_close (&File);
	assert(nSize == nBytesRead);

	float nSeconds = (CTimer::Get()->GetClockTicks() - nStartTicks) / 1000000.0f;
	m_Logger.Write (FromKernel, LogNotice, "File read in %0.2f seconds", nSeconds);
	
	m_Logger.Write (FromKernel, LogNotice, "Performing SHA256 hash...");
	u8 Hash[32]{0};
	lonesha256 (Hash, pBuffer, nSize);

	CString HexByte;
	CString HashString;
	for (u8 nByte : Hash)
	{
		HexByte.Format ("%02x", nByte);
		HashString.Append (HexByte);
	}
	
	m_Logger.Write (FromKernel, LogNotice, "SHA256 sum for %s: %s", FILENAME_READ, static_cast<const char*>(HashString));
	
	Result = f_open (&File, DRIVE FILENAME_WRITE, FA_WRITE | FA_CREATE_ALWAYS);
	if (Result != FR_OK)
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot open %s for writing", FILENAME_WRITE);
	}
	
	m_Logger.Write (FromKernel, LogNotice, "Writing %dMB to SD card...", nSize / 1024 / 1024);
	
	nStartTicks = CTimer::Get()->GetClockTicks();

	UINT nBytesWritten = 0;
	Result = f_write (&File, pBuffer, nSize, &nBytesWritten);
	if (Result != FR_OK)
	{
		m_Logger.Write (FromKernel, LogPanic, "Failed to write %s", FILENAME_WRITE);
	}
	
	f_close (&File);
	assert(nSize == nBytesWritten);
	
	nSeconds = (CTimer::Get()->GetClockTicks() - nStartTicks) / 1000000.0f;
	m_Logger.Write (FromKernel, LogNotice, "File written in %0.2f seconds", nSeconds);

	return ShutdownHalt;
}
