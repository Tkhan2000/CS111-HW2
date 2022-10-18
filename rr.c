#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process {
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  u32 remaining_time;
  u32 waiting_time;
  u32 response_time;
  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end) {
  u32 current = 0;
  bool started = false;
  while (*data != data_end) {
    char c = **data;

    if (c < 0x30 || c > 0x39) {
      if (started) {
	return current;
      }
    }
    else {
      if (!started) {
	current = (c - 0x30);
	started = true;
      }
      else {
	current *= 10;
	current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data) {
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++])) {
    if (c < 0x30 || c > 0x39) {
      exit(EINVAL);
    }
    if (!started) {
      current = (c - 0x30);
      started = true;
    }
    else {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED) {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;
  

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL) {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i) {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }
  
  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */
  struct process *current_process;
  u32 curTime = 0;
  bool finish = false; // While loop condition
  int totalTime = 0; // Stores sum of burst times
  int arriveTimes[size]; // Stores arrival times

  // Run through each process and initiallize parameters
  for (u32 i=0; i<size; i++) 
  {
    current_process = &data[i];
    totalTime += current_process->burst_time;
    //Initialize process variables and arriveTimes array
    current_process->remaining_time = current_process->burst_time;
    current_process->pid = i;
    current_process->response_time = 0;
    current_process->waiting_time = 0; 
    arriveTimes[i] = current_process->arrival_time;
    //Initiallize head with the process with arrival time 0
    if (current_process->arrival_time == 0)
      TAILQ_INSERT_HEAD(&list, &data[0], pointers);
    
  }
  //Loop until all processes are finished
  while (!finish)
  {
    current_process = TAILQ_FIRST(&list);
    u32 remTime = current_process->remaining_time;
    // Make length variable which is min(quantum, remTIme)
    u32 length;
    if (quantum_length > remTime)
      length = remTime;
    else
      length = quantum_length;
    
    for (int i = 0; i < length; i++)
    {
      //Check if process has just started by comparing remaining and burst times
      if (current_process->remaining_time == current_process->burst_time)
        current_process->response_time = curTime - current_process->arrival_time;

      curTime++;
      current_process->remaining_time--;

      struct process *node;
      TAILQ_FOREACH(node, &list, pointers) //Update waiting times of other nodes
      {
        if (current_process->pid != node->pid)
          node->waiting_time++;
      }

      for (int j = 0; j < size; j++) //Check if any processes are arriving
      {
        struct process *proc = &data[j];
        if (arriveTimes[j] == curTime)
          TAILQ_INSERT_TAIL(&list, proc, pointers);
      }
    }
    TAILQ_REMOVE(&list, current_process, pointers); //Remove head
    if (current_process->remaining_time > 0) //Add process back to queue if unfinished
      TAILQ_INSERT_TAIL(&list, current_process, pointers); 
    if (curTime >= totalTime) finish = true;
  }
  // Add up response and waiting times
  for (u32 i=0; i<size; i++)
  {
    current_process = &data[i];
    total_response_time += current_process->response_time;
    total_waiting_time += current_process->waiting_time;
  }

  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float) total_waiting_time / (float) size);
  printf("Average response time: %.2f\n", (float) total_response_time / (float) size);

  free(data);
  return 0;
}

/*
1, 0, 7
2, 2, 4
3, 4, 1
4, 5, 4

[1,1,1,2,2,2,1,1,1,3,4,4,4,2,1,4] 
*/