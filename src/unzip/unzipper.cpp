#include "unzipper.h"
#include "zip.h"
#include "unzip.h"

#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
#  ifndef __USE_FILE_OFFSET64
#    define __USE_FILE_OFFSET64
#  endif
#  ifndef __USE_LARGEFILE64
#    define __USE_LARGEFILE64
#  endif
#  ifndef _LARGEFILE64_SOURCE
#    define _LARGEFILE64_SOURCE
#  endif
#  ifndef _FILE_OFFSET_BIT
#    define _FILE_OFFSET_BIT 64
#  endif
#endif

#ifdef __APPLE__
/* In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions */
#  define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#  define FTELLO_FUNC(stream) ftello(stream)
#  define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#  define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#  define FTELLO_FUNC(stream) ftello64(stream)
#  define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
#  include <direct.h>
#  include <io.h>
#else
#  include <sys/stat.h>
#  include <unistd.h>
#  include <utime.h>
#endif

#ifdef _WIN32
#  define MKDIR(d) _mkdir(d)
#  define CHDIR(d) _chdir(d)
#else
#  define MKDIR(d) mkdir(d, 0775)
#  define CHDIR(d) chdir(d)
#endif

#include "unzip.h"

#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME     (256)

#ifdef _WIN32
#  define USEWIN32IOAPI
#  include "iowin32.h"
#endif

void change_file_date(const char *filename, uLong dosdate, tm_unz tmu_date)
{
#ifdef _WIN32
    HANDLE hFile;
    FILETIME ftm, ftLocal, ftCreate, ftLastAcc, ftLastWrite;

    hFile = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, &ftCreate, &ftLastAcc, &ftLastWrite);
        DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate, &ftLocal);
        LocalFileTimeToFileTime(&ftLocal, &ftm);
        SetFileTime(hFile, &ftm, &ftLastAcc, &ftm);
        CloseHandle(hFile);
    }
#else
#if defined unix || defined __APPLE__
    struct utimbuf ut;
    struct tm newdate;

    newdate.tm_sec = tmu_date.tm_sec;
    newdate.tm_min = tmu_date.tm_min;
    newdate.tm_hour = tmu_date.tm_hour;
    newdate.tm_mday = tmu_date.tm_mday;
    newdate.tm_mon = tmu_date.tm_mon;
    if (tmu_date.tm_year > 1900)
        newdate.tm_year = tmu_date.tm_year - 1900;
    else
        newdate.tm_year = tmu_date.tm_year ;
    newdate.tm_isdst = -1;

    ut.actime = ut.modtime = mktime(&newdate);
    utime(filename,&ut);
#endif
#endif
}

int check_file_exists(const char* filename)
{
    FILE* ftestexist = FOPEN_FUNC(filename,"rb");
    if (ftestexist == NULL)
        return 0;
    fclose(ftestexist);
    return 1;
}

int makedir(const char *newdir)
{
    char *buffer = NULL;
    char *p = NULL;
    int len = (int)strlen(newdir);

    if (len <= 0)
        return 0;

    buffer = (char*)malloc(len+1);
    if (buffer == NULL)
    {
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    strcpy(buffer, newdir);

    if (buffer[len-1] == '/')
        buffer[len-1] = 0;

    if (MKDIR(buffer) == 0)
    {
        free(buffer);
        return 1;
    }

    p = buffer + 1;
    while (1)
    {
        char hold;
        while(*p && *p != '\\' && *p != '/')
            p++;
        hold = *p;
        *p = 0;

        if ((MKDIR(buffer) == -1) && (errno == ENOENT))
        {
            printf("couldn't create directory %s (%d)\n", buffer, errno);
            free(buffer);
            return 0;
        }

        if (hold == 0)
            break;

        *p++ = hold;
    }

    free(buffer);
    return 1;
}


int do_extract_currentfile(unzFile uf, int opt_extract_without_path, int* popt_overwrite, const char *password)
{
    unz_file_info file_info = {0};
    FILE* fout = NULL;
    void* buf = NULL;
    uInt size_buf = WRITEBUFFERSIZE;
    int err = UNZ_OK;
    int errclose = UNZ_OK;
    int skip = 0;
    char filename_inzip[256] = {0};
    char* filename_withoutpath = NULL;
    const char* write_filename = NULL;
    char* p = NULL;

    err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
    {
        printf("error %d with zipfile in unzGetCurrentFileInfo\n",err);
        return err;
    }

    p = filename_withoutpath = filename_inzip;
    while (*p != 0)
    {
        if ((*p == '/') || (*p == '\\'))
            filename_withoutpath = p+1;
        p++;
    }

    /* If zip entry is a directory then create it on disk */
    if (*filename_withoutpath == 0)
    {
        if (opt_extract_without_path == 0)
        {
            printf("creating directory: %s\n", filename_inzip);
            MKDIR(filename_inzip);
        }
        return err;
    }

    buf = (void*)malloc(size_buf);
    if (buf == NULL)
    {
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    err = unzOpenCurrentFilePassword(uf, password);
    if (err != UNZ_OK)
        printf("error %d with zipfile in unzOpenCurrentFilePassword\n", err);

    if (opt_extract_without_path)
        write_filename = filename_withoutpath;
    else
        write_filename = filename_inzip;

    /* Determine if the file should be overwritten or not and ask the user if needed */
    if ((err == UNZ_OK) && (*popt_overwrite == 0) && (check_file_exists(write_filename)))
    {
        char rep = 0;
        do
        {
            char answer[128];
            printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ", write_filename);
            if (scanf("%1s", answer) != 1)
                exit(EXIT_FAILURE);
            rep = answer[0];
            if ((rep >= 'a') && (rep <= 'z'))
                rep -= 0x20;
        }
        while ((rep != 'Y') && (rep != 'N') && (rep != 'A'));

        if (rep == 'N')
            skip = 1;
        if (rep == 'A')
            *popt_overwrite = 1;
    }

    /* Create the file on disk so we can unzip to it */
    if ((skip == 0) && (err == UNZ_OK))
    {
        fout = FOPEN_FUNC(write_filename, "wb");
        /* Some zips don't contain directory alone before file */
        if ((fout == NULL) && (opt_extract_without_path == 0) &&
            (filename_withoutpath != (char*)filename_inzip))
        {
            char c = *(filename_withoutpath-1);
            *(filename_withoutpath-1) = 0;
            makedir(write_filename);
            *(filename_withoutpath-1) = c;
            fout = FOPEN_FUNC(write_filename, "wb");
        }
        if (fout == NULL)
            printf("error opening %s\n", write_filename);
    }

    /* Read from the zip, unzip to buffer, and write to disk */
    if (fout != NULL)
    {
        printf(" extracting: %s\n", write_filename);

        do
        {
            err = unzReadCurrentFile(uf, buf, size_buf);
            if (err < 0)
            {
                printf("error %d with zipfile in unzReadCurrentFile\n", err);
                break;
            }
            if (err == 0)
                break;
            if (fwrite(buf, err, 1, fout) != 1)
            {
                printf("error %d in writing extracted file\n", errno);
                err = UNZ_ERRNO;
                break;
            }
        }
        while (err > 0);

        if (fout)
            fclose(fout);

        /* Set the time of the file that has been unzipped */
        if (err == 0)
            change_file_date(write_filename,file_info.dosDate, file_info.tmu_date);
    }

    errclose = unzCloseCurrentFile(uf);
    if (errclose != UNZ_OK)
        printf("error %d with zipfile in unzCloseCurrentFile\n", errclose);

    free(buf);
    return err;
}

int do_extract_all(unzFile uf, int opt_extract_without_path, int opt_overwrite, const char *password)
{
    int err = unzGoToFirstFile(uf);
    if (err != UNZ_OK)
    {
        printf("error %d with zipfile in unzGoToFirstFile\n", err);
        return 1;
    }

    do
    {
        err = do_extract_currentfile(uf, opt_extract_without_path, &opt_overwrite, password);
        if (err != UNZ_OK)
            break;
        err = unzGoToNextFile(uf);
    }
    while (err == UNZ_OK);

    if (err != UNZ_END_OF_LIST_OF_FILE)
    {
        printf("error %d with zipfile in unzGoToNextFile\n", err);
        return 1;
    }
    return 0;
}

int do_extract_onefile(unzFile uf, const char* filename, int opt_extract_without_path, int opt_overwrite,
    const char* password)
{
    if (unzLocateFile(uf, filename, NULL) != UNZ_OK)
    {
        printf("file %s not found in the zipfile\n", filename);
        return 2;
    }
    if (do_extract_currentfile(uf, opt_extract_without_path, &opt_overwrite, password) == UNZ_OK)
        return 0;
    return 1;
}

int unzip(const char* zipfilename, const char* unzipPath, bool overwrite)
{
    unzFile uf=NULL;
    char filename_try[MAXFILENAME+16] = "";
#ifdef USEWIN32IOAPI
    zlib_filefunc_def ffunc;
#endif
    strncpy(filename_try, zipfilename, MAXFILENAME-1);
    /* strncpy doesnt append the trailing NULL, of the string is too long. */
    filename_try[ MAXFILENAME ] = '\0';

# ifdef USEWIN32IOAPIx
    fill_win32_filefunc(&ffunc);
    uf = unzOpen2(zipfilename,&ffunc);
#else
    uf = unzOpen(zipfilename);
#endif

    if (uf==NULL)
    {
        printf("Cannot open %s or %s.zip\n",zipfilename,zipfilename);
        return 1;
    }
    printf("%s opened\n",filename_try);
    if (chdir(unzipPath))
    {
        printf("Error changing into %s, aborting\n", unzipPath);
        return 2;
    }

    int res = do_extract_all(uf,0,overwrite,"");
    unzCloseCurrentFile(uf);
    return res;
}
