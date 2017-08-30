/*
Code is based on mini unzip, demo of unzip package.

Ported to C++ and modified to suite MeOS.

*/

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>

#include <direct.h>
#include <io.h>

#include "meosexception.h"
#include "meos_util.h"
#include "minizip/unzip.h"
#include "minizip/zip.h"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (1024 * 256)
#define MAXFILENAME (260)

#define USEWIN32IOAPI
#include "minizip/iowin32.h"

const char *zipError = "Error processing zip-file";

/* change_file_date : change the date/time of a file
filename : the filename of the file where date/time must be modified
dosdate : the new date at the MSDos format (4 bytes)
tmu_date : the SAME new date at the tm_unz format */
void change_file_date(const wchar_t *filename, uLong dosdate, tm_unz tmu_date) {
  HANDLE hFile;
  FILETIME ftm,ftLocal,ftCreate,ftLastAcc,ftLastWrite;

  hFile = CreateFile(filename,GENERIC_READ | GENERIC_WRITE,
    0,NULL,OPEN_EXISTING,0,NULL);
  GetFileTime(hFile,&ftCreate,&ftLastAcc,&ftLastWrite);
  DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate,&ftLocal);
  LocalFileTimeToFileTime(&ftLocal,&ftm);
  SetFileTime(hFile,&ftm,&ftLastAcc,&ftm);
  CloseHandle(hFile);
}


/* mymkdir and change_file_date are not 100 % portable
As I don't know well Unix, I wait feedback for the unix portion */

int mymkdir(const wchar_t *dirname)
{
  int ret=0;
  ret = _wmkdir(dirname);
  return ret;
}

int makedir (const wchar_t *newdir)
{
  int  len = (int)wcslen(newdir);

  if (len <= 0)
    return 0;

  wstring buffer = newdir;

  if (buffer[len-1] == '/') {
    buffer = buffer.substr(0, len-1);
  }

  if (mymkdir(buffer.c_str()) == 0) {
    return 1;
  }

  size_t index = 1;
  while (index<buffer.length()) {

    while(index<buffer.length() && buffer[index] != '\\' && buffer[index] != '/')
      index++;
    wchar_t hold = buffer[index];

    wstring sub = buffer.substr(0, index);
    if ((mymkdir(sub.c_str()) == -1) && (errno == ENOENT)) {
      throw std::exception("Error creating directories");
    }
    index++;
    if (hold == 0)
      break;
  }
  return 1;
}

wstring do_extract_currentfile(unzFile uf, const wstring &baseDir, const char* password)
{
  wstring write_filename;
  char filename_inzip[256];
  char* filename_withoutpath;
  char* p;
  int err=UNZ_OK;
  FILE *fout=NULL;
  void* buf;
  uInt size_buf;

  bool createSubdir = false;

  unz_file_info64 file_info;
  err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

  if (err!=UNZ_OK)
    throw std::exception(zipError);

  size_buf = WRITEBUFFERSIZE;
  vector<BYTE> byteBuff;
  byteBuff.resize(size_buf);
  buf = (void *)&byteBuff[0];

  p = filename_withoutpath = filename_inzip;
  while ((*p) != '\0') {
    if (((*p)=='/') || ((*p)=='\\'))
      filename_withoutpath = p+1;
    p++;
  }

  if ((*filename_withoutpath)=='\0') {
    if (createSubdir) {
      string x(filename_inzip);
      wstring wx(x.begin(), x.end());
      mymkdir(wx.c_str());
    }
  }
  else {
    write_filename = baseDir;
    if (createSubdir) {
      string x(filename_inzip);
      write_filename.insert(write_filename.begin() + write_filename.length(), x.begin(), x.end());
    }
    else {
      string x(filename_withoutpath);
      write_filename.insert(write_filename.begin() + write_filename.length(), x.begin(), x.end());
    }
    err = unzOpenCurrentFilePassword(uf,password);
    if (err!=UNZ_OK)
      throw std::exception(zipError);

    fout = fopen64(write_filename.c_str(),L"wb");

    // some zipfile doesn't contain directory alone before file
    if ((fout==NULL) && createSubdir &&
      (filename_withoutpath!=(char*)filename_inzip)) {
      char c=*(filename_withoutpath-1);
      *(filename_withoutpath-1)='\0';
      makedir(write_filename.c_str());
      *(filename_withoutpath-1)=c;
      fout=fopen64(write_filename.c_str(),L"wb");
    }

    if (fout==NULL) {
      wstring err = L"Error opening " + write_filename;
      throw meosException(err);
    }

    do {
      err = unzReadCurrentFile(uf,buf,size_buf);
      if (err<0)
        throw std::exception(zipError);

      if (err>0)
        if (fwrite(buf,err,1,fout)!=1) {
          throw std::exception("Error writing extracted file.");
        }
    }
    while (err>0);

    if (fout)
      fclose(fout);

    change_file_date(write_filename.c_str(),file_info.dosDate, file_info.tmu_date);


    err = unzCloseCurrentFile (uf);
    if (err != UNZ_OK)
      throw std::exception(zipError);
   }

  return write_filename;
}


void do_extract(unzFile uf, const wchar_t *basePath, const char* password, vector<wstring> &extractedFiles)
{
  uLong i;
  unz_global_info64 gi;
  int err;

  err = unzGetGlobalInfo64(uf,&gi);
  if (err != UNZ_OK)
    throw std::exception(zipError);

  for (i=0;i<gi.number_entry;i++) {
    wstring name = do_extract_currentfile(uf, basePath, password);
    registerTempFile(name);
    extractedFiles.push_back(name);

    if ((i+1)<gi.number_entry) {
      err = unzGoToNextFile(uf);
      if (err != UNZ_OK) {
        throw std::exception(zipError);
        break;
      }
    }
  }
}

void unzip(const wchar_t *wzipfilename, const char *password, vector<wstring> &extractedFiles)
{
  wstring wzfn(wzipfilename);
  string zfn(wzfn.begin(), wzfn.end());
  extractedFiles.clear();
  zlib_filefunc64_def ffunc;
  fill_win32_filefunc64W(&ffunc);
  unzFile uf = unzOpen2_64(wzfn.c_str(),&ffunc);

  if (uf==NULL)
    throw std::exception("Cannot open zip file");

  wstring base = getTempPath();
  wchar_t end = base[base.length()-1];
  if (end != '\\' && end != '/')
    base += L"\\";

  int id = rand();
  wstring target;
  do {
    target = base + L"zip" + itow(id) + L"\\";
    id++;
  }
  while ( _waccess( target.c_str(), 0 ) == 0 );

  if (CreateDirectory(target.c_str(), NULL) == 0)
    throw std::exception("Failed to create temporary folder");

  registerTempFile(target);
  do_extract(uf, target.c_str(), password, extractedFiles);

  unzClose(uf);
}



uLong filetime(const wchar_t *f, uLong *dt) {
  int ret = 0;
  FILETIME ftLocal;
  HANDLE hFind;
  WIN32_FIND_DATA ff32;

  hFind = FindFirstFile(f,&ff32);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
    FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
    FindClose(hFind);
    ret = 1;
  }
  return ret;
}

int check_exist_file(const wchar_t* filename)
{
  FILE* ftestexist;
  int ret = 1;
  ftestexist = fopen64(filename,L"rb");
  if (ftestexist==NULL)
      ret = 0;
  else
      fclose(ftestexist);
  return ret;
}

/* calculate the CRC32 of a file,  because to encrypt a file, we need known the CRC32 of the file before */
/*int getFileCrc(const char* filenameinzip,void*buf,unsigned long size_buf,unsigned long* result_crc)
{
   unsigned long calculate_crc=0;
   int err=ZIP_OK;
   FILE * fin = fopen64(filenameinzip,"rb");
   unsigned long size_read = 0;
   unsigned long total_read = 0;
   if (fin==NULL)
   {
       err = ZIP_ERRNO;
   }

    if (err == ZIP_OK)
        do
        {
            err = ZIP_OK;
            size_read = (int)fread(buf,1,size_buf,fin);
            if (size_read < size_buf)
                if (feof(fin)==0)
            {
                printf("error in reading %s\n",filenameinzip);
                err = ZIP_ERRNO;
            }

            if (size_read>0)
                calculate_crc = crc32(calculate_crc,buf,size_read);
            total_read += size_read;

        } while ((err == ZIP_OK) && (size_read>0));

    if (fin)
        fclose(fin);

    *result_crc=calculate_crc;
    printf("file %s crc %lx\n", filenameinzip, calculate_crc);
    return err;
}*/

int isLargeFile(const wchar_t* filename)
{
  int largeFile = 0;
  ZPOS64_T pos = 0;
  FILE* pFile = fopen64(filename, L"rb");

  if (pFile != NULL) {
    fseeko64(pFile, 0, SEEK_END);
    pos = ftello64(pFile);
    if (pos >= 0xffffffff)
      largeFile = 1;
    fclose(pFile);
  }

  return largeFile;
}

int zip(const wchar_t *zipfilename, const char *password, const vector<wstring> &files) {
  int opt_compress_level= Z_BEST_COMPRESSION;
  const int opt_exclude_path = 1;
  wchar_t filename_try[MAXFILENAME+16];
  int err=0;
  int size_buf=0;
  wchar_t eb[256];

  size_buf = WRITEBUFFERSIZE;
  vector<BYTE> vbuff(size_buf, 0);
  void * buf = (void*)&vbuff[0];

  zipFile zf;
  int errclose;
  zlib_filefunc64_def ffunc;
  fill_win32_filefunc64W(&ffunc);
  //wstring wzipfn(zipfilename);
  //string zipfn(wzipfn.begin(), wzipfn.end());
  wcscpy_s(filename_try, zipfilename);
  zf = zipOpen2_64(filename_try, 0,NULL, &ffunc);

  if (zf == NULL) {
    swprintf_s(eb, L"Error opening %s.",filename_try);
    throw meosException(eb);
  }

  for (size_t i=0; i < files.size(); i++) {
    FILE * fin;
    int size_read;
    const wstring &wfn = files[i];
    //string asciiName(wfn.begin(), wfn.end());
    const wchar_t* filenameinzip = wfn.c_str();
    const wchar_t *savefilenameinzip;
    zip_fileinfo zi;
    unsigned long crcFile=0;
    int zip64 = 0;

    zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
    zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
    zi.dosDate = 0;
    zi.internal_fa = 0;
    zi.external_fa = 0;
    filetime(filenameinzip, &zi.dosDate);

/*
    if ((password != NULL) && (err==ZIP_OK))
        err = getFileCrc(filenameinzip,buf,size_buf,&crcFile);
        */
    zip64 = isLargeFile(wfn.c_str());

    /* The path name saved, should not include a leading slash. */
    /*if it did, windows/xp and dynazip couldn't read the zip file. */
    savefilenameinzip = filenameinzip;
    while( savefilenameinzip[0] == '\\' || savefilenameinzip[0] == '/' ) {
      savefilenameinzip++;
    }

    /*should the zip file contain any path at all?*/
    if ( opt_exclude_path ) {
      const wchar_t *tmpptr;
      const wchar_t *lastslash = 0;
      for( tmpptr = savefilenameinzip; *tmpptr; tmpptr++) {
        if ( *tmpptr == '\\' || *tmpptr == '/')
          lastslash = tmpptr;
      }
      if ( lastslash != NULL )  {
        savefilenameinzip = lastslash+1; // base filename follows last slash.
      }
    }

    /**/
    wstring wn = savefilenameinzip;
    string asciiName(wn.begin(), wn.end());
    err = zipOpenNewFileInZip3_64(zf, asciiName.c_str(),&zi,
                      NULL,0,NULL,0,NULL /* comment*/,
                      (opt_compress_level != 0) ? Z_DEFLATED : 0,
                      opt_compress_level,0,
                      /* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
                      -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                      password,crcFile, zip64);

    if (err != ZIP_OK) {
      swprintf_s(eb, L"Error opening %s in zipfile",filenameinzip);
      throw meosException(eb);
    }
    else {
      fin = fopen64(wfn.c_str(),L"rb");
      if (fin==NULL)  {
        swprintf_s(eb, L"Error opening %s for reading",filenameinzip);
        throw meosException(eb);
      }
    }

    if (err == ZIP_OK)
      do {
        err = ZIP_OK;
        size_read = (int)fread(buf,1,size_buf,fin);
        if (size_read < size_buf) {
          if (feof(fin)==0) {
            swprintf_s(eb, L"Error reading %s",filenameinzip);
            throw meosException(eb);
          }
        }

        if (size_read > 0) {
          err = zipWriteInFileInZip (zf,buf,size_read);
          if (err<0) {
            swprintf_s(eb, L"Error in writing %s in the zipfile",filenameinzip);
            throw meosException(eb);
          }
        }
      } while ((err == ZIP_OK) && (size_read>0));

    if (fin)
        fclose(fin);

    if (err<0)
        err=ZIP_ERRNO;
    else {
      err = zipCloseFileInZip(zf);
      if (err!=ZIP_OK) {
        swprintf_s(eb, L"Error closing %s in the zipfile", filenameinzip);
        throw meosException(eb);
      }
    }
  }

  errclose = zipClose(zf,NULL);
  if (errclose != ZIP_OK) {
    swprintf_s(eb, L"Error closing %s",filename_try);
    throw meosException(eb);
  }

  return 0;
}
