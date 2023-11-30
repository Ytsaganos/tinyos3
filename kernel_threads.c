
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  if(task != NULL){
    PCB* pcb = CURPROC;
    TCB* tcb = spawn_thread(pcb, start_ptcb_main_thread);

    spawn_ptcb(tcb, task, argl, args);

    pcb -> thread_count++;
    wakeup (tcb);
	 return (Tid_t) tcb->ptcb;
  }
else

  return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}



/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{

  PCB* curproc = CURPROC;  // Get current process.


  rlnode* ptcb_node = rlist_find(& curproc->ptcb_list, (PTCB *) tid, NULL); // Find ptcb's rlnode in curproc's ptcb list.


  if(ptcb_node == NULL) 
      return -1;


  PTCB* ptcb = ptcb_node->ptcb; // Get the ptcb from its rlnode.


  if(ptcb == NULL)
      return -1;

    
  if(tid == sys_ThreadSelf()) // Check that the current thread is not trying to join itself. 
      return -1;


  if(ptcb->detached == 1) // Check that the thread we are trying to join is not detached.
      return -1;

  
  ptcb->refcount ++; //Increment the refcounter.


  while(ptcb->exited != 1 && ptcb->detached != 1) 
      kernel_wait(& ptcb->exit_cv, SCHED_USER);  //  Wait for the thread to exit.


  ptcb->refcount --;  //Decrement the refcounter.

  
  if(ptcb->detached) //Join failed if the thread was detached while running.
    return -1;


  if(exitval != NULL)
      *exitval = ptcb->exitval; // Passes the exit value of the joined thread at the location stored in the exitval pointer.
  

  if(ptcb->refcount == 0){   //  If no other thread waits for the joined thread to finish:
      rlist_remove(&ptcb->ptcb_list_node); // Remove the ptcb node from the intrusive list.
      free(ptcb);           //  Free the ptcb's allocated memory.
  }

  return 0; 

}



/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  PCB* pcb = CURPROC;
  PTCB* ptcb = (PTCB*) tid;

  if(rlist_find(&(pcb-> ptcb_list),ptcb,NULL) == NULL || ptcb-> exited == 1)
  {
    return -1;
  }
  
  ptcb -> detached = 1;
   
  kernel_broadcast(&(ptcb -> exit_cv));

  return 0;

}





/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

 //euresi tou current ptcb meso tou current tcb
  PTCB* ptcb = cur_thread()->ptcb;

  
//initialize ton exit metabliton tou ptcb
  ptcb->exited = 1;
  ptcb->exitval = exitval;
  kernel_broadcast(&(ptcb->exit_cv));//broadcast sto exit_cv gia na ksipnisoun osoi perimenoun
  //elegxos threadcount kai refcount
  PCB *curproc = CURPROC;  /* cache for efficiency */
  curproc->thread_count--;

  
//an thread count == 0 katharizo kai enimerono to PTCB opos tin sys_exit
  if(CURPROC->thread_count == 0){
  

       /* Reparent any children of the exiting process to the 
     initial task */
    if(get_pid(curproc) != 1){
      PCB* initpcb = get_pcb(1);
      while(!is_rlist_empty(& curproc->children_list)) {
        rlnode* child = rlist_pop_front(& curproc->children_list);
        child->pcb->parent = initpcb;
        rlist_push_front(& initpcb->children_list, child);
      }

    /* Add exited children to the initial task's exited list 
     and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    if(curproc->parent != NULL) {   /* Maybe this is init */
      rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(& curproc->parent->child_exit);
    }
   }

  /* Do all the other cleanup we want here, close files etc. */

   while(is_rlist_empty(&curproc->ptcb_list) != 0){
    rlnode* ptcb_temp;
    ptcb_temp = rlist_pop_front(&curproc->ptcb_list);
    free(ptcb_temp->ptcb);

   }
    if(curproc->args) {
      free(curproc->args);
      curproc->args = NULL;
    }

    /* Clean up FIDT */
    for(int i=0;i<MAX_FILEID;i++) {
      if(curproc->FIDT[i] != NULL) {
        FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }
    
  
  /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;
  }

  kernel_sleep(EXITED,SCHED_USER);//as paei to thread gia nani
 
}


