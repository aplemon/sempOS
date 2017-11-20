#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>


typedef unsigned char       UINT8;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;

#define SAFE_ALLOCA(name, size)                                                             \
    ; unsigned char ____anonymous_##name[(size)]; name = (void *)____anonymous_##name;      \
    memset(____anonymous_##name, 0, sizeof(____anonymous_##name))

#define LICENSE_LEN (32+1)

#pragma pack(1)
typedef struct
{
    UINT64 type: 10;
    UINT64     :4;
    UINT64 /*chassis*/: 10;
    UINT64 slot: 10;
    UINT64 pos:  10;
    UINT64 port: 10;
    UINT64 iline:1;
    UINT64 line: 9;
} IF_INDEX_T;
#pragma pack()

static int parseLine(char *line, IF_INDEX_T *interface, char *license);
static void parse(FILE *fp);
static int licenseEncode(char *s, UINT32 len);
static int licenseDecode(char *s, UINT32 len);
static int licenseGen(char *identifier, char *license, UINT32 len);

static void usage(void)
{
    printf("usage : ./SempOSLicenseGen <file>\n");
    printf("\n");
    return;
}
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        usage();
        return -1;
    }

    FILE *fp = NULL;
    char *filePath = argv[1];

    if (access(filePath, F_OK | R_OK))
    {
        printf("[%04d]%s\n", __LINE__, strerror(errno));
        return -1;
    }
    if ((fp = fopen(filePath, "r")) == NULL)
    {
        printf("[%04d]%s\n", __LINE__, strerror(errno));
        return -1;
    }

#define MAX_SIZE (1024*1024)
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    if (size > MAX_SIZE)
    {
        printf("[%04d]support file size %d at most\n", __LINE__, MAX_SIZE);
        return -1;
    }
    fseek(fp, 0, SEEK_SET);

    parse(fp);

    fclose(fp);
    return 0;
}

static char *getRow(char *line, int row, char **start, char **end)
{
    if (!line)
    {
        return NULL;
    }

    char *pRow = line;

    int i = 0;
    do {
        if (!(pRow = strchr(pRow, ' ')))
            break;
        while (*pRow == ' ') pRow++;

        i++;
    } while (i <= row);
    if (i > row)
    {
        *start = pRow;
        *end = strchr(pRow, ' ') ? : pRow + strlen(pRow);
    }
    else
    {
        *start = *end = NULL;
    }

    return *start;
}
static int parseIf(char *pIf, IF_INDEX_T *interface)
{
    if (!pIf || !interface)
    {
        return -1;
    }

    char *slotStr   SAFE_ALLOCA(slotStr, strlen(pIf));
    char *posStr    SAFE_ALLOCA(posStr, strlen(pIf));
    char *portStr   SAFE_ALLOCA(portStr, strlen(pIf));
    char *lineStr   SAFE_ALLOCA(lineStr, strlen(pIf));
    if (strchr(pIf, '.') ?
        sscanf(pIf, "%[0-9]/%[0-9]/%[0-9].%[0-9]", slotStr, posStr, portStr, lineStr) != 4 :
        sscanf(pIf, "%[0-9]/%[0-9]/%[0-9]", slotStr, posStr, portStr) != 3)
    {
        return -1;
    }

    interface->iline = strchr(pIf, '.') ? 1 : 0;
    interface->slot = strtoul(slotStr, NULL, 0);
    interface->pos  = strtoul(posStr, NULL, 0);
    interface->port = strtoul(portStr, NULL, 0);
    interface->line = strtoul(lineStr, NULL, 0);

    return 0;
}
static int parseIdentifier(char *pID)
{
    if (!pID)
    {
        return -1;
    }

    char *identifier    SAFE_ALLOCA(identifier, strlen(pID)+1);

    UINT32 i = 0;
    while (pID[i] != '\0')
    {
        if (pID[i] == '-')
        {
            snprintf(identifier+strlen(identifier), strlen(pID)+1-strlen(identifier), "-");
            i += 1;
        }
        else
        {
            char code[3] = { 0 };
            UINT32 val = 0;
            strncpy(code, &pID[i], sizeof(code)-1);
            sscanf(code, "%x", &val);
            snprintf(identifier+strlen(identifier), strlen(pID)+1-strlen(identifier), "%c", (int)val);
            i += 2;
        }
    }
    printf("identifier: %s\n", identifier);

    return 0;
}
static int parseLine(char *line, IF_INDEX_T *interface, char *identifier)
{
    if (!line || !interface || !identifier)
    {
        return -1;
    }

    char *pStart = NULL, *pEnd = NULL;
    char *pIf   SAFE_ALLOCA(pIf, strlen(line)+1);
    char *pID   SAFE_ALLOCA(pID, strlen(line)+1);

    if (!getRow(line, 0, &pStart, &pEnd))
    {
        return -1;
    }
    strncpy(pIf, pStart, (int)(pEnd-pStart));

    IF_INDEX_T idx = { };
    if (parseIf(pIf, &idx))
    {
        return -1;
    }
    *interface = idx;
    printf("interface: %s -> %d/%d/%d.%d\n", pIf, idx.slot, idx.pos, idx.port, idx.line);

    if (!getRow(line, 3, &pStart, &pEnd))
    {
        return -1;
    }
    strncpy(pID, pStart, (int)(pEnd-pStart));
    strcpy(identifier, pID);
    printf("identifier: %s\n", pID);

    return 0;
}
static void parse(FILE *fp)
{
    char isFirst = 1;
    UINT32 slot = -1;
    char line[2048] = {0};
    while (fgets(line, sizeof(line), fp))
    {
        char *tail = strstr(line, "\n");
        if (tail) *tail = '\0';
        if (strlen(line) == 0)
            continue;

        IF_INDEX_T interface = { };
        char *identifier    SAFE_ALLOCA(identifier, strlen(line)+1);
        if (parseLine(line, &interface, identifier) != 0)
        {
            printf("[%04d]parse \"%s\" fail!\n", __LINE__, line);
            continue;
        }

        if (licenseDecode(identifier, strlen(line)+1) != 0)
        {
            printf("[%04d]decode \"%s\" fail!\n", __LINE__, line);
            continue;
        }
        if (parseIdentifier(identifier))
        {
            continue;
        }

        char license[LICENSE_LEN] = { 0 };
        if (licenseGen(identifier, license, sizeof(license)))
        {
            printf("[%04d]generate \"%s\" fail!\n", __LINE__, line);
            continue;
        }

#define LICENSE_FILE    "./license.slot"
        FILE *fp1 = NULL;
        char filePath[128];
        snprintf(filePath, sizeof(filePath), "%s%d", LICENSE_FILE, interface.slot);
        if (slot == -1 || slot != interface.slot)
        {
            isFirst = 1;
            slot = interface.slot;
        }
        if (isFirst)
        {
            isFirst = 0;
            if (!access(filePath, F_OK))
            {
                char cmd[128] = { 0 };
                snprintf(cmd, sizeof(cmd), "rm -rf %s", filePath);
                system(cmd);
            }
        }
        if ((fp1 = fopen(filePath, "a+")) == NULL)
        {
            printf("[%04d]open %s failed, error: %s!\n", __LINE__, filePath, strerror(errno));
            continue;
        }


        do {
            char content[512] = { 0 };

            interface.iline ?
            snprintf(content+strlen(content), sizeof(content)-strlen(content), "port%d/%d.%d: ",    interface.pos, interface.port, interface.line) :
            snprintf(content+strlen(content), sizeof(content)-strlen(content), "port%d/%d: ",       interface.pos, interface.port);

            snprintf(content+strlen(content), sizeof(content)-strlen(content), "%s",                license);

            fprintf(fp1, "%s\n", content);
        } while (0);

        fclose(fp1);
        sync();
    }
    return;
}

static const char codeMap[][16] =
{
    { '7', '5', 'e', '3', '0', 'c', '4', 'd', '9', 'b', '8', 'f', 'a', '1', '2', '6' },
    { 'a', '9', '6', '8', '7', 'd', '0', '5', 'e', '3', 'c', '1', 'b', '2', 'f', '4' },
    { '9', '7', '2', 'b', '3', '1', 'e', 'a', '0', 'd', 'f', '4', 'c', '6', '8', '5' },
};

UINT8 asc_to_hex(UINT8 ch)
{
    if( ch >= '0' && ch <= '9')
        ch -= '0';
    else if( ch >= 'A' && ch <= 'Z' )
        ch -= 0x37;
    else if( ch >= 'a' && ch <= 'z' )
        ch -= 0x57;
    else ch = 0xff;
    return ch;
}
static int licenseEncode(char *s, UINT32 len)
{
    if (!s)
    {
        return -1;
    }

    UINT32 i = 0;

    char *tmpS  SAFE_ALLOCA(tmpS, len);
    while (s[i] != '\0')
    {
        if (s[i] != '-')
        {
            UINT32 idx = 0;
            idx = asc_to_hex(s[i]);
            tmpS[i] = codeMap[i%3][idx];
        }
        else
        {
            tmpS[i] = s[i];
        }

        i++;
    }
    strncpy(s, tmpS, len-1);

    return 0;
}
static int licenseDecode(char *s, UINT32 len)
{
    if (!s)
    {
        return -1;
    }

    UINT32 i = 0;

    char *tmpS  SAFE_ALLOCA(tmpS, len);
    while (s[i] != '\0')
    {
        if (s[i] != '-')
        {
            UINT32 idx = 0;
            for (idx = 0; idx < 16; idx++)
            {
                if (codeMap[i%3][idx] == s[i]) break;
            }
            if (idx >= 16)
            {
                return -1;
            }
            snprintf(tmpS+strlen(tmpS), len-strlen(tmpS), "%x", idx);
        }
        else
        {
            tmpS[i] = s[i];
        }

        i++;
    }
    strncpy(s, tmpS, len-1);

    return 0;
}

#define LICENSE_FILE_TMP    "./.license.tmp"
static int md5(char *in, char *out, UINT32 len)
{
    int ret = 0;
    if (!in || !out)
        return -1;

    FILE *fp = NULL;
    if ((fp = fopen(LICENSE_FILE_TMP, "w")) == NULL)
    {
        printf("[%04d]open %s failed, error: %s!\n", __LINE__, LICENSE_FILE_TMP, strerror(errno));
        return -1;
    }

    fprintf(fp, "%s", in);
    truncate(LICENSE_FILE_TMP, ftell(fp));
    fclose(fp);

    char cmd[512] = { 0 };
    snprintf(cmd, sizeof(cmd), "md5sum %s", LICENSE_FILE_TMP);
    if ((fp = popen(cmd, "r")) == NULL)
    {
        printf("[%04d]popen command(%s) failed, error: %s!\n", __LINE__, cmd, strerror(errno));
        return -1;
    }

    char line[512] = { 0 }, buf[LICENSE_LEN] = { 0 };
    if (fgets(line, sizeof(line), fp))
    {
        sscanf(line, "%s", buf);
        strncpy(out, buf, len-1);
    }
    else
    {
        printf("[%04d]fgets failed, error: %s!\n", __LINE__, strerror(errno));
        ret = -1;
    }

    pclose(fp);

    snprintf(cmd, sizeof(cmd), "rm -rf %s", LICENSE_FILE_TMP);
    system(cmd);

    return ret;
}

static int licenseGen(char *identifier, char *license, UINT32 len)
{
    int ret = 0;

#define INPUTCODE_LEN       (sizeof(identifier) + 4 + 100)
#define LICENSE_EXPAND_KEY  "0000"
#define LICENSE_PRIV_KEY    "002145"
    char *inputCode     SAFE_ALLOCA(inputCode, INPUTCODE_LEN);
    snprintf(inputCode, INPUTCODE_LEN, "%s%s%s", identifier, LICENSE_EXPAND_KEY, LICENSE_PRIV_KEY);

    ret = md5(inputCode, license, len);

    return ret;
}
