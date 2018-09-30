#ifndef _FAT_H_
#define _FAT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SD		1
#define USB		2

#define DEV_MOUNT  "Fat"

u32 FileLength;

/* 'FAT Device' structure */
typedef struct {
    /* Device mount point */
    char *mount;

    /* Device name */
    char *name;

    /* Device interface */
    const DISC_INTERFACE *interface;
} fatDevice;

//extern fatDevice fdevList[];
fatDevice *fdev;
//extern const int NB_FAT_DEVICES;

/* Prototypes */
u32 Fat_Mount(int);
s32 Fat_Unmount(int);
s32 Fat_ReadFile(const char *, void **);
int Fat_MakeDir(const char *);
bool Fat_CheckFile(const char *);
s32 Fat_SaveFile(const char *, void **, u32);
s32 Fat_Mountsmb(fatDevice *);
void Fat_Unmountsmb(fatDevice *);
#ifdef __cplusplus
}
#endif

#endif
