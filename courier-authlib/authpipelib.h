#ifndef PIPE_H
#define PIPE_H PIPE_H

/* no headers needed */

/* forks the pipe-program fills the 2 filed
   returns 0 for success, 1 for error */
int getPipe(int *dataIn, int *dataOut);

/* closes fds */
void closePipe(void);

#endif

