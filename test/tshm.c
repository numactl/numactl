#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <numaif.h>

#define err(x) perror(x),exit(1)

enum { 
	MEMSZ = 10*1024*1024,
}; 

struct req {
	enum cmd { 
		SET = 1,
		CHECK, 
		REPLY,
		EXIT,
	} cmd;
	long offset;
	long len;
	int policy;
	nodemask_t nodes;
}; 

void worker(void) 
{ 
	struct req req;
	while (read(0, &req, sizeof(struct req) > 0)) { 
		switch (req.cmd) { 
		case SET:
			if (mbind(map + req.offset, req.len, req.policy, &req.nodes,
				  NUMA_MAX_NODES+1, 0) < 0) 
				err("mbind");
			break;
		case TEST:
			req.cmd = REPLY;
			if (get_mempolicy(&req.policy, &req.nodes, NUMA_MAX_NODES+1,
					  map + req.offset, MPOL_F_ADDR) < 0)
				err("get_mempolicy");
			write(1, &req, sizeof(struct req)); 
			break;
		case EXIT:
			return;
		default:
			abort();
		}
	}
} 

void sendreq(int fd, enum cmd cmd, int policy, long offset, long len, nodemask_t nodes) 
{ 
	struct req req = { 
		.cmd = cmd, 
		.offset = offset, 
		.len = len, 
		.policy = policy,
		.nodes = nodes
	}; 
	if (write(fd, &req, sizeof(struct req)) != sizeof(struct req))
		panic("bad req write");
} 

void readreq(int fd, int *policy, nodemask_t *nodes, long offset, long len)
{
	struct req req;
	if (read(fd, &req, sizeof(struct req)) != sizeof(struct req))
		panic("bad req read");
	if (req.cmd != REPLY)
		abort();
	*policy = req.policy;
	*nodes = req.nodes;
}

int main(void)
{
	int fd = open("tshm", O_CREAT, 0600);   
	close(fd);
	key_t key = ftok("tshm", 1); 
	int shm = shmget(key, MEMSZ,  IPC_CREAT|0600);
	if (shm < 0) err("shmget");
	char *map = shmat(shm, NULL, 0); 
	printf("map = %p\n", map); 

	unsigned long nmask = 0x3;
	if (mbind(map, MEMSZ, MPOL_INTERLEAVE, &nmask, 4, 0) < 0) err("mbind1"); 

	int fd[2];
	if (pipe(fd) < 0) err("pipe"); 
	if (fork() == 0) {
		close(0); 
		close(1); 
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		worker();
		_exit(0);
	} 
	       
	int pagesz = getpagesize();
	int i;

	srand(1); 
	for (;;) { 

		/* chose random offset */

		/* either in child or here */

		/* change policy */

		/* ask other guy to check */ 

	} 
	
  
	shmdt(map);
	shmctl(shm, IPC_RMID, 0);
} 


