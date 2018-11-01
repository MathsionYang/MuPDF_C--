// Pdf2Png.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MuPDFConvert.h"

int _tmain(int argc, _TCHAR* argv[])
{
	CMuPDFConvert pdfConvert;
	int nNum = 0;
	pdfConvert.Pdf2Png(L"123.pdf", "test", nNum);
	return 0;
}

