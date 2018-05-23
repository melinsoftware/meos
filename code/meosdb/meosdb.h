// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MEOSDB_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// MEOSDB_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef MEOSDB_EXPORTS
#define MEOSDB_API  __declspec(dllexport) __cdecl
#else
#define MEOSDB_API  __declspec(dllimport) __cdecl
#endif

#include <vector>
#include <string>

/*
extern "C"{
// This class is exported from the meosdb.dll
class  Cmeosdb {
public:
	Cmeosdb(void);
	// TODO: add your methods here.
};

//extern MEOSDB_API int nmeosdb;


MEOSDB_API int fnmeosdb(void);
}*/
