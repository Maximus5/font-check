
/*
Copyright (c) 2009-2017 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <windows.h>
#include <mlang.h>
#include <iostream>
#include <atlbase.h>
#include <iomanip>

using namespace std;

HANDLE hOut;
CComPtr<IMLangFontLink2> fl;
DWORD fontCodePages = 0;

void write(LPCWSTR asText, int iLen = -1)
{
	DWORD written;
	WriteConsoleW(hOut, asText, (iLen >= 0) ? iLen : wcslen(asText), &written, NULL);
}

void dumpfont(HDC hdc, HFONT hFont)
{
	HFONT hOld = (HFONT)SelectObject(hdc, hFont);

	// Now, query font properties
	LPOUTLINETEXTMETRICW pOut = NULL;
	UINT nSize = GetOutlineTextMetricsW(hdc, 0, NULL);
	if (nSize)
	{
		pOut = (LPOUTLINETEXTMETRIC)calloc(nSize,1);

		if (pOut)
		{
			pOut->otmSize = nSize;

			if (GetOutlineTextMetricsW(hdc, nSize, pOut))
			{
				pOut->otmpFamilyName = (PSTR)(((LPBYTE)pOut) + (DWORD_PTR)pOut->otmpFamilyName);
				pOut->otmpFaceName = (PSTR)(((LPBYTE)pOut) + (DWORD_PTR)pOut->otmpFaceName);
				pOut->otmpStyleName = (PSTR)(((LPBYTE)pOut) + (DWORD_PTR)pOut->otmpStyleName);
				pOut->otmpFullName = (PSTR)(((LPBYTE)pOut) + (DWORD_PTR)pOut->otmpFullName);
				wcout << setbase(10) << L"Fam=" << pOut->otmTextMetrics.tmPitchAndFamily
					<< L" CS=" << pOut->otmTextMetrics.tmCharSet
					<< L" Name=`";
				write((wchar_t*)pOut->otmpFaceName);
				wcout << L"`" << endl;
			}
			free(pOut); pOut = NULL;
		}
	}

	if (hOld && hOld != hFont)
		::SelectObject(hdc, hOld);
}

void showmap(HDC hdc, HFONT hDefFont, DWORD theCodePages, long theCount, LPCWSTR asText, int iLen)
{
	HRESULT hr;

	if (asText)
	{
		wcout << L"  `";
		write(asText, iLen);
		wcout << L"` ";

		//if (!theCodePages)
		{
			wcout << L"U+" << setbase(16) << (DWORD)*asText << L" "; // "GetStrCodePages returns empty set" << endl;
			//return;
		}
	}

	if (theCodePages)
	{
		UINT cp = 0;
		HRESULT hr = fl->CodePagesToCodePage(theCodePages, 0, &cp);
		wcout << L"Pgs=0x" << setbase(16) << theCodePages << setbase(10) << L"("<<theCount<<L") CP=" << cp << L" ";
	}

	if (theCodePages && (theCodePages & fontCodePages))
	{
		wcout << L"<default> ";
		dumpfont(hdc, hDefFont);
	}
	else
	{
		HFONT linkFont = NULL;
		if (theCodePages)
			hr = fl->MapFont(hdc, theCodePages, NULL, &linkFont);
		else
			hr = fl->MapFont(hdc, 0, *asText, &linkFont);

		if (FAILED(hr))
		{
			wcout << L"MapFont failed, code=0x" << setbase(16) << (DWORD)hr << endl;
			return;
		}

		dumpfont(hdc, linkFont);
		fl->ReleaseFont(linkFont);
	}
}

void parse(HDC hdc, HFONT hDefFont, LPCWSTR asText)
{
	HRESULT hr;
	long count = lstrlen(asText);
	DWORD theCodePages = 0, prev = 0;
	long theCount = 0, prevCount = 0;
	LPCWSTR pszFrom = asText;

	// "E:\シュート・ザ・ムーン (フエタキシ)"
	// "←⇵➔➕⟵❱❯"
	hr = fl->GetStrCodePages(asText, count, fontCodePages, &theCodePages, &theCount);
	if (SUCCEEDED(hr) && theCodePages)
	{
		wcout << L"  ";
		showmap(hdc, hDefFont, theCodePages, theCount, NULL, 0);
	}
	
	while (*asText)
	{
		hr = fl->GetStrCodePages(asText, 1, fontCodePages, &theCodePages, &theCount);
		if (FAILED(hr) || !theCodePages || !theCount)
		{
			if (asText > pszFrom)
			{
				showmap(hdc, hDefFont, prev, prevCount, pszFrom, asText - pszFrom);
				pszFrom = asText+1;
				prev = 0;
			}
			showmap(hdc, hDefFont, 0, 0, asText, 1);
			pszFrom = ++asText;
			continue;
		}
		if (!prev)
		{
			prev = theCodePages;
			prevCount = theCount;
		}

		if (theCodePages == prev)
		{
			asText++;
			continue;
		}

		showmap(hdc, hDefFont, prev, prevCount, pszFrom, asText - pszFrom);
		pszFrom = asText++;
		prev = theCodePages;
		prevCount = theCount;
	}

	if (*pszFrom)
		showmap(hdc, hDefFont, theCodePages, theCount, pszFrom, -1);
}

int wmain(int argv, wchar_t** argc)
{
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	wcout << L"Arg count: " << argv << endl;

	HRESULT hr;
	HDC hdc = NULL;
	HFONT hFont = NULL, hOldFont = NULL;
	HWND hWnd = NULL;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	wcout << L"Creating instance of CMultiLanguage...";
	if (FAILED(hr = fl.CoCreateInstance(CLSID_CMultiLanguage)))
	{
		wcout << L" Failed, code=0x" << setbase(16) << (DWORD)hr << endl;
		goto wrap;
	}
	wcout << L" OK" << endl;

	wcout << L"Acquiring DC descriptor...";
	hWnd = GetConsoleWindow();
	hdc = GetDC(hWnd);
	if (!hdc)
	{
		wcout << L" Failed, code=" << setbase(10) << GetLastError() << endl;
		goto wrap;
	}
	hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	hOldFont = (HFONT)SelectObject(hdc, hFont);
	wcout << L" OK" << endl;

	wcout << L"Default font codepages: ";
	if (FAILED(hr = fl->GetFontCodePages(hdc,hFont,&fontCodePages)))
	{
		wcout << L" Failed, code=0x" << setbase(16) << (DWORD)hr << endl;
		goto wrap;
	}
	wcout << setbase(16) << L"0x" << fontCodePages << endl;

	for (int i = 0; i < argv; i++)
	{
		if (!i && argv) continue;
		wcout << L"#"<<i<<L": `";
		write(argc[i]);
		wcout << L"`" << endl;
		parse(hdc, hFont, argc[i]);
	}

wrap:
	if (hdc)
	{
		wcout << L"Releasing DC descriptor" << endl;
		if (hOldFont)
			SelectObject(hdc, hOldFont);
		ReleaseDC(hWnd, hdc);
	}
	wcout << L"Finalizing" << endl;
	fl.Release();
	CoUninitialize();
	return 0;
}
