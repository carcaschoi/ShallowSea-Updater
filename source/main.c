#include <stdio.h>
#include <stdarg.h>
#include <unistd.h> // chdir
#include <dirent.h> // mkdir
#include <switch.h>
#include <string.h> // copyfile
#include <stdlib.h> //libnx ask me to add it
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h> 
#include <netinet/in.h>
//#include <jansson.h>

#include "download.h"
#include "unzip.h"
#include "main.h"
#include "option.h"
#include "reboot_payload.h"


#define ROOT                    "sdmc:/"
#define APP_PATH                "sdmc:/switch/ShallowSea-toolbox/"
#define APP_OUTPUT              "sdmc:/switch/ShallowSea-toolbox/ShallowSea-toolbox.nro"
#define OLD_APP_PATH            "sdmc:/switch/ShallowSea-updater/ShallowSea-updater.nro"
#define AMS                     "sdmc:/updating/"
//#define wait(msec) svcSleepThread(10000000 * (s64)msec)

#define APP_VERSION             "1.1.6"
#define CURSOR_LIST_MAX         3

const char *OPTION_LIST[] =
{
    "= Update ShallowSea-ams",
	"= Update English-extra-package",
    "= Update this app",
	"= Reboot to payload",
	
};

void refreshScreen(int cursor)
{
    consoleClear();

    printf("\x1B[36mShallowSea-Toolbox by carcaschoi: v%s\x1B[37m\n\n\n", APP_VERSION);
	printf("Github: https://github.com/carcaschoi/ShallowSea-Toolbox/\n\n");
	printf("This app supports both erista and mariko\n\n\n");
    printf("Press (A) to select option\n\n\n\n");
    printf("Press (+) to exit\n\n\n\n\n");

    for (int i = 0; i < CURSOR_LIST_MAX + 1; i++)
        printf("[%c] %s\n\n", cursor == i ? 'X' : ' ', OPTION_LIST[i]);

    consoleUpdate(NULL);
}

void printDisplay(const char *text, ...)
{
    va_list v;
    va_start(v, text);
    vfprintf(stdout, text, v);
    va_end(v);
    consoleUpdate(NULL);
}

int appInit()
{
    consoleInit(NULL);
    socketInitializeDefault();
    return 0;
}

void appExit()
{
    socketExit();
    consoleExit(NULL);
	appletSetAutoSleepDisabled(false);
	bpcExit();
	appletUnlockExit();
}

// tell the app what copyFile does
void copyFile(char *src, char *dest)
{
    FILE *srcfile = fopen(src, "rb");
    FILE *newfile = fopen(dest, "wb");

    if (srcfile && newfile)
    {
        char buffer[10000]; // 10kb per write, which is fast
        size_t bytes;       // size of the file to write (10kb or filesize max)

        while (0 < (bytes = fread(buffer, 1, sizeof(buffer), srcfile)))
        {
            fwrite(buffer, 1, bytes, newfile);
        }
    }
    fclose(srcfile);
    fclose(newfile);
}
// finished
int parseSearch(char *parse_string, char *filter, char *new_string)
{
    FILE *fp = fopen(parse_string, "r");

    if (fp)
    {
        char c;
        while ((c = fgetc(fp)) != EOF)
        {
            if (c == *filter)
            {
                for (int i = 0, len = strlen(filter) - 1; c == filter[i]; i++)
                {
                    c = fgetc(fp);
                    if (i == len)
                    {
                        for (int j = 0; c != '\"'; j++)
                        {
                            new_string[j] = c;
                            new_string[j + 1] = '\0';
                            c = fgetc(fp);
                        }
                        fclose(fp);
                        remove(parse_string);
                        return 0;
                    }
                }
            }
        }
    }
    fclose(fp);
    return 1;
}

// Check if it's dir on 'path'
int is_dir(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

// Remove all the shit. Please note that there should be no '/' char in the end. Otherwise update this snippet.
int remove_entry(const char *dir_name)
{
    if (!is_dir(dir_name))
        return unlink(dir_name);

    DIR *dir = opendir(dir_name);

    if (dir == NULL)
        return 1;

    size_t dSz = strlen(dir_name);
    struct dirent *s_dirent;
    char *full_name;

    while ((s_dirent = readdir(dir)) != NULL)
    {
        if ((strcmp(s_dirent->d_name, ".") == 0) || (strcmp(s_dirent->d_name, "..") == 0))
            continue;
        full_name = malloc(dSz + strlen(s_dirent->d_name) + 2); // '/'+'\0'

        strcpy(full_name, dir_name);
        strcat(full_name, "/");
        strcat(full_name, s_dirent->d_name);

        if (s_dirent->d_type == DT_DIR)
            remove_entry(full_name); // NOTE: Handle returning value
        else
            unlink(full_name); // NOTE: Add validation

        free(full_name);
    }

    closedir(dir);

    return rmdir(dir_name); // NOTE: Add validation
}

int main(int argc, char **argv)
{
    // init stuff
    appInit();
    mkdir(APP_PATH, 0777);
    
    // change directory to root (defaults to /switch/)
    chdir(ROOT);

    // set the cursor position to 0
    short cursor = 0;

    // main menu
    refreshScreen(cursor);
	
	padConfigureInput(8, HidNpadStyleSet_NpadStandard);
	PadState pad;
    padInitializeAny(&pad);
	
    apmInitialize();
	appletSetAutoSleepDisabled(true);
	appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
	appletSetAutoSleepTimeAndDimmingTimeEnabled(false);
	appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
	bpcInitialize();

    // muh loooooop
    while(appletMainLoop())
    {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        // move cursor down...
        if (kDown & HidNpadButton_Down)
        {
            if (cursor == CURSOR_LIST_MAX) cursor = 0;
            else cursor++;
            refreshScreen(cursor);
        }

        // move cursor up...
        if (kDown & HidNpadButton_Up)
        {
            if (cursor == 0) cursor = CURSOR_LIST_MAX;
            else cursor--;
            refreshScreen(cursor);
        }

        if (kDown & HidNpadButton_A)
        {   
	        remove(TEMP_FILE);
            switch (cursor)
            {
            case UP_AMS:
                if (downloadFile(AMS_URL, TEMP_FILE, OFF))
		        {
					appletLockExit ();
					remove_entry(AMS);
					mkdir(AMS, 0777);
				    chdir(AMS);
                    unzip(TEMP_FILE);
					chdir(ROOT);
					romfsInit();
					copyFile("/updating/config/ShallowSea-updater/startup.te", "/startup.te");
                    copyFile("romfs:/payload.bin", "/atmosphere/reboot_payload.bin");
                    copyFile("romfs:/payload.bin", "/payload.bin");
                    copyFile("romfs:/boot.dat", "/boot.dat");
                    copyFile("romfs:/boot.ini", "/boot.ini");
					copyFile("/updating/config/ShallowSea-updater/hekate_ipl.ini", "/bootloader/hekate_ipl.ini");
					//rename("/NSP/", "/helloworld/");
					consoleClear();
					printDisplay("Finished download and extract ShallowSea-ams\n\nNow reboot to tegraexplorer to finish the final step.");
					svcSleepThread(5000000000ULL);
					reboot_payload("romfs:/payload.bin");
					romfsExit();
			    }
                else
                {
                    printDisplay("Failed to download ShallowSea-ams\n");
                }
                break;
				
			case UP_ENG:
                if (downloadFile(ENG_URL, TEMP_FILE, OFF))
		        {
					appletLockExit ();
                    unzip(TEMP_FILE);
					consoleClear();
					printDisplay("Finished download and extract English_extra_package\n\nNow reboot the console");
					svcSleepThread(5000000000ULL);
					romfsInit();
					reboot_payload("romfs:/payload.bin");
					romfsExit();
			    }
                else
                {
                    printDisplay("Failed to download English-extra-package\n");
                }
                break;

            case UP_APP:
                if (downloadFile(APP_URL, TEMP_FILE, OFF))
                {
					romfsExit();
                    remove(APP_OUTPUT);
                    rename(TEMP_FILE, APP_OUTPUT);
                    remove(OLD_APP_PATH);
		            printDisplay("Please reopen the app");
					svcSleepThread(2000000000ULL);
			        //exit the app
			        appExit();
			        return 0;
                }
                else
                {
                    printDisplay("Failed to download app update\n");
                }
                break;
				
            case REBOOT:
                {
					romfsInit();
					reboot_payload("romfs:/payload.bin");
					svcSleepThread(5000000000ULL);
					romfsExit();
                    printDisplay("Failed to reboot console\n");
                }
                break;
            }
        }
        
        // exit...
        if (kDown & HidNpadButton_Plus) break;
    }

    // cleanup then exit
    appExit();
    return 0;
}
