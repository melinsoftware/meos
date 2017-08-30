# Microsoft Developer Studio Project File - Name="meos" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=meos - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "meos.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "meos.mak" CFG="meos - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "meos - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "meos - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "meos - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x41d /d "NDEBUG"
# ADD RSC /l 0x41d /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 Msimg32.lib comctl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "meos - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x41d /d "_DEBUG"
# ADD RSC /l 0x41d /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 Msimg32.lib comctl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "meos - Win32 Release"
# Name "meos - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\csvparser.cpp
# End Source File
# Begin Source File

SOURCE=.\gdioutput.cpp
# End Source File
# Begin Source File

SOURCE=.\meos.cpp
# End Source File
# Begin Source File

SOURCE=.\meos.rc
# End Source File
# Begin Source File

SOURCE=.\oBase.cpp
# End Source File
# Begin Source File

SOURCE=.\oCard.cpp
# End Source File
# Begin Source File

SOURCE=.\oClass.cpp
# End Source File
# Begin Source File

SOURCE=.\oClub.cpp
# End Source File
# Begin Source File

SOURCE=.\oControl.cpp
# End Source File
# Begin Source File

SOURCE=.\oCourse.cpp
# End Source File
# Begin Source File

SOURCE=.\oEvent.cpp
# End Source File
# Begin Source File

SOURCE=.\oPunch.cpp
# End Source File
# Begin Source File

SOURCE=.\oRunner.cpp
# End Source File
# Begin Source File

SOURCE=.\pages.cpp
# End Source File
# Begin Source File

SOURCE=.\pages_classes.cpp
# End Source File
# Begin Source File

SOURCE=.\pages_competition.cpp
# End Source File
# Begin Source File

SOURCE=.\pages_course.cpp
# End Source File
# Begin Source File

SOURCE=.\pages_lists.cpp
# End Source File
# Begin Source File

SOURCE=.\pages_si.cpp
# End Source File
# Begin Source File

SOURCE=.\random.cpp
# End Source File
# Begin Source File

SOURCE=.\SportIdent.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\TimeStamp.cpp
# End Source File
# Begin Source File

SOURCE=.\xmlparser.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\csvparser.h
# End Source File
# Begin Source File

SOURCE=.\gdioutput.h
# End Source File
# Begin Source File

SOURCE=.\meos.h
# End Source File
# Begin Source File

SOURCE=.\oBase.h
# End Source File
# Begin Source File

SOURCE=.\oCard.h
# End Source File
# Begin Source File

SOURCE=.\oClass.h
# End Source File
# Begin Source File

SOURCE=.\oClub.h
# End Source File
# Begin Source File

SOURCE=.\oControl.h
# End Source File
# Begin Source File

SOURCE=.\oCourse.h
# End Source File
# Begin Source File

SOURCE=.\oEvent.h
# End Source File
# Begin Source File

SOURCE=.\oPunch.h
# End Source File
# Begin Source File

SOURCE=.\oRunner.h
# End Source File
# Begin Source File

SOURCE=.\random.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SportIdent.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TimeStamp.h
# End Source File
# Begin Source File

SOURCE=.\xmlparser.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\meos.ico
# End Source File
# Begin Source File

SOURCE=.\small.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
# Section meos : {4E2984B5-19E9-4743-8ED4-A0D97FA48101}
# 	2:21:DefaultSinkHeaderFile:_sicomm.h
# 	2:16:DefaultSinkClass:C_SiComm
# End Section
# Section meos : {D2F56B3B-02FC-415B-843E-AFBA48C5F2B2}
# 	2:5:Class:C_SiComm
# 	2:10:HeaderFile:_sicomm.h
# 	2:8:ImplFile:_sicomm.cpp
# End Section
