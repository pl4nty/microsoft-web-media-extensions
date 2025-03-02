//

#pragma once

//#include <Windows.h>

/*---------------------------------------------------------------------------*\
* CProtect: class definition
\*---------------------------------------------------------------------------*/
class CProtect
{
public:
	CProtect(LPCRITICAL_SECTION lpcs) : m_lpcs(lpcs)
	{
		EnterCriticalSection(m_lpcs);
	}
	~CProtect()
	{
		LeaveCriticalSection(m_lpcs);
	}

private:
	LPCRITICAL_SECTION m_lpcs;

}; /* CProtect */