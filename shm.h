
extern int shmfd;
extern long shmid;
extern void *shmptr;
extern unsigned long long shmlen;
extern mode_t shmmode;
extern unsigned long long shmoffset;
extern int shmflags;

extern void dump_shm(void);
extern void attach_shared(char *name);
extern void attach_sysvshm(char *name); 
extern void verify_shm(int policy, nodemask_t nodes);
